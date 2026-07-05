#pragma once
#include "app.hpp"
#include "../renderer/render_props.hpp"
#include "../renderer/render_terrain.hpp"
#include "../renderer/render_road.hpp"
#include "../renderer/render_ocean.hpp"
#include "../renderer/render_pose.hpp"
#include "../renderer/render_gizmo.hpp"
#include "../renderer/render_hud.hpp"

// full editor-mode frame: camera, input, shadow pass, planar reflection,
// main draw, HUD, pose mode overlay, pedestrian config overlay
void app_run_editor(App& app, float dt);