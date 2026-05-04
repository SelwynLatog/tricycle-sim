#include "mesh_builder.hpp"
#include <glm/glm.hpp>
#include <cmath>

//  internal helpers
void push_vertex(std::vector<float>& v, glm::vec3 pos, glm::vec3 col){
    v.push_back(pos.x);
    v.push_back(pos.y);
    v.push_back(pos.z);
    v.push_back(col.x);
    v.push_back(col.y);
    v.push_back(col.z);
}

void push_quad(std::vector<float>& v, glm::vec3 a, glm::vec3 b,
    glm::vec3 c, glm::vec3 d, glm::vec3 col){
    // a quad is 2 triangles: ABC and ACD
    // winding order is counter clockwise
    push_vertex(v, a, col);
    push_vertex(v, b, col);
    push_vertex(v, c, col);
    push_vertex(v, a, col);
    push_vertex(v, c, col);
    push_vertex(v, d, col);
}

//  push_box
void push_box(std::vector<float>& v, glm::vec3 o, glm::vec3 s, glm::vec3 col){
    // compute the 8 corners of the box from origin (centre-bottom) and size
    float x0 = o.x - s.x * 0.5f, x1 = o.x + s.x * 0.5f;
    float y0 = o.y,               y1 = o.y + s.y;
    float z0 = o.z - s.z * 0.5f, z1 = o.z + s.z * 0.5f;

    // trying to achieve that low polymorph sort of gmod style
    // but instead of actually learning to 3D model (fuck that),
    // each face just needs slightly different shades so the box reads it as "3D"
    // we fake ambient occlusion by darkening side and bottom faces
    glm::vec3 top   = col;
    glm::vec3 side  = col * 0.75f;
    glm::vec3 front = col * 0.85f;
    glm::vec3 bot   = col * 0.5f;

    // Top
    push_quad(v, {x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}, top);
    // Bottom
    push_quad(v, {x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}, bot);
    // Front  (positive Z)
    push_quad(v, {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}, front);
    // Back   (negative Z)
    push_quad(v, {x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}, front);
    // Right  (positive X)
    push_quad(v, {x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1}, side);
    // Left   (negative X)
    push_quad(v, {x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0}, side);
}

void push_box_lit(std::vector<float>& v, glm::vec3 o, glm::vec3 s, glm::vec3){
    float x0=o.x-s.x*0.5f, x1=o.x+s.x*0.5f;
    float y0=o.y, y1=o.y+s.y;
    float z0=o.z-s.z*0.5f, z1=o.z+s.z*0.5f;

    auto push = [&](glm::vec3 p, glm::vec3 n){
        v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
        v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    };
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n){
        push(a,n); push(b,n); push(c,n);
        push(a,n); push(c,n); push(d,n);
    };

    quad({x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}, { 0, 1, 0}); // top
    quad({x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}, { 0,-1, 0}); // bottom
    quad({x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}, { 0, 0, 1}); // front
    quad({x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}, { 0, 0,-1}); // back
    quad({x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1}, { 1, 0, 0}); // right
    quad({x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0}, {-1, 0, 0}); // left
}

//  push_cylinder
void push_cylinder(std::vector<float>& v,
    glm::vec3 center, float radius, float depth, int seg, glm::vec3 col){
    // a cylinder is a disc front face, disc back face, and a ring of quads connecting them
    // we build it along the Z axis centered on "center"
    float half     = depth * 0.5f;
    glm::vec3 side = col * 0.6f;

    for (int i = 0; i < seg; i++)
    {
        float a0 = (float)i       / seg * 2.0f * 3.14159265f;
        float a1 = (float)(i + 1) / seg * 2.0f * 3.14159265f;

        float x0 = cosf(a0) * radius, y0 = sinf(a0) * radius;
        float x1 = cosf(a1) * radius, y1 = sinf(a1) * radius;

        glm::vec3 cf = center + glm::vec3(0, 0,  half);
        glm::vec3 cb = center + glm::vec3(0, 0, -half);

        // Front disc - triangle from center to each edge pair
        push_vertex(v, cf,                        col);
        push_vertex(v, cf + glm::vec3(x0, y0, 0), col);
        push_vertex(v, cf + glm::vec3(x1, y1, 0), col);

        // Back disc
        push_vertex(v, cb,                        col);
        push_vertex(v, cb + glm::vec3(x1, y1, 0), col);
        push_vertex(v, cb + glm::vec3(x0, y0, 0), col);

        // Side quad connecting front and back edges
        push_quad(v,
            cf + glm::vec3(x0, y0, 0),
            cb + glm::vec3(x0, y0, 0),
            cb + glm::vec3(x1, y1, 0),
            cf + glm::vec3(x1, y1, 0),
            side);
    }
}

// push_ground_quad
void push_ground_quad(std::vector<float>& v, float half_extent){
    // flat quad at Y=0, normal pointing straight up (0,1,0)
    // vertex layout: px py pz nx ny nz cause matches the OBJ/lighting shader
    // so the ground shares the same shader as the trike with no extra code
    float S = half_extent;
    glm::vec3 n = {0.0f, 1.0f, 0.0f};  // up normal, same for all 4 verts

    // two triangles, counter clockwise winding from above
    // triangle 1:(-S,0,-S) (S,0,-S) (S,0,S)
    // triangle 2:(-S,0,-S) (S,0,S) (-S,0,S)
    auto push = [&](glm::vec3 p){
        v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
        v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    };

    push({-S, 0.0f, -S});
    push({ S, 0.0f, -S});
    push({ S, 0.0f,  S});
    push({-S, 0.0f, -S});
    push({ S, 0.0f,  S});
    push({-S, 0.0f,  S});
}

void push_axis_gizmo(std::vector<float>& v, float length){
    // x axis - red
    push_vertex(v, {0.0f,   0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    push_vertex(v, {length, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    // y axis - green
    push_vertex(v, {0.0f, 0.0f,   0.0f}, {0.0f, 1.0f, 0.0f});
    push_vertex(v, {0.0f, length, 0.0f}, {0.0f, 1.0f, 0.0f});
    // z axis - blue
    push_vertex(v, {0.0f, 0.0f, 0.0f  }, {0.0f, 0.0f, 1.0f});
    push_vertex(v, {0.0f, 0.0f, length}, {0.0f, 0.0f, 1.0f});
}