#include "editor_input.hpp"
#include "asset_scan.hpp"
#include "raycast.hpp"
#include "input_object.hpp"
#include "input_terrain.hpp"
#include "input_road.hpp"
#include "input_ocean.hpp"
#include "input_light.hpp"
#include "input_audio.hpp"
#include "input_ambience.hpp"
#include "input_pose.hpp"
#include "const.hpp"
#include <fstream>
#include <iostream>

/**********************************************************************
EDITOR INPUT

Owns:
- Mode-switch hotkeys (H/M/O/L/Z/I/K)
- Ghost cursor raycast (shared by every mode)
- Dispatch to the active mode's input handler

Per-mode logic lives in:
  input_object.cpp   - OBJECT mode (default)
  input_terrain.cpp  - TERRAIN mode
  input_road.cpp     - ROAD mode
  input_ocean.cpp     - OCEAN mode
  input_light.cpp    - LIGHT mode
  input_audio.cpp    - AUDIO mode
  input_ambience.cpp - AMBIENCE mode
  input_pose.cpp     - POSE mode
  input_settings.cpp - settings menu (called separately from app_editor.cpp,
                        not from here, since it runs while the world is frozen)

HOW TO USE 101 - mode switching:
- H toggles TERRAIN mode
- M toggles ROAD mode
- O toggles OCEAN mode
- L toggles LIGHT mode
- Z toggles AUDIO mode (requires a selected object)
- I toggles AMBIENCE mode
- K toggles POSE mode (auto-loads the selected pedestrian, or the driver
  if none is selected)
- any of the above pressed again while already in that mode returns to
  OBJECT mode

**********************************************************************/

// key state tracking to prevent held key repeat, for mode-switch keys only
static bool s_h_last = false;
static bool s_m_last = false;

void editor_input_update(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty){

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    // update ghost pos every frame by raycasting mouse to ground
    // shared by every mode: placement, sculpting, road points, zone placement, etc
    editor.placement_valid = editor_raycast_ground(
        mx, my, view, proj, screen_w, screen_h, editor.ghost_pos,
        (map.terrain.rows > 0) ? &map.terrain : nullptr);
    
    // snap ghost to grid
    if (editor.placement_valid){
        editor.ghost_pos.x = std::round(editor.ghost_pos.x / Const::EDITOR_GRID_SNAP) * Const::EDITOR_GRID_SNAP;
        editor.ghost_pos.z = std::round(editor.ghost_pos.z / Const::EDITOR_GRID_SNAP) * Const::EDITOR_GRID_SNAP;
        editor.ghost_pos.y = heightfield_sample(map.terrain, editor.ghost_pos.x, editor.ghost_pos.z);
    }

    // P toggles paint sub-mode inside terrain mode
    static bool s_p_last = false;
    bool p_down = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (p_down && !s_p_last && editor.mode == MODE_TERRAIN){
        editor.paint_mode = !editor.paint_mode;
        er.terrain_surface_dirty = true;
        std::cout << "[editor] paint mode " << (editor.paint_mode ? "ON" : "OFF") << "\n";
    }
    s_p_last = p_down;

    // H to toggle terain sculpt mode
    bool h_down = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    bool ctrl_h = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    if (h_down && !s_h_last && !ctrl_h){
        editor.mode = (editor.mode == MODE_TERRAIN) ? MODE_OBJECT : MODE_TERRAIN;
        er.terrain_surface_dirty = true;
        std::cout << "[editor] mode -> " << (editor.mode == MODE_TERRAIN ? "TERRAIN" : "OBJECT") << "\n";
    }
    s_h_last = h_down;

    // M to toggle to raod spline mode
    bool m_down = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    bool ctrl_m = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    if (m_down && !s_m_last && !ctrl_m){
        editor.mode = (editor.mode == MODE_ROAD) ? MODE_OBJECT : MODE_ROAD;
        editor.road_placing = (editor.mode == MODE_ROAD);
        std::cout << "[editor] mode -> " << (editor.mode == MODE_ROAD ? "ROAD" : "OBJECT") << "\n";
    }
    s_m_last = m_down;

    // O to toggle ocean mode
    static bool s_o_last = false;
    bool o_down = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (o_down && !s_o_last){
        editor.mode = (editor.mode == MODE_OCEAN) ? MODE_OBJECT : MODE_OCEAN;
        std::cout << "[editor] mode -> " << (editor.mode == MODE_OCEAN ? "OCEAN" : "OBJECT") << "\n";
    }
    s_o_last = o_down;

    // L to toggle light placement mode
    static bool s_l_last = false;
    bool l_down = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (l_down && !s_l_last){
        editor.mode = (editor.mode == MODE_LIGHT) ? MODE_OBJECT : MODE_LIGHT;
        editor.selected_light_id = -1;
        std::cout << "[editor] mode -> " << (editor.mode == MODE_LIGHT ? "LIGHT" : "OBJECT") << "\n";
    }
    s_l_last = l_down;

    // Z to toggle audio editor mode
    // requires an object to be selected
    static bool s_z_last = false;
    bool z_down = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (z_down && !s_z_last){
        if (editor.mode == MODE_AUDIO){
            editor.mode = MODE_OBJECT;
            std::cout << "[editor] mode -> OBJECT\n";
        }
        else if (editor.selected_id != -1){
            editor.mode = MODE_AUDIO;
            editor.audio_slot = 0;
            editor.audio_file_page = 0;
            editor_scan_audio(editor, "../assets");
            std::cout << "[editor] mode -> AUDIO (id=" << editor.selected_id << ")\n";
        }
        else {
            std::cout << "[editor] audio mode requires a selected object\n";
        }
    }
    s_z_last = z_down;

    // I to toggle ambience editor mode
    static bool s_i_last = false;
    bool i_down = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    if (i_down && !s_i_last){
        if (editor.mode == MODE_AMBIENCE){
            editor.mode = MODE_OBJECT;
            std::cout << "[editor] mode -> OBJECT\n";
        }
        else {
            editor.mode = MODE_AMBIENCE;
            editor.selected_zone_id = -1;
            editor.ambience_placing = false;
            editor_scan_audio(editor, "../assets");
            std::cout << "[editor] mode -> AMBIENCE\n";
        }
    }
    s_i_last = i_down;

    // K to toggle pose editor mode
    static bool s_k_last = false;
    bool k_down = glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS;
    if (k_down && !s_k_last){
        if (editor.mode == MODE_POSE){
            editor.mode = MODE_OBJECT;
            // if we were editing an npc, restore driver pose seat
            // npc world pos gets written to pose_seat for hail preview
            // which corrupts the driver seat on return to drive mode
            if (editor.pose_npc_id != -1){
                std::ifstream pf("../assets/entity/driver_pose.txt");
                if (pf.is_open()){
                    std::string tag;
                    while (pf >> tag){
                        if (tag == "seat")
                            pf >> editor.pose_seat.x >> editor.pose_seat.y >> editor.pose_seat.z;
                        else if (tag == "quat"){
                            int idx; pf >> idx;
                            if (idx >= 0 && idx < 6)
                                pf >> editor.pose_quat[idx].w >> editor.pose_quat[idx].x
                                   >> editor.pose_quat[idx].y >> editor.pose_quat[idx].z;
                        }
                        else if (tag == "offset"){
                            int idx; pf >> idx;
                            if (idx >= 0 && idx < 6)
                                pf >> editor.pose_offset[idx].x >> editor.pose_offset[idx].y
                                   >> editor.pose_offset[idx].z;
                        }
                    }
                }
            }
            editor.pose_npc_id = -1;
            editor.pose_editing_hail = false;
            std::cout << "[editor] mode -> OBJECT\n";
        } 
        else {
                editor.mode = MODE_POSE;
            editor.pose_numpad_translate = false;
            for (int i = 0; i < 6; i++) editor.pose_quat[i] = glm::quat(1,0,0,0);
            for (int i = 0; i < 6; i++) editor.pose_offset[i] = glm::vec3(0.0f);
            editor.pose_seat = glm::vec3(
                Const::DRIVER_SEAT_OFFSET_X,
                Const::DRIVER_SEAT_OFFSET_Y,
                Const::DRIVER_SEAT_OFFSET_Z);

            // check if a pedestrian is selected 
            // auto-load it as pose target
            editor.pose_npc_id = -1;
            for (const auto& o : map.objects){
                if (o.id == editor.selected_id && o.behavior == PEDESTRIAN){
                    editor.pose_npc_id = o.id;
                    // load mount pose as starting point 
                    // Ctrl+H overwrites with hail if needed
                    for (int i = 0; i < 6; i++) editor.pose_quat[i] = o.npc_mount_quat[i];
                    for (int i = 0; i < 6; i++) editor.pose_offset[i] = o.npc_mount_offset[i];
                    editor.pose_seat = o.npc_mount_seat;
                    std::cout << "[pose] loaded mount pose from npc id=" << o.id << "\n";
                    break;
                }
            }

            // no npc selected 
            // fall back to driver pose file
            if (editor.pose_npc_id == -1){
                std::ifstream pf("../assets/entity/driver_pose.txt");
                if (pf.is_open()) {
                    std::string tag;
                    while (pf >> tag) {
                        if (tag == "seat") {
                            pf >> editor.pose_seat.x >> editor.pose_seat.y >> editor.pose_seat.z;
                        } 
                        else if (tag == "quat") {
                            int idx; pf >> idx;
                            if (idx >= 0 && idx < 6)
                                pf >> editor.pose_quat[idx].w >> editor.pose_quat[idx].x
                                   >> editor.pose_quat[idx].y >> editor.pose_quat[idx].z;
                        } 
                        else if (tag == "offset") {
                            int idx; pf >> idx;
                            if (idx >= 0 && idx < 6)
                                pf >> editor.pose_offset[idx].x >> editor.pose_offset[idx].y
                                   >> editor.pose_offset[idx].z;
                        }
                    }
                    std::cout << "[pose] loaded ../assets/entity/driver_pose.txt\n";
                }
            }
            std::cout << "[editor] mode -> POSE (target=" 
                      << (editor.pose_npc_id == -1 ? "driver" : "npc") << ")\n";
        }
    }
    s_k_last = k_down;

    // dispatch to the active mode's handler
    switch (editor.mode){
        case MODE_AUDIO:
            editor_input_audio(editor, map, window, dt, map_dirty);
            return;
        case MODE_AMBIENCE:
            editor_input_ambience(editor, map, window, dt, map_dirty);
            return;
        case MODE_POSE:
            editor_input_pose(editor, map, er, window, view, proj, screen_w, screen_h, dt, map_dirty);
            return;
        case MODE_TERRAIN:
            editor_input_terrain(editor, map, er, window, dt);
            return;
        case MODE_ROAD:
            editor_input_road(editor, map, window, dt);
            return;
        case MODE_OCEAN:
            editor_input_ocean(editor, map, er, window, dt);
            return;
        case MODE_LIGHT:
            editor_input_light(editor, map, window, view, proj, screen_w, screen_h, dt, map_dirty);
            return;
        default:
            editor_input_object(editor, map, er, window, view, proj, screen_w, screen_h, dt, map_dirty);
            return;
    }
}