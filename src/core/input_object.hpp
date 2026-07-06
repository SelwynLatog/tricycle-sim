#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include "../renderer/editor_renderer.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// default editor mode: tool switching, transform (translate/rotate/scale),
// placement, deletion, behavior/preset cycling, copy/paste, pedestrian config,
// prop palette selection, F5 rescan
void editor_input_object(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty);