#include "input_pose.hpp"
#include "const.hpp"
#include "raycast.hpp"
#include "../world/npc.hpp"
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <cmath>
#include <iostream>

/**********************************************************************
INPUT POSE
Responsibilities
- Bone cycling + rotation
- Seat / bone-offset translation
- Hail / mount pose target switching
- Save hail pose / mount pose (NPC) or driver pose (file)
- Dump pose as pasteable code

How to 101 in case you struggle in operating in the program:
- K keu to enter POSE MODE (auto-loads the selected pedestrian if one
  was selected, otherwise edits the driver)
- F cycles which bone you're editing: TORSO, HEAD, LEG_L, LEG_R, ARM_L, ARM_R
- arrow keys rotate the bone, PgUp/PgDn rotates it on the third axis
- hold SHIFT for slower, finer rotation
- numpad 0 toggles between moving the SEAT and moving the active BONE
  - numpad 8/2 = forward/back, 4/6 = left/right, +/- = up/down
- LMB click a different pedestrian to switch who you're posing
- V toggles between editing the HAIL pose and the MOUNT pose (NPCs only)
- Ctrl+H saves the current pose as that NPC's hail pose
- Ctrl+M saves the current pose as that NPC's mount pose
- Ctrl+S saves the driver's pose to file (driver only, not NPCs)
- ENTER prints the pose as code in the console, for pasting into
  driver_anim.cpp / const.hpp
**********************************************************************/

void editor_input_pose(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty){

    static bool s_lmb_last = false;
    static bool s_pose_f_last = false;
    static bool s_pose_v_last = false;
    static bool s_kp0_last = false;
    static bool s_pose_enter_last = false;
    static bool s_pose_h_last = false;
    static bool s_pose_m_last = false;
    static bool s_pose_s_last = false;

    // LMB: click a pedestrian to switch pose target mid-session
    bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (lmb && !s_lmb_last){
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        int hit = editor_raycast_objects(mx, my, view, proj, screen_w, screen_h, map, er, false);
        if (hit != -1){
            for (const auto& o : map.objects){
                if (o.id != hit || o.behavior != PEDESTRIAN) continue;
                editor.pose_npc_id = o.id;
                editor.selected_id = o.id;
                for (int i = 0; i < 6; i++) editor.pose_quat[i] = o.npc_mount_quat[i];
                for (int i = 0; i < 6; i++) editor.pose_offset[i] = o.npc_mount_offset[i];
                editor.pose_seat = o.npc_mount_seat;
                std::cout << "[pose] switched to npc id=" << o.id << "\n";
                break;
            }
        }
    }
    s_lmb_last = lmb;

    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    float rot_speed = glm::radians(60.0f * dt);
    if (shift) rot_speed *= 0.1f;

    // F cycles active bone
    bool f_down = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (f_down && !s_pose_f_last){
        editor.pose_bone = (editor.pose_bone + 1) % 6;
        static const char* bone_names[6] = {
            "TORSO", "HEAD", "LEG_L", "LEG_R", "ARM_L", "ARM_R"
        };
        std::cout << "[pose] bone -> " << bone_names[editor.pose_bone] << "\n";
    }
    s_pose_f_last = f_down;

    // V toggles between hail and mount pose editing
    bool v_down = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
    if (v_down && !s_pose_v_last && editor.pose_npc_id != -1){
        editor.pose_editing_hail = !editor.pose_editing_hail;
        // load the target pose so you start from existing saved data
        for (const auto& o : map.objects){
            if (o.id != editor.pose_npc_id) continue;
            if (editor.pose_editing_hail){
                // if no hail pose saved yet, start from identity (standing)
                // seat at NPC's editor position so it renders in place
                bool hail_empty = (glm::length(o.npc_hail_seat) < 0.001f);
                if (hail_empty){
                    for (int i = 0; i < 6; i++) editor.pose_quat[i]   = glm::quat(1,0,0,0);
                    for (int i = 0; i < 6; i++) editor.pose_offset[i] = glm::vec3(0.0f);
                    editor.pose_seat = o.position;
                    std::cout << "[pose] HAIL pose empty, starting from standing at editor pos\n";
                } 
                else {
                    for (int i = 0; i < 6; i++) editor.pose_quat[i] = o.npc_hail_quat[i];
                    for (int i = 0; i < 6; i++) editor.pose_offset[i] = o.npc_hail_offset[i];
                    editor.pose_seat = o.npc_hail_seat;
                }
                std::cout << "[pose] switched to HAIL pose\n";
            } 
            else {
                bool mount_empty = (glm::length(o.npc_mount_seat) < 0.001f);
                if (mount_empty){
                    for (int i = 0; i < 6; i++) editor.pose_quat[i]   = glm::quat(1,0,0,0);
                    for (int i = 0; i < 6; i++) editor.pose_offset[i] = glm::vec3(0.0f);
                    editor.pose_seat = o.position;
                    std::cout << "[pose] MOUNT pose empty, starting from standing at editor pos\n";
                } 
                else {
                    for (int i = 0; i < 6; i++) editor.pose_quat[i]   = o.npc_mount_quat[i];
                    for (int i = 0; i < 6; i++) editor.pose_offset[i] = o.npc_mount_offset[i];
                    editor.pose_seat = o.npc_mount_seat;
                }
                std::cout << "[pose] switched to MOUNT pose\n";
            }
            break;
        }
    }
    s_pose_v_last = v_down;

    glm::quat& q = editor.pose_quat[editor.pose_bone];

    // incremental rotation in bone local space — no gimbal
    // each press rotates around a fixed local axis
    // left/right = Y (twist), up/down = X (bend fwd/back), pgup/dn = Z (lean)
    if (!editor.pose_numpad_translate) {
        if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS)
            q = glm::angleAxis(-rot_speed, glm::vec3(1,0,0)) * q;
        if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS)
            q = glm::angleAxis( rot_speed, glm::vec3(1,0,0)) * q;
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS)
            q = glm::angleAxis(-rot_speed, glm::vec3(0,1,0)) * q;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            q = glm::angleAxis( rot_speed, glm::vec3(0,1,0)) * q;
        if (glfwGetKey(window, GLFW_KEY_PAGE_UP)   == GLFW_PRESS)
            q = glm::angleAxis( rot_speed, glm::vec3(0,0,1)) * q;
        if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
            q = glm::angleAxis(-rot_speed, glm::vec3(0,0,1)) * q;
    }

    // numpad 0 toggles between seat nudge and bone translate
    bool kp0 = glfwGetKey(window, GLFW_KEY_KP_0) == GLFW_PRESS;
    if (kp0 && !s_kp0_last){
        editor.pose_numpad_translate = !editor.pose_numpad_translate;
        std::cout << "[pose] numpad -> "
                  << (editor.pose_numpad_translate ? "BONE TRANSLATE" : "SEAT") << "\n";
    }
    s_kp0_last = kp0;

    float nudge_speed = 8.0f * dt;
    if (shift) nudge_speed *= 0.1f;

    if (editor.pose_numpad_translate){
        // bone translate mode: move active bone mesh in model space
        glm::vec3& off = editor.pose_offset[editor.pose_bone];
        if (glfwGetKey(window, GLFW_KEY_KP_8) == GLFW_PRESS) off.z -= nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_2) == GLFW_PRESS) off.z += nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS) off.x -= nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS) off.x += nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) off.y += nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) off.y -= nudge_speed;
    } 
    else {
        // seat mode: move entire driver
        if (glfwGetKey(window, GLFW_KEY_KP_8) == GLFW_PRESS) editor.pose_seat.z -= nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_2) == GLFW_PRESS) editor.pose_seat.z += nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS) editor.pose_seat.x -= nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS) editor.pose_seat.x += nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) editor.pose_seat.y += nudge_speed;
        if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) editor.pose_seat.y -= nudge_speed;
    }

    // Enter: dump as axis-angle, paste into driver_anim.cpp
    bool enter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    if (enter && !s_pose_enter_last){
        static const char* bone_names[6] = {
            "BONE_TORSO", "BONE_HEAD", "BONE_LEG_L", "BONE_LEG_R", "BONE_ARM_L", "BONE_ARM_R"
        };
        std::cout << "\n// --- paste into pose_sit() in driver_anim.cpp ---\n";
        for (int i = 0; i < 6; i++){
            glm::quat& bq = editor.pose_quat[i];
            // skip identity bones
            if (std::abs(bq.w - 1.0f) < 0.001f) continue;
            float angle = 2.0f * std::acos(glm::clamp(bq.w, -1.0f, 1.0f));
            float s = std::sqrt(1.0f - bq.w * bq.w);
            glm::vec3 axis = (s > 0.001f)
                ? glm::vec3(bq.x/s, bq.y/s, bq.z/s)
                : glm::vec3(1,0,0);
            std::cout << "pose.local[" << bone_names[i] << "] = "
                      << "glm::rotate(pose.local[" << bone_names[i] << "], "
                      << angle << "f, glm::vec3("
                      << axis.x << "f, " << axis.y << "f, " << axis.z << "f));\n";
        }
       // bone offsets
        for (int i = 0; i < 6; i++){
            glm::vec3& off = editor.pose_offset[i];
            if (glm::length(off) < 0.001f) continue;
            std::cout << "// " << bone_names[i] << " offset\n";
            std::cout << "bone_local = glm::translate(bone_local, glm::vec3("
                      << off.x << "f, " << off.y << "f, " << off.z << "f));\n";
        }
        std::cout << "\n// --- paste into const.hpp ---\n";
        std::cout << "inline constexpr float DRIVER_SEAT_OFFSET_X = "
                  << editor.pose_seat.x << "f;\n";
        std::cout << "inline constexpr float DRIVER_SEAT_OFFSET_Y = "
                  << editor.pose_seat.y << "f;\n";
        std::cout << "inline constexpr float DRIVER_SEAT_OFFSET_Z = "
                  << editor.pose_seat.z << "f;\n\n";
    }
    s_pose_enter_last = enter;

    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

    // Ctrl+H  save current pose as hail pose for selected NPC
    bool h_down = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    if (ctrl && h_down && !s_pose_h_last && editor.pose_npc_id != -1){
        for (auto& o : map.objects){
            if (o.id != editor.pose_npc_id || o.behavior != PEDESTRIAN) continue;
            for (int i = 0; i < 6; i++){
                o.npc_hail_quat[i] = editor.pose_quat[i];
                o.npc_hail_offset[i] = editor.pose_offset[i];
            }
            o.npc_hail_seat = editor.pose_seat;
            map_dirty = true;
            std::cout << "[pose] hail pose saved to npc id=" << o.id << "\n";
            break;
        }
    }
    s_pose_h_last = h_down;

    // Ctrl+M save current pose as mount/passenger pose for selected NPC
    bool pose_m_down = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (ctrl && pose_m_down && !s_pose_m_last && editor.pose_npc_id != -1){
        for (auto& o : map.objects){
            if (o.id != editor.pose_npc_id || o.behavior != PEDESTRIAN) continue;
            for (int i = 0; i < 6; i++){
                o.npc_mount_quat[i]   = editor.pose_quat[i];
                o.npc_mount_offset[i] = editor.pose_offset[i];
            }
            o.npc_mount_seat = editor.pose_seat;
            map_dirty = true;
            std::cout << "[pose] mount pose saved to npc id=" << o.id << "\n";
            break;
        }
    }
    s_pose_m_last = pose_m_down;

    // Ctrl+S saves driver pose to file
    bool s_down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    if (ctrl && s_down && !s_pose_s_last && editor.pose_npc_id == -1) {
        std::ofstream pf("../assets/entity/driver_pose.txt");
        if (pf.is_open()) {
            pf << "seat " << editor.pose_seat.x << " "
               << editor.pose_seat.y << " " << editor.pose_seat.z << "\n";
            for (int i = 0; i < 6; i++)
                pf << "quat " << i << " "
                   << editor.pose_quat[i].w << " " << editor.pose_quat[i].x << " "
                   << editor.pose_quat[i].y << " " << editor.pose_quat[i].z << "\n";
            for (int i = 0; i < 6; i++)
                pf << "offset " << i << " "
                   << editor.pose_offset[i].x << " "
                   << editor.pose_offset[i].y << " "
                   << editor.pose_offset[i].z << "\n";
            std::cout << "[pose] saved ../assets/entity/driver_pose.txt\n";
        }
    }
    s_pose_s_last = s_down;
}