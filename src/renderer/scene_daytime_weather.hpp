#pragma once
#include "scene.hpp"

// advances the day/night clock and derives sun direction, light color,
// ambient/diffuse intensity, sky blend/tint, fog color/distance, and
// rain overcast lighting for the current time of day
void scene_update_daytime(SceneState& scene, float dt);