#include "render_init.hpp"
#include "render_textures.hpp"
#include "../core/const.hpp"
#include <cstdio>

/**********************************************************************
RENDER INIT
Responsibilities
- Shader compilation
- Snap grid mesh construction
- Uniform location caching (obj/road/ocean/depth/point-light)
- Water detail texture load
- Line batch allocation
- GL resource teardown
**********************************************************************/

void editor_renderer_init(EditorRenderer& er){
    shader_init_from_file(er.shader, "../assets/shaders/gizmo.vert", "../assets/shaders/gizmo.frag");
    shader_init_from_file(er.obj_shader, "../assets/shaders/lit.vert", "../assets/shaders/object.frag");
    shader_init_from_file(er.road_shader, "../assets/shaders/lit.vert", "../assets/shaders/road.frag");
    shader_init_from_file(er.ocean_shader, "../assets/shaders/ocean.vert", "../assets/shaders/ocean.frag");
    font_init(er.font, Const::WINDOW_WIDTH, Const::WINDOW_HEIGHT);
    shader_init_from_file(er.depth_shader, "../assets/shaders/depth.vert", "../assets/shaders/depth.frag");

    // build the snap grid as a static mesh
    // two sets parallel one along x, one along z
    // color is baked into vertex buffer so no need to uniform per line
    std::vector<float> lines;
    float r = (float) Const::EDITOR_GRID_RADIUS;

    for (int i = -Const::EDITOR_GRID_RADIUS; i <= Const::EDITOR_GRID_RADIUS; i++){
        float f = (float)i;

        // line along X axis at Z=f
        lines.insert(lines.end(), { -r, 0.0f, f,  0.3f, 0.3f, 0.3f });
        lines.insert(lines.end(), {  r, 0.0f, f,  0.3f, 0.3f, 0.3f });
        // line along Z axis at X=f
        lines.insert(lines.end(), {  f, 0.0f,-r,  0.3f, 0.3f, 0.3f });
        lines.insert(lines.end(), {  f, 0.0f, r,  0.3f, 0.3f, 0.3f });
    }

    mesh_init(er.grid, lines);

    // CACHED UNIFORMS FOR ALL LOCS ONCE HERE
    auto cache_obj = [&](){
        auto& L = er.obj_loc;
        GLuint id = er.obj_shader.id;
        L.view = glGetUniformLocation(id, "u_view");
        L.proj = glGetUniformLocation(id, "u_proj");
        L.model = glGetUniformLocation(id, "u_model");
        L.normal_mat = glGetUniformLocation(id, "u_normal_mat");
        L.light_dir = glGetUniformLocation(id, "u_light_dir");
        L.light_space = glGetUniformLocation(id, "u_light_space");
        L.shadow_bias = glGetUniformLocation(id, "u_shadow_bias");
        L.ambient = glGetUniformLocation(id, "u_ambient");
        L.diff_intensity = glGetUniformLocation(id, "u_diff_intensity");
        L.light_color = glGetUniformLocation(id, "u_light_color");
        L.shadow_map = glGetUniformLocation(id, "u_shadow_map");
        L.tex = glGetUniformLocation(id, "u_tex");
        L.use_texture = glGetUniformLocation(id, "u_use_texture");
        L.kd = glGetUniformLocation(id, "u_kd");
        L.fog_color = glGetUniformLocation(id, "u_fog_color");
        L.fog_near = glGetUniformLocation(id, "u_fog_near");
        L.fog_far = glGetUniformLocation(id, "u_fog_far");
        L.fog_cam_pos = glGetUniformLocation(id, "u_cam_pos_fog");
    };
    auto cache_road = [&](){
        auto& L = er.road_loc;
        GLuint id = er.road_shader.id;
        L.view = glGetUniformLocation(id, "u_view");
        L.proj = glGetUniformLocation(id, "u_proj");
        L.model = glGetUniformLocation(id, "u_model");
        L.normal_mat = glGetUniformLocation(id, "u_normal_mat");
        L.light_dir = glGetUniformLocation(id, "u_light_dir");
        L.light_space = glGetUniformLocation(id, "u_light_space");
        L.shadow_bias = glGetUniformLocation(id, "u_shadow_bias");
        L.ambient = glGetUniformLocation(id, "u_ambient");
        L.diff_intensity = glGetUniformLocation(id, "u_diff_intensity");
        L.light_color = glGetUniformLocation(id, "u_light_color");
        L.shadow_map = glGetUniformLocation(id, "u_shadow_map");
        L.tex = glGetUniformLocation(id, "u_tex");
        L.use_texture = glGetUniformLocation(id, "u_use_texture");
        L.kd = glGetUniformLocation(id, "u_kd");
        L.fog_color = glGetUniformLocation(id, "u_fog_color");
        L.fog_near = glGetUniformLocation(id, "u_fog_near");
        L.fog_far = glGetUniformLocation(id, "u_fog_far");
        L.fog_cam_pos = glGetUniformLocation(id, "u_cam_pos_fog");
    };
    cache_obj();
    cache_road();

     {
        GLuint id = er.ocean_shader.id;
        auto& OL = er.ocean_loc;
        OL.view = glGetUniformLocation(id, "u_view");
        OL.proj = glGetUniformLocation(id, "u_proj");
        OL.time = glGetUniformLocation(id, "u_time");
        OL.y_level = glGetUniformLocation(id, "u_y_level");
        OL.light_dir = glGetUniformLocation(id, "u_light_dir");
        OL.cam_pos = glGetUniformLocation(id, "u_cam_pos");
        OL.light_color = glGetUniformLocation(id, "u_light_color");
        OL.ambient = glGetUniformLocation(id, "u_ambient");
        OL.diff_intensity = glGetUniformLocation(id, "u_diff_intensity");
        OL.reflect_tex = glGetUniformLocation(id, "u_reflect_tex");
        OL.refl_view_proj = glGetUniformLocation(id, "u_refl_view_proj");
        OL.normal_tex = glGetUniformLocation(id, "u_normal_tex");
        OL.foam_tex = glGetUniformLocation(id, "u_foam_tex");
        OL.foam_shore_tex = glGetUniformLocation(id, "u_foam_shore_tex");
        OL.max_depth = glGetUniformLocation(id, "u_max_depth");
    }

    er.ocean_normal_tex = load_water_texture("../assets/shaders/textures/water_normal.png");
    er.ocean_foam_tex = load_water_texture("../assets/shaders/textures/foam.png");
    er.ocean_foam_shore_tex = load_water_texture("../assets/shaders/textures/foam_shore.png");
    er.depth_loc.light_space = glGetUniformLocation(er.depth_shader.id, "u_light_space");
    er.depth_loc.model = glGetUniformLocation(er.depth_shader.id, "u_model");

    {
        GLuint id = er.obj_shader.id;
        auto& LL = er.pt_light_loc;
        LL.count = glGetUniformLocation(id, "u_light_count");
        char buf[64];
        for (int i = 0; i < Const::MAX_POINT_LIGHTS; i++){
            snprintf(buf, sizeof(buf), "u_light_pos[%d]", i);
            LL.pos[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_color_pt[%d]", i);
            LL.color[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_radius[%d]", i);
            LL.radius[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_intensity[%d]", i);
            LL.intensity[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_spot_dir[%d]", i);
            LL.spot_dir[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_cos_cutoff[%d]", i);
            LL.cos_cutoff[i] = glGetUniformLocation(id, buf);
        }
    }

    // init persistent line batch with empty buffer
    // sized for worst case: 200 wire boxes * 24 verts each
    er.line_verts.reserve(200 * 24 * 6);
    std::vector<float> empty(6, 0.0f);
    mesh_init(er.line_batch, empty);
}

void editor_renderer_destroy(EditorRenderer& er){
    shader_destroy(er.shader);
    shader_destroy(er.obj_shader);
    shader_destroy(er.road_shader);
    shader_destroy(er.ocean_shader);
    shader_destroy(er.depth_shader);
    mesh_destroy(er.grid);
    if (er.terrain_mesh.vao) mesh_destroy(er.terrain_mesh);
    if (er.terrain_surface_mesh.vao) mesh_destroy(er.terrain_surface_mesh);
    font_destroy(er.font);
    for (auto& [name, mesh] : er.prop_cache)
        obj_mesh_destroy(mesh);
    er.prop_cache.clear();
    for (auto& [path, id] : er.tex_cache)
        if (id) glDeleteTextures(1, &id);
    er.tex_cache.clear();
    if (er.ocean_normal_tex) glDeleteTextures(1, &er.ocean_normal_tex);
    if (er.ocean_foam_tex) glDeleteTextures(1, &er.ocean_foam_tex);
    if (er.ocean_foam_shore_tex) glDeleteTextures(1, &er.ocean_foam_shore_tex);
}