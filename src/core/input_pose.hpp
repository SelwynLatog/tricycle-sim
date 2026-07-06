#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include "../renderer/editor_renderer.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// pose mode: bone cycle, rotation, seat/bone translate, hail/mount toggle,
// save hail/mount/driver pose, dump pose as code
void editor_input_pose(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty);