#pragma once
#include "editor_renderer.hpp"
#include "../world/ocean.hpp"
#include "../world/height_field.hpp"

// advances ocean.time and draws the ocean mesh with reflection + foam
void editor_renderer_draw_ocean(EditorRenderer& er, Ocean& ocean, const HeightField& hf,
    const glm::mat4& view, const glm::mat4& proj, float dt,
    float terrain_x_min, float terrain_x_max, float terrain_z_min, float terrain_z_max);