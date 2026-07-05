#pragma once
#include "editor_renderer.hpp"
#include "../core/editor_state.hpp"
#include "../world/world_map.hpp"

// draws all mode-specific control hints, palettes, and status text
void editor_renderer_draw_hud(EditorRenderer& er, const EditorState& editor, const WorldMap& map);

// draws the full settings menu (main/graphics/controls/maps pages)
void editor_renderer_draw_settings_menu(EditorRenderer& er, const EditorState& editor);