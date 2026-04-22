#include "mesh.hpp"

void mesh_init(Mesh& m, const std::vector<float>& vertices){

    // 6 floats per vertex
    m.count= (int)vertices.size() / 6;

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);

    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_STATIC_DRAW);

    // attribute 0, position - first 3 floats of each vertex
    // stride is 6 floats/24 bytes- which is the full size of one vertex
    // offset is 0- position starts at the beginning
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // attribute 1: color- last 3 floats of each vertex
    // offset is 3 floats/12 bytes - color starts after positioning
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
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