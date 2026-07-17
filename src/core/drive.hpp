#pragma once
#include "app.hpp"
#include "npc_update.hpp"
#include "../renderer/render_props.hpp"
#include "../renderer/render_terrain.hpp"
#include "../renderer/render_road.hpp"
#include "../renderer/render_ocean.hpp"
#include "../renderer/render_hud.hpp"

// full drive-mode frame: trike/foot input, fixed-timestep physics,
// collision, camera, audio, shadow pass, planar reflection, main draw,
// NPC draw, destination marker, rain, HUD
void app_run_drive(App& app, float dt);
