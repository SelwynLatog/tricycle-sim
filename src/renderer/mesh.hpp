#pragma once
#include <glad/glad.h>
#include <vector>

struct Mesh {
    GLuint vao= 0;
    GLuint vbo= 0;
    int count= 0;  // number of vertices
};

// Each vertex is: position (xyz) + color (rgb) 6 floats, 24 bytes
void mesh_init(Mesh& m, const std::vector<float>& vertices);
void mesh_destroy(Mesh& m);
void mesh_draw(const Mesh& m);