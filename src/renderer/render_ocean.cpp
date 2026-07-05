#include "render_ocean.hpp"
#include "../core/settings.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/**********************************************************************
RENDER OCEAN
Responsibilities
- Ocean mesh rebuild trigger + time accumulation
- Planar reflection + normal/foam texture binding
- Ocean surface draw with alpha blending
**********************************************************************/

void editor_renderer_draw_ocean(EditorRenderer& er, Ocean& ocean, const HeightField& hf,
    const glm::mat4& view, const glm::mat4& proj, float dt,
    float terrain_x_min, float terrain_x_max, float terrain_z_min, float terrain_z_max){
    if (!ocean.enabled) return;
    if (ocean.mesh_dirty)
        ocean_build_mesh(ocean, hf, terrain_x_min, terrain_x_max, terrain_z_min, terrain_z_max);

    ocean.time += dt;

    glm::vec3 cam_pos = glm::vec3(glm::inverse(view)[3]);
    glm::vec3 light_dir = glm::normalize(er.sun_dir);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    auto& OL = er.ocean_loc;
    shader_bind(er.ocean_shader);
    glUniformMatrix4fv(OL.view,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(OL.proj,  1, GL_FALSE, glm::value_ptr(proj));
    glUniform1f(OL.time, ocean.time);
    glUniform1f(OL.y_level, ocean.y_level);
    glUniform3f(OL.light_dir, light_dir.x, light_dir.y, light_dir.z);
    glUniform3f(OL.cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(OL.light_color, er.light_color.r, er.light_color.g, er.light_color.b);
    glUniform1f(OL.ambient, er.ambient);
    glUniform1f(OL.diff_intensity, er.diff_intensity);
    glUniform1f(OL.max_depth, ocean.max_depth);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, er.reflect_tex);
    glUniform1i(OL.reflect_tex, 0);
    glUniformMatrix4fv(OL.refl_view_proj, 1, GL_FALSE, glm::value_ptr(er.reflect_view_proj));

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, er.ocean_normal_tex);
    glUniform1i(OL.normal_tex, 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, er.ocean_foam_tex);
    glUniform1i(OL.foam_tex, 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, er.ocean_foam_shore_tex);
    glUniform1i(OL.foam_shore_tex, 4);

    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (ocean.mesh.vao && ocean.mesh.count > 0){
        glBindVertexArray(ocean.mesh.vao);
        glDrawElements(GL_TRIANGLES, ocean.mesh.count, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}