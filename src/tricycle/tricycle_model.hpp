#pragma once
#include "../renderer/shader.hpp"
#include "../renderer/obj_mesh.hpp"

struct TrikeModel {
    ObjMesh mesh;
};

void trike_model_init(TrikeModel& t);
void trike_model_draw(const TrikeModel& t, const Shader& shader);
void trike_model_destroy(TrikeModel& t);