#include "render_road.hpp"
#include "render_helpers.hpp"
#include "render_textures.hpp"
#include "../core/const.hpp"
#include "../core/settings.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <string>

/**********************************************************************
RENDER ROAD
Responsibilities
- Road spline mesh rendering, textured by road type
- Fallback flat color when texture missing
**********************************************************************/

void editor_renderer_draw_roads(EditorRenderer& er, const std::vector<RoadSpline>& roads,
    const glm::mat4& view, const glm::mat4& proj){
    if (roads.empty()) return;

    glm::vec3 LIGHT_DIR = glm::normalize(er.sun_dir);

    // road type colors
    // used when no texture is present
    static const glm::vec3 ROAD_COLORS[ROAD_COUNT] = {
        {0.20f, 0.20f, 0.20f}, // asphalt
        {0.55f, 0.48f, 0.38f}, // gravel
        {0.45f, 0.32f, 0.18f}, // dirt
        {0.85f, 0.78f, 0.55f}, // sand
        {0.25f, 0.55f, 0.18f}, // grass
        {0.70f, 0.70f, 0.68f}, // cement
        {0.90f, 0.88f, 0.60f}, // road_lines
    };

    /*
    ROAD SPLINE TEX NAMES ADD _COLOR.JPEG FILES IN ASSETS/
    AND CHANGE IT BASED ON NAME DEFS HERE
    */

     static const char* ROAD_TEX_NAMES[ROAD_COUNT] = {
        "asphalt.jpg",
        "gravel.jpg",
        "dirt.jpg",
        "sand.jpg",
        "grass.jpg",
        "cement.jpg",
        "road_lines.jpg",
    };

    auto& RL = er.road_loc;
    glm::mat4 identity = glm::mat4(1.0f);
    glm::mat3 nm = glm::mat3(1.0f);
    shader_bind(er.road_shader);
    glUniformMatrix4fv(RL.model, 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(RL.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(RL.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix3fv(RL.normal_mat, 1, GL_FALSE, glm::value_ptr(nm));
    glUniform3f(RL.light_dir, LIGHT_DIR.x, LIGHT_DIR.y, LIGHT_DIR.z);
    glUniformMatrix4fv(RL.light_space, 1, GL_FALSE, glm::value_ptr(er.light_space_mat));
    glUniform1f(RL.shadow_bias, Const::SHADOW_BIAS);
    glUniform1f(RL.ambient, er.ambient);
    glUniform1f(RL.diff_intensity, er.diff_intensity);
    glUniform3f(RL.light_color, er.light_color.r, er.light_color.g, er.light_color.b);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, er.shadow_depth_tex);
    glUniform1i(RL.shadow_map, 1);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(RL.tex, 0);

    glm::vec3 cam_pos_r = glm::vec3(glm::inverse(view)[3]);
    glUniform3f(RL.fog_color, er.fog_color.r,  er.fog_color.g,  er.fog_color.b);
    glUniform1f(RL.fog_near, my_settings.render_fog ? er.fog_near : Const::CAM_FAR);
    glUniform1f(RL.fog_far,  my_settings.render_fog ? er.fog_far : Const::CAM_FAR + 1.0f);
    glUniform3f(RL.fog_cam_pos, cam_pos_r.x, cam_pos_r.y, cam_pos_r.z);
    upload_point_lights(er.road_shader.id, er.last_lights, cam_pos_r, er.night_factor);

    for (const auto& road : roads){
        if (road.vao == 0 || road.index_count == 0) continue;

        int type_idx = glm::clamp((int)road.type, 0, (int)ROAD_COUNT - 1);

        std::string tex_path = std::string("../assets/props/") + ROAD_TEX_NAMES[type_idx];
        GLuint tex = load_texture(er, tex_path);

        if (tex){
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(RL.use_texture, 1);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1i(RL.use_texture, 0);
            glm::vec3 kd = ROAD_COLORS[type_idx];
            glUniform3f(RL.kd, kd.r, kd.g, kd.b);
        }

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset((road.type == ROAD_LINES) ? -2.0f : -1.0f, -1.0f);
        glBindVertexArray(road.vao);
        glDrawElements(GL_TRIANGLES, road.index_count, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}