#pragma once
#include "editor_renderer.hpp"
#include "../world/height_field.hpp"
#include "../world/ocean.hpp"

// rebuilds the wireframe terrain mesh from the heightfield
void editor_renderer_build_terrain_mesh(EditorRenderer& er, const HeightField& hf);

// draws the wireframe terrain + brush radius circle at cursor
void editor_renderer_draw_terrain(EditorRenderer& er, const HeightField& hf,
    const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& brush_pos, float brush_radius, bool placement_valid);

// rebuilds the textured terrain surface mesh, bucketed by surface type
void editor_renderer_build_terrain_surface(EditorRenderer& er, const HeightField& hf, const Ocean& ocean);

// draws the textured terrain surface, one draw call per surface type bucket
void editor_renderer_draw_terrain_surface(EditorRenderer& er, const HeightField& hf,
    const glm::mat4& view, const glm::mat4& proj, const Ocean& ocean);