#include "input_ambience.hpp"
#include "const.hpp"
#include "map_manager.hpp"
#include <cstring>
#include <algorithm>

/**********************************************************************
INPUT AMBIENCE
Responsibilities
- Ambience zone placement + selection
- Radius adjustment
- Type toggle (proximity / night-only)
- Audio file assignment
- Delete
- Save

How to 101 in case you struggle in operating in the program:
- I key to enter AMBIENCE MODE
- LMB places a new zone, or selects an existing one if you click near its center
- [ / ] shrinks/grows the zone's radius
- F toggles the zone between PROXIMITY and NIGHT
- UP/DOWN scrolls the audio file list
- number keys 1-8 assign the highlighted file to the zone
- DEL deletes the selected zone
- ctrl+S to save both the map and the ambience file
**********************************************************************/

void editor_input_ambience(EditorState& editor, WorldMap& map, GLFWwindow* window, float dt, bool& map_dirty){
    static constexpr int AMB_PAGE_SIZE = 8;

    static bool s_lmb_last = false;
    static bool s_del_last = false;
    static bool s_amb_f_last = false;
    static bool s_arr_up_last = false;
    static bool s_arr_down_last = false;

    bool ctrl  = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    bool lmb   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // LMB: place new zone or select existing one
    if (lmb && !s_lmb_last && editor.placement_valid){
        // check if clicking near an existing zone center (within 2m)
        int closest_id = -1;
        float closest_dist = 2.0f;
        for (int i = 0; i < map.ambience_count; i++){
            glm::vec3 d = map.ambience_zones[i].pos - editor.ghost_pos;
            d.y = 0.0f;
            float dist = glm::length(d);
            if (dist < closest_dist){ closest_dist = dist; closest_id = map.ambience_zones[i].id; }
        }

        if (closest_id != -1){
            editor.selected_zone_id = closest_id;
            editor.ambience_file_page = 0;
            std::cout << "[ambience] selected zone id=" << closest_id << "\n";
        }
        else if (map.ambience_count < Const::MAX_AMBIENCE_ZONES){
            // place new zone
            AmbienceZone z;
            z.id = map.next_ambience_id++;
            z.pos = editor.ghost_pos;
            z.radius = Const::AMBIENCE_RADIUS_DEFAULT;
            z.type = AMBIENCE_PROXIMITY;
            z.night_only = false;
            map.ambience_zones[map.ambience_count++] = z;
            editor.selected_zone_id = z.id;
            editor.ambience_file_page = 0;
            map_dirty = true;
            std::cout << "[ambience] placed zone id=" << z.id << "\n";
        }
    }
    s_lmb_last = lmb;

    // operate on selected zone
    AmbienceZone* zone = nullptr;
    for (int i = 0; i < map.ambience_count; i++)
        if (map.ambience_zones[i].id == editor.selected_zone_id){ zone = &map.ambience_zones[i]; break; }

    if (zone){
        // [ / ] shrink / grow radius
        bool brk_l = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
        bool brk_r = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
        if (brk_l){ zone->radius = std::max(Const::AMBIENCE_RADIUS_MIN, zone->radius - Const::AMBIENCE_RADIUS_STEP); map_dirty = true; }
        if (brk_r){ zone->radius = std::min(Const::AMBIENCE_RADIUS_MAX, zone->radius + Const::AMBIENCE_RADIUS_STEP); map_dirty = true; }

        // F cycle type: PROXIMITY <-> NIGHT
        bool f_down = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        if (f_down && !s_amb_f_last){
            zone->type = (zone->type == AMBIENCE_PROXIMITY) ? AMBIENCE_NIGHT : AMBIENCE_PROXIMITY;
            zone->night_only = (zone->type == AMBIENCE_NIGHT);
            map_dirty = true;
            std::cout << "[ambience] zone id=" << zone->id
                      << " type -> " << (zone->type == AMBIENCE_NIGHT ? "NIGHT" : "PROXIMITY") << "\n";
        }
        s_amb_f_last = f_down;

        // up/down scroll audio file list
        bool au = glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS;
        bool ad = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
        if (au && !s_arr_up_last)
            editor.ambience_file_page = std::max(0, editor.ambience_file_page - 1);
        if (ad && !s_arr_down_last){
            int max_page = std::max(0, (int)editor.audio_file_list.size() - AMB_PAGE_SIZE);
            editor.ambience_file_page = std::min(max_page, editor.ambience_file_page + 1);
        }
        s_arr_up_last  = au;
        s_arr_down_last = ad;

        // 1-8 assign audio file to zone
        static const int num_keys[8] = {
            GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
            GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8
        };
        for (int i = 0; i < AMB_PAGE_SIZE; i++){
            if (glfwGetKey(window, num_keys[i]) == GLFW_PRESS){
                int idx = editor.ambience_file_page + i;
                if (idx < (int)editor.audio_file_list.size()){
                    strncpy(zone->audio_path, editor.audio_file_list[idx].c_str(), 255);
                    zone->audio_path[255] = '\0';
                    map_dirty = true;
                    std::cout << "[ambience] zone id=" << zone->id
                              << " audio -> " << zone->audio_path << "\n";
                }
            }
        }

        // DEL delete selected zone
        bool del = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
        if (del && !s_del_last){
            // compact the array
            for (int i = 0; i < map.ambience_count; i++){
                if (map.ambience_zones[i].id != editor.selected_zone_id) continue;
                map.ambience_zones[i] = map.ambience_zones[--map.ambience_count];
                break;
            }
            editor.selected_zone_id = -1;
            map_dirty = true;
            std::cout << "[ambience] zone deleted\n";
        }
        s_del_last = del;
    }

    // Ctrl+S saves both map and ambience file
    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        world_map_save(map, g_maps.loaded_dir + "/map.txt");
        ambience_save(map.ambience_zones, map.ambience_count,
            (g_maps.loaded_dir + "/_ambience.amb").c_str());
    }
}