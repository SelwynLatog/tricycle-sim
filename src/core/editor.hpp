#pragma once
#include "app.hpp"

// full editor-mode frame: camera, input, shadow pass, planar reflection,
// main draw, HUD, pose mode overlay, pedestrian config overlay
void app_run_editor(App& app, float dt);