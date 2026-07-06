#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include "../renderer/editor_renderer.hpp"
#include <GLFW/glfw3.h>

// ocean mode: Y level nudge, enable/disable toggle, save
void editor_input_ocean(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, float dt);