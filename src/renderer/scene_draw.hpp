#pragma once
#include "scene.hpp"
#include "../physics/obstacle.hpp"
#include "../physics/trike_state.hpp"
#include "../physics/player_state.hpp"
#include "../world/light_source.hpp"

// draws the skybox (day/night/rain blend) as a fullscreen quad
void scene_draw_sky(SceneState& scene, const glm::mat4& view, const glm::mat4& proj);

// draws ground, trike, and (optionally) AABB hitbox wireframes
// view/proj come from app since camera lives there
void scene_draw(
    SceneState& scene,
    const TrikeState& trike,
    const std::vector<Obstacle>& obstacles,
    const std::vector<LightSource>& lights,
    const glm::mat4& view,
    const glm::mat4& proj,
    bool show_hitboxes = false
);

// draws the driver model posed for the current player/trike state
void scene_draw_driver(
    SceneState& scene,
    const PlayerState& player,
    const TrikeState& trike,
    const glm::mat4& view,
    const glm::mat4& proj,
    const Shader& lit_shader,
    const glm::quat pose_quats[BONE_COUNT],
    const glm::vec3 pose_offsets[BONE_COUNT],
    glm::vec3 pose_seat);

// draws the pulsing destination marker ring while a passenger is riding
void scene_draw_drop_marker(SceneState& scene, glm::vec3 pos, float pulse, const glm::mat4& view, const glm::mat4& proj);