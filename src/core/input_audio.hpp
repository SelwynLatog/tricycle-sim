#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include <GLFW/glfw3.h>

// audio mode: slot cycling, file assign from list, proximity radius, clear slot, save
void editor_input_audio(EditorState& editor, WorldMap& map, GLFWwindow* window,
    float dt, bool& map_dirty);