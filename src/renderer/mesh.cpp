#include "mesh.hpp"
#include <glad/glad.h>

void mesh_init(Mesh& m, const std::vector<float>& verts){
    m.count = (int)verts.size() / 6;

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    // attribute 0: position (xyz)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // attribute 1: normal (xyz)  ← was wrongly labeled "color"
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void mesh_destroy(Mesh& m){
    glDeleteVertexArrays(1, &m.vao);
    glDeleteBuffers(1, &m.vbo);
    m.vao = m.vbo = 0;
}

void mesh_draw(const Mesh& m){
    glBindVertexArray(m.vao);
    glDrawArrays(GL_TRIANGLES, 0, m.count);
    glBindVertexArray(0);
}