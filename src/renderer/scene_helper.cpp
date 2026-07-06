#include "scene_helper.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

/**********************************************************************
SCENE HELPER
Responsibilities
- Shared uniform upload helpers (vec3/mat4/mat3) used across
  scene_init, scene_shadow, and scene_draw
**********************************************************************/

void set_vec3(const Shader& s, const char* n, glm::vec3 v){
    glUniform3f(glGetUniformLocation(s.id, n), v.x, v.y, v.z);
}

void set_mat3(const Shader& s, const char* n, const glm::mat3& m){
    glUniformMatrix3fv(glGetUniformLocation(s.id, n), 1, GL_FALSE, glm::value_ptr(m));
}