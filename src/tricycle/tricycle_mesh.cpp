#include "tricycle_mesh.hpp"
#include "../renderer/mesh_builder.hpp"
#include <glm/glm.hpp>

// quick mesh rendering test
// its so ass I will use blender for the cab model wip

// test colors
static constexpr glm::vec3 YELLOW  = {0.95f, 0.75f, 0.05f};
static constexpr glm::vec3 BLACK   = {0.08f, 0.08f, 0.08f};
static constexpr glm::vec3 SILVER  = {0.75f, 0.75f, 0.78f};
static constexpr glm::vec3 DGREY   = {0.20f, 0.20f, 0.22f};
static constexpr glm::vec3 BROWN   = {0.25f, 0.15f, 0.08f};
static constexpr glm::vec3 CHROME  = {0.85f, 0.87f, 0.90f};
static constexpr glm::vec3 RED     = {0.75f, 0.10f, 0.08f};
static constexpr glm::vec3 WHITE   = {0.90f, 0.90f, 0.90f};

void build_tricycle_mesh(std::vector<float>& v){
    // cords
    // +X= forward (direction of travel)
    // +Y= up
    // +Z= left (toward sidecar)
    // Origin = rear axle of motorcycle, ground level
    //
    // Vehicle layout (top view):
    //   sidecar occupies Z= +0.35 to +1.45
    //   motorcycle occupies Z= -0.15 to +0.25
    //   length runs X= -0.1 to +1.9

    
    //sidecar

    // main cab body - large yellow box
    push_box(v, {0.80f, 0.30f,  0.90f}, {1.60f, 0.85f, 1.00f}, YELLOW);

    // roof - wider and longer than the body, overhangs on all sides
    push_box(v, {0.80f, 1.16f,  0.90f}, {1.75f, 0.09f, 1.12f}, YELLOW);

    // roof visor front overhang lip in roof
    push_box(v, {1.60f, 1.10f,  0.90f}, {0.12f, 0.08f, 1.05f}, WHITE);

    // front windshield
    push_box(v, {1.55f, 0.95f,  0.90f}, {0.06f, 0.20f, 0.90f}, DGREY);

    // left wall
    push_box(v, {0.80f, 0.75f,  1.42f}, {1.50f, 0.06f, 0.06f}, DGREY);

    // rear wall closed panel accent
    push_box(v, {0.02f, 0.72f,  0.90f}, {0.06f, 0.50f, 0.90f}, DGREY);

    // interior floor visible through open front
    push_box(v, {0.80f, 0.30f,  0.90f}, {1.50f, 0.04f, 0.95f}, DGREY);

    // passenger seat (bench across the back interior)
    push_box(v, {0.25f, 0.52f,  0.90f}, {0.40f, 0.14f, 0.80f}, BROWN);

    // sidecar wheel + fender
    push_cylinder(v, {0.10f, 0.28f,  1.38f}, 0.28f, 0.10f, 16, BLACK);
    push_cylinder(v, {0.10f, 0.28f,  1.38f}, 0.09f, 0.13f, 12, SILVER);
    // fender over sidecar wheel
    push_box(v, {0.10f, 0.52f,  1.38f}, {0.55f, 0.10f, 0.42f}, YELLOW);

    // sidecar connection arm (links cab to motorcycle frame)
    push_box(v, {0.80f, 0.38f,  0.28f}, {1.40f, 0.07f, 0.20f}, DGREY);
    push_box(v, {0.80f, 0.22f,  0.28f}, {1.40f, 0.07f, 0.20f}, DGREY);

    // motorcycle

    // Main frame spine
    push_box(v, {0.80f, 0.38f,  0.00f}, {1.70f, 0.10f, 0.16f}, DGREY);

    // engine block
    push_box(v, {0.72f, 0.16f,  0.00f}, {0.38f, 0.30f, 0.20f}, RED);

    // engine casing
    push_box(v, {0.72f, 0.16f,  0.00f}, {0.44f, 0.32f, 0.24f}, DGREY);

    // fuel tank
    push_box(v, {1.00f, 0.44f,  0.00f}, {0.45f, 0.16f, 0.18f}, YELLOW);

    // rider seat
    push_box(v, {0.72f, 0.50f,  0.00f}, {0.55f, 0.09f, 0.17f}, BROWN);

    // front fork
    push_box(v, {1.72f, 0.26f,  0.00f}, {0.09f, 0.44f, 0.08f}, SILVER);

    // handlebar
    push_box(v, {1.66f, 0.62f,  0.00f}, {0.10f, 0.06f, 0.52f}, CHROME);

    // exhaust pipe (runs along right side)
    push_box(v, {0.38f, 0.16f, -0.14f}, {0.80f, 0.05f, 0.05f}, SILVER);

    // motorcycle wheels
    // Rear wheel
    push_cylinder(v, {0.00f, 0.28f,  0.00f}, 0.28f, 0.12f, 16, BLACK);
    push_cylinder(v, {0.00f, 0.28f,  0.00f}, 0.09f, 0.15f, 12, SILVER);

    // front wheel
    push_cylinder(v, {1.80f, 0.28f,  0.00f}, 0.28f, 0.09f, 16, BLACK);
    push_cylinder(v, {1.80f, 0.28f,  0.00f}, 0.09f, 0.12f, 12, SILVER);

    // headlight
    push_box(v, {1.84f, 0.44f,  0.00f}, {0.08f, 0.12f, 0.14f}, WHITE);
}