#include "scene_draw.hpp"
#include "scene_helper.hpp"
#include "render_helpers.hpp"
#include "../core/settings.hpp"
#include "../core/const.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

/**********************************************************************
SCENE DRAW
Responsibilities
- Sky rendering (day/night/rain blend fullscreen quad)
- World rendering (ground, trike, AABB hitbox wireframes)
- Driver rendering
- Drop marker (destination ring) overlay
**********************************************************************/

void scene_draw_sky(SceneState& scene, const glm::mat4& view, const glm::mat4& proj){
    if (!scene.sky_tex) return;

    glm::mat4 rot_only = glm::mat4(glm::mat3(view));
    glm::mat4 inv_vp = glm::inverse(proj * rot_only);

    auto& SL = scene.sky_loc;
    shader_bind(scene.sky_shader);
    glUniformMatrix4fv(SL.inv_view_proj, 1, GL_FALSE, glm::value_ptr(inv_vp));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.sky_tex);
    glUniform1i(SL.sky_tex, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, scene.sky_night_tex ? scene.sky_night_tex : scene.sky_tex);
    glUniform1i(SL.sky_night_tex, 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, scene.sky_rain_tex ? scene.sky_rain_tex : scene.sky_tex);
    glUniform1i(SL.sky_rain_tex, 2);
    glActiveTexture(GL_TEXTURE0);
    glUniform3f(SL.tint_a, scene.sky_tint_a.r, scene.sky_tint_a.g, scene.sky_tint_a.b);
    glUniform3f(SL.tint_b, scene.sky_tint_b.r, scene.sky_tint_b.g, scene.sky_tint_b.b);
    glUniform1i(SL.flip_a, scene.sky_flip_a);
    glUniform1i(SL.flip_b, scene.sky_flip_b);
    glUniform1f(SL.blend, scene.sky_blend);
    glUniform1f(SL.uv_offset, scene.sky_uv_offset);
    glUniform1i(SL.use_night_b, scene.sky_use_night_b);
    glUniform1f(SL.rain_blend, scene.sky_rain_blend);
    glUniform1f(SL.night_factor, scene.night_factor);
    
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(scene.sky_quad.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// =====================================================
// MAIN SCENE DRAW
//
// Draw order:
//
// 1. Ground
// 2. Obstacles
// 3. Dynamic lights
// 4. Vehicle
// 5. Debug overlays
//
// =====================================================
void scene_draw(
    SceneState& scene,
    const TrikeState& trike,
    const std::vector<Obstacle>& obstacles,
    const std::vector<LightSource>& lights,
    const glm::mat4& view,
    const glm::mat4& proj,
    bool show_hitboxes)
{
    // ground
    glm::mat4 gm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, Const::GROUND_Y_OFFSET, 0.0f));
    glm::mat3 gnm = glm::mat3(1.0f);

    auto& L = scene.shader_loc;
    shader_bind(scene.shader);
    glUniformMatrix4fv(L.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(L.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(L.light_dir, scene.sun_dir.x, scene.sun_dir.y, scene.sun_dir.z);
    glUniformMatrix4fv(L.light_space, 1, GL_FALSE, glm::value_ptr(scene.light_space_mat));
    glUniform3f(L.light_color, scene.light_color.r, scene.light_color.g, scene.light_color.b);
    glUniform1f(L.ambient, scene.ambient);
    glUniform1f(L.diff_intensity, scene.diff_intensity);
    glUniform1f(L.shadow_bias, Const::SHADOW_BIAS);
    glUniform3f(L.fog_color, scene.fog_color.r, scene.fog_color.g, scene.fog_color.b);
    glUniform1f(L.fog_near, my_settings.render_fog ? scene.fog_near : Const::CAM_FAR);
    glUniform1f(L.fog_far,  my_settings.render_fog ? scene.fog_far  : Const::CAM_FAR + 1.0f);

    // upload point lights 
    // cpu cull to camera distance
    glm::vec3 cam_pos = glm::vec3(glm::inverse(view)[3]);
    int active_lcount = 0;
    glUniform3f(L.fog_cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    if (scene.night_factor >= 0.01f){
        for (int i = 0; i < (int)lights.size() && active_lcount < Const::MAX_POINT_LIGHTS; i++){
            glm::vec3 d = lights[i].position - cam_pos;
            float light_cull_sq = my_settings.light_cull_dist * my_settings.light_cull_dist;
            if (glm::dot(d, d) > light_cull_sq) continue;
            glUniform3f(scene.light_loc.pos[active_lcount], lights[i].position.x, lights[i].position.y, lights[i].position.z);
            glUniform3f(scene.light_loc.color[active_lcount], lights[i].color.r, lights[i].color.g, lights[i].color.b);
            glUniform1f(scene.light_loc.radius[active_lcount], lights[i].radius);
            glUniform1f(scene.light_loc.intensity[active_lcount], lights[i].intensity * scene.night_factor);
            active_lcount++;
        }
    }
    glUniform1i(scene.light_loc.count, active_lcount);


    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, scene.shadow_depth_tex);
    glUniform1i(L.shadow_map, 1);
    glActiveTexture(GL_TEXTURE0);

    glUniformMatrix4fv(L.model, 1, GL_FALSE, glm::value_ptr(gm));
    glUniformMatrix3fv(L.normal_mat, 1, GL_FALSE, glm::value_ptr(gnm));
    glUniform3f(L.kd, Const::GROUND_KD, Const::GROUND_KD, Const::GROUND_KD);
    glUniform3f(L.kd_alt, Const::GROUND_KD_ALT, Const::GROUND_KD_ALT, Const::GROUND_KD_ALT);
    glUniform1f(L.checker_scale, 1.0f / Const::GROUND_GRID_TILE_SIZE);
    glUniform1i(L.use_checker, 0);

    // axis gizmo
    /*shader_bind(scene.gizmo_shader);
    set_mat4(scene.gizmo_shader, "u_view", view);
    set_mat4(scene.gizmo_shader, "u_proj", proj);
    set_mat4(scene.gizmo_shader, "u_model", glm::mat4(1.0f));
    glBindVertexArray(scene.gizmo.vao);
    glDrawArrays(GL_LINES, 0, scene.gizmo.count);
    glBindVertexArray(0);*/

    // trike 
    glm::vec3 render_pos = trike.position;
    if (trike.is_tipping) render_pos.y = scene.model_half_height * std::abs(std::cos(trike.roll_angle));

    else if (trike.is_rolled_over) render_pos.y = 0.0f;

    glm::vec3 sc = scene.model_center * scene.model_scale;
    glm::mat4 tm =
        glm::translate(glm::mat4(1.0f), render_pos)
        * glm::rotate(glm::mat4(1.0f), -trike.heading, glm::vec3(0,1,0))
        * glm::rotate(glm::mat4(1.0f), -trike.roll_angle, glm::vec3(1,0,0))
        * glm::rotate(glm::mat4(1.0f), glm::radians(Const::TRIKE_MODEL_YAW_OFFSET), glm::vec3(0,1,0))
        * glm::translate(glm::mat4(1.0f), -sc)
        * glm::scale(glm::mat4(1.0f), glm::vec3(scene.model_scale));

    glm::mat3 tnm = glm::mat3(tm);

    glUniformMatrix4fv(L.model, 1, GL_FALSE, glm::value_ptr(tm));
    glUniformMatrix3fv(L.normal_mat, 1, GL_FALSE, glm::value_ptr(tnm));

    if constexpr (Const::USE_PROC_MESH){
        // proc mesh uses rgb color layout not normals 
        // for now I'll draw with gizmo shader
        // lighting won't apply but colors are baked per face in mesh_builder
        
        shader_bind(scene.gizmo_shader);
        glUniformMatrix4fv(scene.gizmo_loc.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(scene.gizmo_loc.proj, 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(scene.gizmo_loc.model,1, GL_FALSE, glm::value_ptr(tm));
        glBindVertexArray(scene.proc_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, scene.proc_mesh.count);
        glBindVertexArray(0);
        shader_bind(scene.shader);
    } 
    else {
        trike_model_draw(scene.trike_model, trike, scene.shader, view, proj);
    }

    // cube meshes not drawn
    // world objects are rendered as OBJ meshes via editor_renderer_draw_props
    // commented for now instead of removed for further testing
    /*
    // obstacle solid meshes
    // flashes red on hit
    shader_bind(scene.shader);
    set_mat4(scene.shader, "u_view", view);
    set_mat4(scene.shader, "u_proj", proj);
    set_vec3(scene.shader, "u_light_dir", LIGHT_DIR);
    glUniform1i(glGetUniformLocation(scene.shader.id, "u_use_checker"), 0);

    for (const auto& obs : obstacles){
        float flash = glm::clamp(obs.hit_timer / 0.35f, 0.0f, 1.0f);
        glm::vec3 base = {0.45f, 0.43f, 0.40f};
        glm::vec3 hit = {0.9f, 0.15f, 0.10f};
        glm::vec3 color = glm::mix(base, hit, flash);

        glm::vec3 cb = glm::vec3(
            (obs.aabb.min.x + obs.aabb.max.x) * 0.5f,
             obs.aabb.min.y,
            (obs.aabb.min.z + obs.aabb.max.z) * 0.5f);

        draw_box_lit(scene.shader, cb, obs.half_extents * 2.0f, color, view, proj);
    }*/

    // AABB wireframes
    // green=trike, 
    // yellow=obstacles
    if (show_hitboxes){
        scene.line_verts.clear();
        auto push_aabb = [&](const Aabb& box, glm::vec3 col){
            glm::vec3 lo = box.min, hi = box.max;
            glm::vec3 c[8] = {
                {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},
                {hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},
                {lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},
                {hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},
            };
            int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
            for (int i = 0; i < 24; i++){
                glm::vec3 p = c[e[i]];
                scene.line_verts.insert(scene.line_verts.end(),
                    {p.x,p.y,p.z, col.r,col.g,col.b});
            }
        };

        push_aabb(trike.aabb, {0.0f,1.0f,0.3f});
        for (const auto& obs : obstacles)
            push_aabb(obs.aabb, {1.0f,0.9f,0.0f});

        glBindBuffer(GL_ARRAY_BUFFER, scene.line_batch.vbo);
        glBufferData(GL_ARRAY_BUFFER,
            scene.line_verts.size() * sizeof(float),
            scene.line_verts.data(), GL_DYNAMIC_DRAW);
        scene.line_batch.count = (int)scene.line_verts.size() / 6;

        glm::mat4 identity = glm::mat4(1.0f);
        shader_bind(scene.gizmo_shader);
        glUniformMatrix4fv(scene.gizmo_loc.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(scene.gizmo_loc.proj, 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(scene.gizmo_loc.model, 1, GL_FALSE, glm::value_ptr(identity));
        glBindVertexArray(scene.line_batch.vao);
        glDrawArrays(GL_LINES, 0, scene.line_batch.count);
        glBindVertexArray(0);
    }

    shader_bind(scene.shader);
}

void scene_draw_driver(
     SceneState& scene,
     const PlayerState& player,
     const TrikeState& trike,
     const glm::mat4& view,
     const glm::mat4& proj,
     const Shader& lit_shader,
     const glm::quat  pose_quats[BONE_COUNT],
     const glm::vec3  pose_offsets[BONE_COUNT],
     glm::vec3 pose_seat)
 {
     shader_bind(lit_shader);
     driver_model_draw(scene.driver_model, player, trike, lit_shader, view, proj,
         pose_quats, pose_offsets, pose_seat);
 }


void scene_draw_drop_marker(SceneState& scene, glm::vec3 pos, float pulse, const glm::mat4& view, const glm::mat4& proj){
    // glowing ring as line loop around drop point
    static constexpr int SEGMENTS = 24;
    static constexpr float RADIUS = 1.5f;
    float r = RADIUS * pulse;

    std::vector<float> verts;
    verts.reserve(SEGMENTS * 2 * 6);

    // pulsing yellow-green color
    float t = (float)glfwGetTime();
    float glow = 0.7f + 0.3f * std::sin(t * 4.0f);
    glm::vec3 col = {0.3f, 1.0f * glow, 0.2f};

    for (int i = 0; i < SEGMENTS; i++){
        float a0 = (float)i / SEGMENTS * glm::two_pi<float>();
        float a1 = (float)(i + 1) / SEGMENTS * glm::two_pi<float>();
        glm::vec3 p0 = pos + glm::vec3(std::cos(a0) * r, 0.05f, std::sin(a0) * r);
        glm::vec3 p1 = pos + glm::vec3(std::cos(a1) * r, 0.05f, std::sin(a1) * r);
        verts.insert(verts.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b});
        verts.insert(verts.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b});
    }

    // second ring elevated
    float up = 1.2f * pulse;
    for (int i = 0; i < SEGMENTS; i++){
        float a0 = (float)i / SEGMENTS * glm::two_pi<float>();
        float a1 = (float)(i + 1) / SEGMENTS * glm::two_pi<float>();
        glm::vec3 p0 = pos + glm::vec3(std::cos(a0) * r, up, std::sin(a0) * r);
        glm::vec3 p1 = pos + glm::vec3(std::cos(a1) * r, up, std::sin(a1) * r);
        verts.insert(verts.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b});
        verts.insert(verts.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b});
    }

    // vertical bars connecting the two rings
    for (int i = 0; i < SEGMENTS; i += 3){
        float a = (float)i / SEGMENTS * glm::two_pi<float>();
        glm::vec3 bot = pos + glm::vec3(std::cos(a) * r, 0.05f, std::sin(a) * r);
        glm::vec3 top = pos + glm::vec3(std::cos(a) * r, up,    std::sin(a) * r);
        verts.insert(verts.end(), {bot.x, bot.y, bot.z, col.r, col.g, col.b});
        verts.insert(verts.end(), {top.x, top.y, top.z, col.r, col.g, col.b});
    }

    glBindBuffer(GL_ARRAY_BUFFER, scene.line_batch.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    scene.line_batch.count = (int)verts.size() / 6;

    glm::mat4 identity = glm::mat4(1.0f);
    shader_bind(scene.gizmo_shader);
    glUniformMatrix4fv(scene.gizmo_loc.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(scene.gizmo_loc.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(scene.gizmo_loc.model, 1, GL_FALSE, glm::value_ptr(identity));
    glBindVertexArray(scene.line_batch.vao);
    glDrawArrays(GL_LINES, 0, scene.line_batch.count);
    glBindVertexArray(0);
}