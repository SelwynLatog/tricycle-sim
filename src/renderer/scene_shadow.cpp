#include "scene_shadow.hpp"
#include "scene_helper.hpp"
#include "render_helpers.hpp"
#include "../core/const.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

/**********************************************************************
SCENE SHADOW
Responsibilities
- Light-space matrix computation for the shadow pass (texel-snapped)
- Planar reflection view matrix (mirror across water plane)
- Trike shadow-pass draw
**********************************************************************/

// legacy debug helpers, unused since the obstacle box-drawing block
//
//
// static void draw_wire_aabb(
//     const Shader& gizmo_shader,
//     const Aabb& box,
//     glm::vec3 color,
//     const glm::mat4& view,
//     const glm::mat4& proj)
// {
//     glm::vec3 lo = box.min, hi = box.max;
//     glm::vec3 c[8] = {
//         {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},
//         {hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},
//         {lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},
//         {hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},
//     };
//     int e[24] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
//
//     std::vector<float> v;
//     for (int i = 0; i < 24; i += 2){
//         glm::vec3 a = c[e[i]], b = c[e[i+1]];
//         v.insert(v.end(), {a.x,a.y,a.z, color.r,color.g,color.b});
//         v.insert(v.end(), {b.x,b.y,b.z, color.r,color.g,color.b});
//     }
//
//     Mesh wire;
//     mesh_init(wire, v);
//     shader_bind(gizmo_shader);
//     set_mat4(gizmo_shader, "u_view", view);
//     set_mat4(gizmo_shader, "u_proj", proj);
//     set_mat4(gizmo_shader, "u_model", glm::mat4(1.0f));
//     glBindVertexArray(wire.vao);
//     glDrawArrays(GL_LINES, 0, wire.count);
//     glBindVertexArray(0);
//     mesh_destroy(wire);
// }
//
// static void draw_box_lit(
//     const Shader& shader,
//     glm::vec3 center_bottom,
//     glm::vec3 full_size,
//     glm::vec3 color,
//     const glm::mat4& view,
//     const glm::mat4& proj)
// {
//     glm::mat4 m = glm::translate(glm::mat4(1.0f), center_bottom);
//     glm::mat3 nm = glm::mat3(glm::transpose(glm::inverse(m)));
//
//     set_mat4(shader, "u_model", m);
//     set_mat3(shader, "u_normal_mat", nm);
//     set_vec3(shader, "u_kd", color);
//
//     std::vector<float> verts;
//     push_box_lit(verts, glm::vec3(0.0f), full_size, color);
//
//     Mesh box;
//     mesh_init(box, verts);
//     glBindVertexArray(box.vao);
//     glDrawArrays(GL_TRIANGLES, 0, box.count);
//     glBindVertexArray(0);
//     mesh_destroy(box);
// }

void scene_shadow_pass(SceneState& scene, const std::vector<Obstacle>& obstacles, glm::vec3 center){
    float texel_size = (2.0f * Const::SHADOW_ORTHO_SIZE) / (float)Const::SHADOW_MAP_SIZE;
    glm::vec3 snapped = glm::vec3(
        std::floor(center.x / texel_size) * texel_size,
        center.y,
        std::floor(center.z / texel_size) * texel_size);

    glm::vec3 light_pos = snapped + scene.sun_dir * 150.0f;
    glm::mat4 light_view = glm::lookAt(light_pos, snapped, glm::vec3(0,1,0));
    float s = Const::SHADOW_ORTHO_SIZE;
    scene.light_space_mat = glm::ortho(-s, s, -s, s, Const::SHADOW_NEAR, Const::SHADOW_FAR) * light_view;
}

glm::mat4 scene_build_reflect_view(const glm::mat4& view, float water_y){
    // reflect the camera across the y = water_y plane:
    // shift down to the plane, flip Y, shift back
    glm::mat4 mirror =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, water_y, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -water_y, 0.0f));
    return view * mirror;
}

void scene_trike_shadow_draw(SceneState& scene, const TrikeState& trike){
    glm::vec3 render_pos = trike.position;
    if (trike.is_tipping)
        render_pos.y = scene.model_half_height * std::abs(std::cos(trike.roll_angle));
    else if (trike.is_rolled_over)
        render_pos.y = 0.0f;

    glm::vec3 sc = scene.model_center * scene.model_scale;
    glm::mat4 tm =
        glm::translate(glm::mat4(1.0f), render_pos)
        * glm::rotate(glm::mat4(1.0f), -trike.heading, glm::vec3(0,1,0))
        * glm::rotate(glm::mat4(1.0f), -trike.roll_angle, glm::vec3(1,0,0))
        * glm::rotate(glm::mat4(1.0f), glm::radians(Const::TRIKE_MODEL_YAW_OFFSET), glm::vec3(0,1,0))
        * glm::translate(glm::mat4(1.0f), -sc)
        * glm::scale(glm::mat4(1.0f), glm::vec3(scene.model_scale));

    shader_bind(scene.shadow_shader);
    glUniformMatrix4fv(scene.shadow_loc.light_space, 1, GL_FALSE, glm::value_ptr(scene.light_space_mat));
    glUniformMatrix4fv(scene.shadow_loc.model, 1, GL_FALSE, glm::value_ptr(tm));

    if constexpr (Const::USE_PROC_MESH){
        glBindVertexArray(scene.proc_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, scene.proc_mesh.count);
        glBindVertexArray(0);
    } 
    else {
        ObjMesh& mesh = scene.trike_model.mesh;
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(mesh.data.vertices.size() / 8));
        glBindVertexArray(0);
    }
}