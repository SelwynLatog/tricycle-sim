#pragma once
#include "scene.hpp"
#include "../physics/obstacle.hpp"
#include "../physics/trike_state.hpp"

// computes the light-space matrix for the current shadow pass,
// snapped to shadow-texel granularity around `center` to avoid shimmer
void scene_shadow_pass(SceneState& scene, const std::vector<Obstacle>& obstacles, glm::vec3 center);

// mirrors a view matrix across the horizontal plane y = water_y
// used to render the world from the "other side" of the water surface for planar reflections
// reused by both drive mode and editor mode
glm::mat4 scene_build_reflect_view(const glm::mat4& view, float water_y);

// draws the trike into the currently bound shadow depth pass
void scene_trike_shadow_draw(SceneState& scene, const TrikeState& trike);