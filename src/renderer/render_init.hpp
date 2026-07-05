#pragma once
#include "editor_renderer.hpp"

// compiles shaders, builds the static grid mesh, caches all uniform locations,
// loads water detail textures, and allocates the persistent line batch
void editor_renderer_init(EditorRenderer& er);

// frees all GL resources owned by EditorRenderer
void editor_renderer_destroy(EditorRenderer& er);