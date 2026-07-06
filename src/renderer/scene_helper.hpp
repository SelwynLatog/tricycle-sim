#pragma once
#include "shader.hpp"
#include <glm/glm.hpp>

// uploads a vec3 uniform by name to the currently bound shader
void set_vec3(const Shader& s, const char* n, glm::vec3 v);

// uploads a mat4 uniform by name to the currently bound shader
void set_mat4(const Shader& s, const char* n, const glm::mat4& m);

// NOTE: set_mat4 lives in render_helpers.hpp to avoid duplicate defs