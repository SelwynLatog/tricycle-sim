#pragma once
#include "app.hpp"
#include "npc_update.hpp"

// full drive-mode frame: trike/foot input, fixed-timestep physics,
// collision, camera, audio, shadow pass, planar reflection, main draw,
// NPC draw, destination marker, rain, HUD
void app_run_drive(App& app, float dt);