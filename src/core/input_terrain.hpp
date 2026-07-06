#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include "../renderer/editor_renderer.hpp"
#include <GLFW/glfw3.h>

// terrain sculpt + paint mode: raise/lower/smooth, surface paint, brush radius,
// undo, flatten, save
void editor_input_terrain(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, float dt);