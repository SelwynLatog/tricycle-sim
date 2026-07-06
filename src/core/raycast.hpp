#pragma once
#include "editor_state.hpp"
#include "../world/world_map.hpp"
#include "../world/height_field.hpp"
#include "../renderer/editor_renderer.hpp"
#include <glm/glm.hpp>

// casts a ray from screen pixel (mx, my) into the world
// returns the ground hit pos (heightfield surface if terrain given, else y=0 plane)
// returns false if ray is parallel to ground
bool editor_raycast_ground(double mx, double my, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, glm::vec3& out_pos,
    const HeightField* terrain = nullptr);

// check if ray hits any placed object's AABB
// uses real mesh bounds from er.prop_bounds when available
// returns id of the closest hit object, -1 if none
// prefer_small = true selects smallest-volume AABB hit instead of closest-t
int editor_raycast_objects(double mx, double my, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, const WorldMap& map, const EditorRenderer& er, bool prefer_small = false);