#include "render_pose.hpp"
#include "../world/npc.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

/**********************************************************************
RENDER POSE
Responsibilities
- Isolated pose-editing view (reference trike + driver or NPC)
- Hail/mount pose preview via scratch NpcState
**********************************************************************/

void editor_renderer_draw_pose_mode(EditorRenderer& er, const EditorState& editor,
    const DriverModel& driver, const TrikeModel& trike,
    const glm::mat4& view, const glm::mat4& proj,
    const DriverModel* npc_model, const WorldMap& map)
{
    auto& OL = er.obj_loc;
    glm::vec3 ld = glm::normalize(er.sun_dir);
    shader_bind(er.obj_shader);
    glUniformMatrix4fv(OL.view,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(OL.proj,  1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(OL.light_dir, ld.x, ld.y, ld.z);
    glUniform3f(OL.light_color, er.light_color.r, er.light_color.g, er.light_color.b);
    glUniform1f(OL.ambient, er.ambient);
    glUniform1f(OL.diff_intensity, er.diff_intensity);
    glUniformMatrix4fv(OL.light_space, 1, GL_FALSE, glm::value_ptr(er.light_space_mat));
    glUniform1f(OL.shadow_bias, Const::SHADOW_BIAS);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, er.shadow_depth_tex);
    glUniform1i(OL.shadow_map, 1);
    glUniform1i(OL.use_texture, 0);
    glUniform1i(er.pt_light_loc.count, 0);
    glActiveTexture(GL_TEXTURE0);

    TrikeState dummy_trike;
    memset(&dummy_trike, 0, sizeof(dummy_trike));

    // trike always renders as reference 
    // same origin regardless of npc or driver
    trike_model_draw(trike, dummy_trike, er.obj_shader, view, proj);

    if (editor.pose_npc_id != -1 && npc_model){
        // build a scratch NpcState that holds the current editor pose
        // position at world origin so it renders relative to the trike reference
        NpcState scratch;
        scratch.position = glm::vec3(0.0f);
        scratch.yaw = 0.0f;

        // find the WorldObject for this NPC to grab its editor scale
        // without this, pose mode always renders at scale 1
        scratch.editor_scale = glm::vec3(1.0f);
        for (const auto& o : map.objects){
            if (o.id == editor.pose_npc_id){
                scratch.editor_scale = o.scale;
                break;
            }
        }
         scratch.mode = editor.pose_editing_hail ? NPC_HAILING : NPC_PASSENGER;
        // hail pose: render at world position, not trike-relative origin
        if (editor.pose_editing_hail)
            scratch.position = editor.pose_seat;

        // load whichever pose is being edited into the scratch state
        // Ctrl+H edits hail, Ctrl+M edits mount we mirror that here
        for (int i = 0; i < 6; i++){
            scratch.hail_pose_quat[i]    = editor.pose_quat[i];
            scratch.hail_pose_offset[i]  = editor.pose_offset[i];
            scratch.mount_pose_quat[i]   = editor.pose_quat[i];
            scratch.mount_pose_offset[i] = editor.pose_offset[i];
        }
        scratch.hail_pose_seat  = editor.pose_seat;
        scratch.mount_pose_seat = editor.pose_seat;
        

        npc_draw(scratch, *npc_model, er.obj_shader, view, proj);
    }
    else {
        // driver pose
        driver_model_draw_pose(
            driver,
            editor.pose_seat,
            editor.pose_quat,
            editor.pose_offset,
            editor.pose_bone,
            er.obj_shader,
            view,
            proj);
    }
}