#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// ambience mode: place/select zone, radius, type toggle, audio file assign, delete, save
void editor_input_ambience(EditorState& editor, WorldMap& map, GLFWwindow* window, float dt, bool& map_dirty);