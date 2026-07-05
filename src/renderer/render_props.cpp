#include "render_props.hpp"
#include "render_helpers.hpp"
#include "render_textures.hpp"
#include "../core/const.hpp"
#include "../core/settings.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/**********************************************************************
RENDER PROPS
Responsibilities
- Placed prop mesh rendering (static/dynamic/decoration/pedestrian)
- Point light culling + upload for props
- Hit-flash color blending
- Shadow depth pass for props
**********************************************************************/

void editor_renderer_shadow_pass(EditorRenderer& er, const WorldMap& map,
    const glm::mat4& light_space_mat,
    const std::unordered_map<int, DynamicSim>& dynamic_sims){
    shader_bind(er.depth_shader);
    glUniformMatrix4fv(er.depth_loc.light_space, 1, GL_FALSE, glm::value_ptr(light_space_mat));
    for (auto& o : map.objects){
        if (o.model_path.empty()) continue;
        if (o.behavior == PEDESTRIAN) continue; // drawn separately as NPC with live position

        // shadow cull
        glm::vec3 diff = o.position - er.shadow_cull_center;
        float shadow_cull_sq = my_settings.prop_cull_dist * my_settings.prop_cull_dist;
        if (glm::dot(diff, diff) > shadow_cull_sq) continue;
        ObjMesh& mesh = get_prop_mesh(er, o.model_path);
        if (mesh.data.vertices.empty()) continue;

        glm::mat4 model = glm::mat4(1.0f);
        auto dit = dynamic_sims.find(o.id);
        if (o.behavior == DYNAMIC && dit != dynamic_sims.end()){
            const DynamicSim& sim = dit->second;
            model = glm::translate(model, sim.position);
            model = glm::rotate(model, sim.yaw + o.rotation.y, glm::vec3(0,1,0));
            model = glm::rotate(model, sim.pitch, glm::vec3(1,0,0));
            model = glm::rotate(model, sim.roll, glm::vec3(0,0,1));
            model = glm::translate(model, glm::vec3(0.0f, o.y_floor_offset, 0.0f));
            model = glm::scale(model, o.scale);
        } 
        else {
            model = glm::translate(model, o.position);
            model = glm::rotate(model, o.rotation.y, glm::vec3(0,1,0));
            model = glm::translate(model, glm::vec3(0.0f, o.y_floor_offset, 0.0f));
            model = glm::scale(model, o.scale);
        }

        glUniformMatrix4fv(er.depth_loc.model, 1, GL_FALSE, glm::value_ptr(model));

        // draw all groups 
        // depth only, no material needed
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.data.vertices.size() / 8);
        glBindVertexArray(0);
    }
}

void editor_renderer_draw_props(EditorRenderer& er, const WorldMap& map,
    const glm::mat4& view, const glm::mat4& proj,
    const std::map<int,float>& flash_map,
    const std::unordered_map<int, DynamicSim>& dynamic_sims,
    const std::vector<LightSource>& lights,
    bool skip_pedestrians){

    glm::vec3 LIGHT_DIR = glm::normalize(er.sun_dir);

    auto& OL = er.obj_loc;
    shader_bind(er.obj_shader);
    glUniformMatrix4fv(OL.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(OL.proj, 1, GL_FALSE, glm::value_ptr(proj));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform3f(OL.light_dir, LIGHT_DIR.x, LIGHT_DIR.y, LIGHT_DIR.z);
    glUniformMatrix4fv(OL.light_space, 1, GL_FALSE, glm::value_ptr(er.light_space_mat));
    glUniform3f(OL.fog_color, er.fog_color.r, er.fog_color.g, er.fog_color.b);
    glUniform1f(OL.fog_near, my_settings.render_fog ? er.fog_near : Const::CAM_FAR);
    glUniform1f(OL.fog_far,  my_settings.render_fog ? er.fog_far : Const::CAM_FAR + 1.0f);
    glUniform1f(OL.ambient, er.ambient);
    glUniform1f(OL.diff_intensity, er.diff_intensity);
    glUniform3f(OL.light_color, er.light_color.r, er.light_color.g, er.light_color.b);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, er.shadow_depth_tex);
    glUniform1i(OL.shadow_map, 1);
    glActiveTexture(GL_TEXTURE0);

    // cpu cull: only upload lights within draw distance of camera
    glm::vec3 cam_pos = glm::vec3(glm::inverse(view)[3]);
    int active_lcount = 0;
    glUniform3f(OL.fog_cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    if (er.night_factor >= 0.01f){
        for (int i = 0; i < (int)lights.size() && active_lcount < Const::MAX_POINT_LIGHTS; i++){
            glm::vec3 d = lights[i].position - cam_pos;
            float light_cull_sq = my_settings.light_cull_dist * my_settings.light_cull_dist;
            if (glm::dot(d, d) > light_cull_sq) continue;
            // compact into slots 0..active_lcount
            glUniform3f(er.pt_light_loc.pos[active_lcount], lights[i].position.x, lights[i].position.y, lights[i].position.z);
            glUniform3f(er.pt_light_loc.color[active_lcount], lights[i].color.r, lights[i].color.g, lights[i].color.b);
            glUniform1f(er.pt_light_loc.radius[active_lcount], lights[i].radius);
            glUniform1f(er.pt_light_loc.intensity[active_lcount], lights[i].intensity * er.night_factor);
            glUniform3f(er.pt_light_loc.spot_dir[active_lcount], lights[i].spot_dir.x, lights[i].spot_dir.y, lights[i].spot_dir.z);
            glUniform1f(er.pt_light_loc.cos_cutoff[active_lcount], lights[i].cos_cutoff);
            active_lcount++;
        }
    }
    glUniform1i(er.pt_light_loc.count, active_lcount);
 
    // extract camera position from inverse view matrix
    // view is already passed in so no extra cost
    er.last_lights = lights;

    for (auto& o : map.objects){
        if (o.model_path.empty()) continue;
        if (skip_pedestrians && o.behavior == PEDESTRIAN) continue;
        if (o.behavior == PEDESTRIAN && o.id == er.pose_npc_id) continue;
        
        // distance cull
        glm::vec3 diff = o.position - cam_pos;
        float prop_cull_sq = my_settings.prop_cull_dist * my_settings.prop_cull_dist;
        if (glm::dot(diff, diff) > prop_cull_sq) continue;
        ObjMesh& mesh = get_prop_mesh(er, o.model_path);
        if (mesh.data.vertices.empty()) continue;

        glm::mat4 model = glm::mat4(1.0f);

        auto dit = dynamic_sims.find(o.id);


        if (o.behavior == DYNAMIC && dit != dynamic_sims.end()){
            // render from simulated transform = tipping, sliding, spinning
            const DynamicSim& sim = dit->second;
            model = glm::translate(model, sim.position);
            model = glm::rotate(model, sim.yaw + o.rotation.y, glm::vec3(0,1,0));
            model = glm::rotate(model, sim.pitch, glm::vec3(1,0,0));
            model = glm::rotate(model, sim.roll, glm::vec3(0,0,1));
            model = glm::translate(model, glm::vec3(0.0f, o.y_floor_offset, 0.0f));
            model = glm::scale(model, o.scale);
        } 
        else {
            // static/decoration/pedestrian = placed transform
            model = glm::translate(model, o.position);
            model = glm::rotate(model, o.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::translate(model, glm::vec3(0.0f, o.y_floor_offset, 0.0f));
            model = glm::scale(model, o.scale);
        }

        // upper-left 3x3 of model is correct normal mat for uniform scale
        // skips the expensive inverse+transpose
        glm::mat3 normal_mat = glm::mat3(model);

        glUniformMatrix4fv(OL.model,      1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix3fv(OL.normal_mat, 1, GL_FALSE, glm::value_ptr(normal_mat));

        // check if this object is flashing from a hit
        float flash = 0.0f;
        auto fit = flash_map.find(o.id);
        if (fit != flash_map.end())
            flash = glm::clamp(fit->second / 0.35f, 0.0f, 1.0f);

        for (int i = 0; i < (int)mesh.data.groups.size(); i++){
            const ObjGroup& grp = mesh.data.groups[i];
            const ObjMaterial* mat = obj_find_material(mesh.data, grp.mat_name);

            glm::vec3 kd = mat ? mat->kd : glm::vec3(0.8f);
            glm::vec3 hit_color = {0.9f, 0.15f, 0.10f};
            kd = glm::mix(kd, hit_color, flash);
            glUniform3f(OL.kd, kd.r, kd.g, kd.b);

            // bind texture if material has one, else flat color
            GLuint tex = (mat && !mat->tex_path.empty())
                         ? load_texture(er, mat->tex_path) : 0;
            if (tex){
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                glUniform1i(OL.tex, 0);
                glUniform1i(OL.use_texture, 1);
            } 
            else {
                glUniform1i(OL.use_texture, 0);
            }

            obj_mesh_draw_group(mesh, i);

            if (tex) glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    glDisable(GL_BLEND);
}