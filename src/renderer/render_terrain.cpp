#include "render_terrain.hpp"
#include "render_helpers.hpp"
#include "render_textures.hpp"
#include "../core/const.hpp"
#include "../core/settings.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

/**********************************************************************
RENDER TERRAIN
Responsibilities
- Wireframe terrain mesh construction + draw (height-colored, brush overlay)
- Textured terrain surface construction + draw (bucketed by surface type)
**********************************************************************/

void editor_renderer_build_terrain_mesh(EditorRenderer& er, const HeightField& hf){
    if (hf.rows < 2 || hf.cols < 2) return;

    // wireframe grid that follows the heightfield surface
    // one quad per cell, drawn as GL_LINES
    // color encodes height 
    // low=dark blue, high=bright green, gives instant readability
    std::vector<float> lines;
    lines.reserve(hf.rows * hf.cols * 24); // rough upper bound

    float max_h = *std::max_element(hf.heights.begin(), hf.heights.end());
    float min_h = *std::min_element(hf.heights.begin(), hf.heights.end());
    float range = (max_h - min_h) < 0.01f ? 1.0f : (max_h - min_h);

    auto get_pos = [&](int r, int c) -> glm::vec3 {
        float x = hf.origin.x + c * hf.cell_size;
        float z = hf.origin.z + r * hf.cell_size;
        float y = hf.heights[r * hf.cols + c];
        return glm::vec3(x, y, z);
    };

    auto height_color = [&](float h) -> glm::vec3 {
        float t = (h - min_h) / range; // 0=low, 1=high
        // low = dark teal, high = bright lime
        return glm::mix(glm::vec3(0.10f, 0.35f, 0.45f), glm::vec3(0.30f, 0.90f, 0.25f), t);
    };

    auto push_line = [&](glm::vec3 a, glm::vec3 b){
        glm::vec3 ca = height_color(a.y);
        glm::vec3 cb = height_color(b.y);
        lines.insert(lines.end(), { a.x,a.y,a.z, ca.r,ca.g,ca.b });
        lines.insert(lines.end(), { b.x,b.y,b.z, cb.r,cb.g,cb.b });
    };

    for (int r = 0; r < hf.rows; r++){
        for (int c = 0; c < hf.cols; c++){
            glm::vec3 p = get_pos(r, c);
            // horizontal edge (along X)
            if (c < hf.cols - 1) push_line(p, get_pos(r, c + 1));
            // vertical edge (along Z)
            if (r < hf.rows - 1) push_line(p, get_pos(r + 1, c));
        }
    }

    if (er.terrain_mesh.vao){
        mesh_destroy(er.terrain_mesh);
        er.terrain_mesh.vao = 0;
        er.terrain_mesh.vbo = 0;
        er.terrain_mesh.count = 0;
    }
    mesh_init(er.terrain_mesh, lines);
    er.terrain_mesh_dirty = false;
}

void editor_renderer_draw_terrain(EditorRenderer& er, const HeightField& hf,
    const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& brush_pos, float brush_radius, bool placement_valid){

    if (er.terrain_mesh_dirty)
        editor_renderer_build_terrain_mesh(er, hf);

    shader_bind(er.shader);
    set_mat4(er.shader, "u_model", glm::mat4(1.0f));
    set_mat4(er.shader, "u_view",  view);
    set_mat4(er.shader, "u_proj",  proj);
    glBindVertexArray(er.terrain_mesh.vao);
    glDrawArrays(GL_LINES, 0, er.terrain_mesh.count);
    glBindVertexArray(0);

    if (!placement_valid) return;
    static const int SEGS = 48;
    er.line_verts.clear();
    er.line_verts.reserve(SEGS * 12);
    for (int i = 0; i < SEGS; i++){
        float a0 = (float)i       / SEGS * 2.0f * 3.14159265f;
        float a1 = (float)(i + 1) / SEGS * 2.0f * 3.14159265f;
        float x0 = brush_pos.x + std::cos(a0) * brush_radius;
        float z0 = brush_pos.z + std::sin(a0) * brush_radius;
        float x1 = brush_pos.x + std::cos(a1) * brush_radius;
        float z1 = brush_pos.z + std::sin(a1) * brush_radius;
        float y0 = heightfield_sample(hf, x0, z0) + 0.1f;
        float y1 = heightfield_sample(hf, x1, z1) + 0.1f;
        er.line_verts.insert(er.line_verts.end(), { x0,y0,z0, 1.0f,1.0f,0.0f });
        er.line_verts.insert(er.line_verts.end(), { x1,y1,z1, 1.0f,1.0f,0.0f });
    }
    flush_line_batch(er, er.shader, view, proj);
}

void editor_renderer_build_terrain_surface(EditorRenderer& er, const HeightField& hf, const Ocean& ocean){
    if (hf.rows < 2 || hf.cols < 2) return;

    // flat colors per surface type used when texture is missing
    static const glm::vec3 SURF_COLORS[(int)SURFACE_COUNT] = {
        {0.00f, 0.00f, 0.00f}, // none
        {0.20f, 0.20f, 0.20f}, // asphalt
        {0.55f, 0.48f, 0.38f}, // gravel
        {0.45f, 0.32f, 0.18f}, // dirt
        {0.85f, 0.78f, 0.55f}, // sand
        {0.25f, 0.55f, 0.18f}, // grass
        {0.70f, 0.70f, 0.68f}, // cement
        {0.55f, 0.52f, 0.48f}, // rock
    };

    // one sub-mesh per surface type so we can batch by texture
    // each bucket holds interleaved pos+normal+uv floats
    std::vector<float> buckets[(int)SURFACE_COUNT];

    auto get_pos = [&](int r, int c) -> glm::vec3 {
        return glm::vec3(
            hf.origin.x + c * hf.cell_size,
            hf.heights[r * hf.cols + c],
            hf.origin.z + r * hf.cell_size);
    };

    // returns the dominant surface type for a quad cell
    // uses top-left corner cell value — paint brush writes all covered cells
    auto cell_surface = [&](int r, int c) -> int {
        return (int)hf.surface[r * hf.cols + c];
    };
    for (int r = 0; r < hf.rows - 1; r++){
        for (int c = 0; c < hf.cols - 1; c++){
            glm::vec3 p00 = get_pos(r, c);
            glm::vec3 p10 = get_pos(r+1, c);
            glm::vec3 p01 = get_pos(r,c+1);
            glm::vec3 p11 = get_pos(r+1, c+1);

            int si = cell_surface(r, c);
            auto& bucket = buckets[si];

            /*float u0 = (float)c, v0 = (float)r;
            float u1 = (float)(c+1), v1 = (float)(r+1);*/

            float tex_scale = 1.0f / 4.0f; // world units per texture repeat
            float u0 = p00.x * tex_scale, v0 = p00.z * tex_scale;
            float u1 = p11.x * tex_scale, v1 = p11.z * tex_scale;

            // smooth per-vertex normals from the heightfield instead of flat
            // per-triangle normals - flat normals is what caused the visible
            // diagonal seam down the middle of every quad (each tri lighting
            // independently), giving that hard "faceted grid" look
            glm::vec3 n00 = heightfield_normal(hf, p00.x, p00.z);
            glm::vec3 n10 = heightfield_normal(hf, p10.x, p10.z);
            glm::vec3 n01 = heightfield_normal(hf, p01.x, p01.z);
            glm::vec3 n11 = heightfield_normal(hf, p11.x, p11.z);

            auto push = [&](glm::vec3 p, glm::vec3 n_, glm::vec2 uv){
                bucket.insert(bucket.end(), {
                    p.x, p.y, p.z, n_.x, n_.y, n_.z, uv.x, uv.y
                });
            };

            push(p00, n00, {u0,v0});
            push(p10, n10, {u0,v1});
            push(p01, n01, {u1,v0});

            push(p10, n10, {u0,v1});
            push(p11, n11, {u1,v1});
            push(p01, n01, {u1,v0});
        }
    }

    // rebuild per-surface VAOs stored in er.terrain_surface_mesh
    // we reuse the single Mesh slot for the first bucket and store extras inline
    // simplest approach: pack all buckets into one mesh, draw in surface-type passes
    // store bucket byte offsets so draw call can glDrawArrays with offset+count
    if (er.terrain_surface_mesh.vao){
        mesh_destroy(er.terrain_surface_mesh);
        er.terrain_surface_mesh.vao = 0;
        er.terrain_surface_mesh.vbo = 0;
        er.terrain_surface_mesh.count = 0;
    }

    // flatten all buckets into one buffer, record offsets
    std::vector<float> combined;
    er.terrain_surface_offsets.clear();
    er.terrain_surface_counts.clear();

    for (int i = 0; i < (int)SURFACE_COUNT; i++){
        int vert_count = (int)buckets[i].size() / 8; // 8 floats per vertex
        er.terrain_surface_offsets.push_back((int)combined.size() / 8);
        er.terrain_surface_counts.push_back(vert_count);
        combined.insert(combined.end(), buckets[i].begin(), buckets[i].end());
    }

    if (!combined.empty()){
        int vert_count = (int)combined.size() / 8;

        glGenVertexArrays(1, &er.terrain_surface_mesh.vao);
        glGenBuffers(1, &er.terrain_surface_mesh.vbo);
        glBindVertexArray(er.terrain_surface_mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, er.terrain_surface_mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,
            combined.size() * sizeof(float), combined.data(), GL_STATIC_DRAW);

        // pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        // uv
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        er.terrain_surface_mesh.count = vert_count;
    }

    er.terrain_surface_dirty = false;
}

void editor_renderer_draw_terrain_surface(EditorRenderer& er, const HeightField& hf,
    const glm::mat4& view, const glm::mat4& proj, const Ocean& ocean){
    if (er.terrain_surface_dirty)
        editor_renderer_build_terrain_surface(er, hf, ocean);

    if (!er.terrain_surface_mesh.vao) return;
    glm::vec3 LIGHT_DIR = glm::normalize(er.sun_dir);
    static const glm::vec3 SURF_COLORS[(int)SURFACE_COUNT] = {
        {0.00f, 0.00f, 0.00f}, // none
        {0.20f, 0.20f, 0.20f}, // asphalt
        {0.55f, 0.48f, 0.38f}, // gravel
        {0.45f, 0.32f, 0.18f}, // dirt
        {0.85f, 0.78f, 0.55f}, // sand
        {0.25f, 0.55f, 0.18f}, // grass
        {0.70f, 0.70f, 0.68f}, // cement
        {0.55f, 0.52f, 0.48f}, // rock
    };

    /*
    SURFACE PAINT TEX NAMES ADD _COLOR.JPEG FILES IN ASSETS/
    AND CHANGE IT BASED ON NAME DEFS HERE
    */
    static const char* SURF_TEX_NAMES[(int)SURFACE_COUNT] = {
        "",
        "asphalt.jpg", 
        "gravel.jpg", 
        "dirt.jpg",
        "sand.jpg",    
        "grass.jpg",
        "cement.jpg",
        "rock.jpg",
    };

     auto& RL = er.road_loc;
    glm::mat4 identity = glm::mat4(1.0f);
    glm::mat3 nm = glm::mat3(1.0f);
    shader_bind(er.road_shader);
    glUniformMatrix4fv(RL.model, 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(RL.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(RL.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix3fv(RL.normal_mat, 1, GL_FALSE, glm::value_ptr(nm));
    glUniform3f(RL.light_dir, LIGHT_DIR.x, LIGHT_DIR.y, LIGHT_DIR.z);
    glUniformMatrix4fv(RL.light_space, 1, GL_FALSE, glm::value_ptr(er.light_space_mat));
    glUniform1f(RL.shadow_bias, Const::SHADOW_BIAS);
    glUniform1f(RL.ambient, er.ambient);
    glUniform1f(RL.diff_intensity, er.diff_intensity);
    glUniform3f(RL.light_color, er.light_color.r, er.light_color.g, er.light_color.b);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, er.shadow_depth_tex);
    glUniform1i(RL.shadow_map, 1);
    glActiveTexture(GL_TEXTURE0);

    glm::vec3 cam_pos_t = glm::vec3(glm::inverse(view)[3]);
    glUniform3f(RL.fog_color, er.fog_color.r,  er.fog_color.g,  er.fog_color.b);
    glUniform1f(RL.fog_near, my_settings.render_fog ? er.fog_near : Const::CAM_FAR);
    glUniform1f(RL.fog_far,  my_settings.render_fog ? er.fog_far : Const::CAM_FAR + 1.0f);
    glUniform3f(RL.fog_cam_pos, cam_pos_t.x, cam_pos_t.y, cam_pos_t.z);
    upload_point_lights(er.road_shader.id, er.last_lights, cam_pos_t, er.night_factor);

    glBindVertexArray(er.terrain_surface_mesh.vao);

    for (int i = 0; i < (int)SURFACE_COUNT; i++){
        if (er.terrain_surface_counts[i] == 0) continue;
        if (i == (int)SURFACE_NONE) continue;

        std::string tex_path = std::string("../assets/props/") + SURF_TEX_NAMES[i];
        GLuint tex = load_texture(er, tex_path);
        if (tex){
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(RL.tex, 0);
            glUniform1i(RL.use_texture, 1);
        }
        else {
            glUniform1i(RL.use_texture, 0);
            glm::vec3 kd = SURF_COLORS[i];
            glUniform3f(RL.kd, kd.r, kd.g, kd.b);
        }

        glDrawArrays(GL_TRIANGLES,
            er.terrain_surface_offsets[i],
            er.terrain_surface_counts[i]);

        if (tex) glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(0);
}