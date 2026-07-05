#pragma once
#include "editor_renderer.hpp"
#include <string>

// loads a texture from disk into GL, caches by path in er.tex_cache
// returns 0 on failure
GLuint load_texture(EditorRenderer& er, const std::string& path);

// loads a water-detail texture (normal map / foam) with linear filtering
// has no disk cache 
// loads everything once at startup
GLuint load_water_texture(const std::string& path);

// loads (or returns cached) ObjMesh for a prop filename
// also computes and caches local bounds + y floor offset as a side effect
ObjMesh& get_prop_mesh(EditorRenderer& er, const std::string& filename);

// returns the y floor offset for a given prop filename, loading it if needed
float editor_get_y_floor_offset(EditorRenderer& er, const std::string& filename);

// preloads GL textures for every material referenced by cached prop meshes
void editor_renderer_preload_textures(EditorRenderer& er);