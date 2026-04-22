#pragma once
#include <vector>
#include <glm/glm.hpp>

// all push_ functions append vertices directly into the provided vector
// vertex layout: px py pz r g b (6 floats per vertex, 2 triangles per face)

void push_box(
    std::vector<float>& v,
    glm::vec3 origin,   // centre-bottom of the box
    glm::vec3 size,     // full width, height, depth
    glm::vec3 color
);

void push_cylinder(
    std::vector<float>& v,
    glm::vec3 center,   // centre of the cylinder
    float radius,
    float depth,        // thickness along Z axis
    int segments,     // 12 is fine for wheels
    glm::vec3 color
);