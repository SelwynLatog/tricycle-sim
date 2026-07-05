#pragma once
#include "editor_renderer.hpp"
#include <vector>

// uploads a mat4 uniform by name to the currently bound shader
void set_mat4(const Shader& s, const char* n, const glm::mat4& m);

// appends a wireframe box (12 edges) in world space to a line-vertex buffer
void push_wire_box(std::vector<float>& verts, const glm::vec3& mn, const glm::vec3& mx, glm::vec3 color);

// flushes er.line_verts to the persistent line_batch and draws it
void flush_line_batch(EditorRenderer& er, const Shader& shader, const glm::mat4& view, const glm::mat4& proj);

// uploads point lights into a lit shader's u_light_pos/color/radius/intensity arrays
// culls by distance to cam_pos, writes only lights that pass into slots 0..N
// shader must already be bound before calling this
void upload_point_lights(GLuint shader_id, const std::vector<LightSource>& lights,
    const glm::vec3& cam_pos, float night_factor);

// maps an ObjectBehavior to a debug wireframe color
glm::vec3 behavior_color(ObjectBehavior b);

// computes world-space AABB for a rotated, scaled, y-offset prop from its local bounds
void rotated_world_bounds(
    glm::vec3 lmin, glm::vec3 lmax,
    const glm::vec3& pos, float yaw, const glm::vec3& scale, float yoff,
    glm::vec3& out_min, glm::vec3& out_max);

// draws the full-screen dark overlay quad used behind the settings menu
void draw_settings_overlay(EditorRenderer& er);