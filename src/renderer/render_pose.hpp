#pragma once
#include "editor_renderer.hpp"
#include "../core/editor_state.hpp"
#include "../tricycle/driver_model.hpp"
#include "../tricycle/tricycle_model.hpp"
#include "../world/world_map.hpp"

// draws the isolated pose-editing view: reference trike + driver or NPC in the pose being edited
void editor_renderer_draw_pose_mode(EditorRenderer& er, const EditorState& editor,
    const DriverModel& driver, const TrikeModel& trike,
    const glm::mat4& view, const glm::mat4& proj,
    const DriverModel* npc_model, const WorldMap& map = WorldMap{});