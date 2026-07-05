#pragma once
#include "editor_renderer.hpp"
#include "../physics/dynamic_sim.hpp"
#include "../world/world_map.hpp"
#include <map>
#include <unordered_map>

// draws only placed obj meshes; called both in editor and drive mode
// flash_map: world_object_id -> hit_timer value (0 = no flash)
// dynamic_sims: when present, DYNAMIC objects render from sim position/angles instead of placed transform
void editor_renderer_draw_props(EditorRenderer& er, const WorldMap& map,
    const glm::mat4& view, const glm::mat4& proj,
    const std::map<int,float>& flash_map = {},
    const std::unordered_map<int, DynamicSim>& dynamic_sims = {},
    const std::vector<LightSource>& lights = {},
    bool skip_pedestrians = false);

void editor_renderer_shadow_pass(EditorRenderer& er, const WorldMap& map,
    const glm::mat4& light_space_mat,
    const std::unordered_map<int, DynamicSim>& dynamic_sims = {});