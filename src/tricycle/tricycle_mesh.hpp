#pragma once
#include <vector>

// builds the full tricycle vertex buffer from named parts
// call once at startup, pass the result to mesh_init()
void build_tricycle_mesh(std::vector<float>& verts);