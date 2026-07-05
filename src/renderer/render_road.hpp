#pragma once
#include "editor_renderer.hpp"
#include "../world/road_spline.hpp"
#include <vector>

// draws all road splines, textured by road type
void editor_renderer_draw_roads(EditorRenderer& er, const std::vector<RoadSpline>& roads, const glm::mat4& view, const glm::mat4& proj);