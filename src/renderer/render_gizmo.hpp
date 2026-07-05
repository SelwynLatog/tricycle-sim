#pragma once
#include "editor_renderer.hpp"
#include "../core/editor_state.hpp"
#include "../world/world_map.hpp"

// draws:
// 1. snap grid on xz plane
// 2. light/ambience mode gizmo wires
// 3. placed WorldMap objects as colored hitbox wires (color by behavior)
// 4. ghost box at cursor showing where next obj will land
// 5. highlight box around the currently selected object
// 6. road-mode cursor diamond + preview line
// (calls editor_renderer_draw_props internally for the solid prop meshes)
void editor_renderer_draw(EditorRenderer& er, const EditorState& editor, const WorldMap& map,
    const glm::mat4& view, const glm::mat4& proj, bool show_hitboxes = false,
    const std::vector<LightSource>& lights = {});