#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// light mode: place/select, move XZ/Y, radius/intensity, RGB tint, delete, save
void editor_input_light(EditorState& editor, WorldMap& map, GLFWwindow* window,
    const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty);