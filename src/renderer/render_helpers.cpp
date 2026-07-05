#include "render_helpers.hpp"
#include "../core/const.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

/**********************************************************************
RENDER HELPERS
Responsibilities
- Shared uniform upload (set_mat4)
- Wireframe box + line batch utilities
- Point light uniform upload
- Behavior color mapping
- Rotated world-space AABB computation
- Settings menu dark overlay quad
**********************************************************************/

void set_mat4(const Shader& s, const char* n, const glm::mat4& m){
    glUniformMatrix4fv(glGetUniformLocation(s.id, n), 1, GL_FALSE, glm::value_ptr(m));
}

// draws a wireframe box from world space min/max corners
// then uplaods a throwaway vao.vbo each call & destroys after drajwing
// not meant for high frequency draws
// purposely for editor only
void push_wire_box(
    std::vector<float>& verts,
    const glm::vec3& mn, const glm::vec3& mx,
    glm::vec3 color){
    glm::vec3 c[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},
        {mx.x,mn.y,mx.z},{mn.x,mn.y,mx.z},
        {mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
        {mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},
    };
    int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
    for (int i = 0; i < 24; i++){
        glm::vec3 p = c[e[i]];
        verts.insert(verts.end(), { p.x,p.y,p.z, color.r,color.g,color.b });
    }
}

// flushes er.line_verts to the persistent line_batch and draws it
void flush_line_batch(EditorRenderer& er, const Shader& shader,
    const glm::mat4& view, const glm::mat4& proj){
    if (er.line_verts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, er.line_batch.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        er.line_verts.size() * sizeof(float),
        er.line_verts.data(), GL_DYNAMIC_DRAW);
    er.line_batch.count = (int)er.line_verts.size() / 6;
    shader_bind(shader);
    set_mat4(shader, "u_model", glm::mat4(1.0f));
    set_mat4(shader, "u_view",  view);
    set_mat4(shader, "u_proj",  proj);
    glBindVertexArray(er.line_batch.vao);
    glDrawArrays(GL_LINES, 0, er.line_batch.count);
    glBindVertexArray(0);
    er.line_verts.clear();
}

// uploads point lights into a lit shader's u_light_pos/color/radius/intensity arrays
// culls by distance to cam_pos, writes only the lights that pass into slots 0..N
// shader must already be bound before calling this
void upload_point_lights(GLuint shader_id, const std::vector<LightSource>& lights,
    const glm::vec3& cam_pos, float night_factor){
    int active = 0;
    char buf[64];
    if (night_factor >= 0.01f){
        for (int i = 0; i < (int)lights.size() && active < Const::MAX_POINT_LIGHTS; i++){
            glm::vec3 d = lights[i].position - cam_pos;
            if (glm::dot(d, d) > Const::LIGHT_CULL_DIST_SQ) continue;
            snprintf(buf, sizeof(buf), "u_light_pos[%d]", active);
            glUniform3f(glGetUniformLocation(shader_id, buf),
                lights[i].position.x, lights[i].position.y, lights[i].position.z);
            snprintf(buf, sizeof(buf), "u_light_color_pt[%d]", active);
            glUniform3f(glGetUniformLocation(shader_id, buf),
                lights[i].color.r, lights[i].color.g, lights[i].color.b);
            snprintf(buf, sizeof(buf), "u_light_radius[%d]", active);
            glUniform1f(glGetUniformLocation(shader_id, buf), lights[i].radius);
            snprintf(buf, sizeof(buf), "u_light_intensity[%d]", active);
            glUniform1f(glGetUniformLocation(shader_id, buf), lights[i].intensity * night_factor);
            snprintf(buf, sizeof(buf), "u_light_spot_dir[%d]", active);
            glUniform3f(glGetUniformLocation(shader_id, buf), lights[i].spot_dir.x, lights[i].spot_dir.y, lights[i].spot_dir.z);
            snprintf(buf, sizeof(buf), "u_light_cos_cutoff[%d]", active);
            glUniform1f(glGetUniformLocation(shader_id, buf), lights[i].cos_cutoff);
            active++;
        }
    }
    glUniform1i(glGetUniformLocation(shader_id, "u_light_count"), active);
}

// maps out a color based on obj behavior
// makes it easier to read at first glance
glm::vec3 behavior_color(ObjectBehavior b){
     switch(b){
        case STATIC: return {0.55f, 0.55f, 0.55f}; // grey
        case DYNAMIC: return {0.20f, 0.50f, 1.00f}; // blue
        case PEDESTRIAN: return {0.20f, 0.85f, 0.30f}; // green
        case DECORATION: return {0.95f, 0.80f, 0.10f}; // yellow
        default: return {1.00f, 1.00f, 1.00f};
    }
}

void rotated_world_bounds(
    glm::vec3 lmin, glm::vec3 lmax,
    const glm::vec3& pos, float yaw, const glm::vec3& scale, float yoff,
    glm::vec3& out_min, glm::vec3& out_max
){

    // apply scale and y offset to local corners
    glm::vec3 smin = glm::vec3(lmin.x * scale.x, (lmin.y + yoff) * scale.y, lmin.z * scale.z);
    glm::vec3 smax = glm::vec3(lmax.x * scale.x, (lmax.y + yoff) * scale.y, lmax.z * scale.z);

    float c = std::cos(yaw);
    float s = std::sin(yaw);

    out_min = glm::vec3( 1e9f);
    out_max = glm::vec3(-1e9f);

    for (int i = 0; i < 8; i++){
        glm::vec3 corner = {
            (i & 1) ? smax.x : smin.x,
            (i & 2) ? smax.y : smin.y,
            (i & 4) ? smax.z : smin.z,
        };
        glm::vec3 rotated = {
            c * corner.x - s * corner.z,
            corner.y,
            s * corner.x + c * corner.z,
        };
        glm::vec3 world = pos + rotated;
        out_min = glm::min(out_min, world);
        out_max = glm::max(out_max, world);
    }
}

void draw_settings_overlay(EditorRenderer& er){
    static GLuint s_vao = 0, s_vbo = 0;
    if (!s_vao){
        float verts[] = {
            -1.f,-1.f,0.f,  0.f,0.f,0.f,
             1.f,-1.f,0.f,  0.f,0.f,0.f,
             1.f, 1.f,0.f,  0.f,0.f,0.f,
            -1.f,-1.f,0.f,  0.f,0.f,0.f,
             1.f, 1.f,0.f,  0.f,0.f,0.f,
            -1.f, 1.f,0.f,  0.f,0.f,0.f,
        };
        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);
        glBindVertexArray(s_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    glm::mat4 identity = glm::mat4(1.0f);
    shader_bind(er.shader);
    set_mat4(er.shader, "u_model", identity);
    set_mat4(er.shader, "u_view", identity);
    set_mat4(er.shader, "u_proj", identity);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_ONE, GL_ONE);
    glBlendColor(0.f, 0.f, 0.f, 0.60f);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendColor(0.f, 0.f, 0.f, 0.f);
    glDisable(GL_BLEND);
}