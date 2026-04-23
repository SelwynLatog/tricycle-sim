#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Mesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int count = 0;
};

// layout: px py pz nx ny nz (6 floats per vertex)
// attribute 0 = position, attribute 1 = normal
void mesh_init(Mesh& m, const std::vector<float>& verts);
void mesh_destroy(Mesh& m);
void mesh_draw(const Mesh& m);