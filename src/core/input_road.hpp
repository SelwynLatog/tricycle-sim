#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include <GLFW/glfw3.h>

// road spline mode: add/undo control points, cycle road type, Y nudge, finish, delete, save
void editor_input_road(EditorState& editor, WorldMap& map, GLFWwindow* window, float dt);