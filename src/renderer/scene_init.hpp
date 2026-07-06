#pragma once
#include "scene.hpp"

// loads shaders, sky/rain/night textures, shadow + reflection FBOs,
// trike/driver models, ground/gizmo meshes, and caches all uniform locations
void scene_init(SceneState& scene);

// frees all GL resources owned by SceneState
void scene_destroy(SceneState& scene);

// recreates the shadow depth texture at the current settings resolution
// called when the shadow map size setting changes
void scene_shadow_resize(SceneState& scene);