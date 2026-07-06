#include "input_audio.hpp"
#include "map_manager.hpp"
#include <algorithm>

/**********************************************************************
INPUT AUDIO
Responsibilities
- Audio slot cycling for the selected object
- Assigning a scanned audio file to the active slot
- Proximity radius adjustment (proximity slot only)
- Clear slot
- Save

How to 101 in case you struggle in operating in the program:
- select an object first, then press Z to enter AUDIO MODE
- TAB key cycles through the object's audio slots (impact, proximity, hail, etc)
- UP/DOWN scrolls the file list
- number keys 1-8 assign the highlighted file to the current slot
- [ / ] adjusts the proximity radius when the proximity slot is active
- DEL clears the current slot
- ctrl+S to save
**********************************************************************/

void editor_input_audio(EditorState& editor, WorldMap& map, GLFWwindow* window,
    float dt, bool& map_dirty){

    static constexpr int AUDIO_SLOT_COUNT = 10;
    static constexpr int AUDIO_PAGE_SIZE  = 8;

    static bool s_audio_tab_last = false;
    static bool s_arr_up_last = false;
    static bool s_arr_down_last = false;
    static bool s_del_last = false;

    static const char* AUDIO_SLOT_NAMES[] = {
        "impact", "proximity",
        "hail", "pickup", "yap",
        "dropoff_good", "dropoff_bad",
        "crash_mild", "crash_heavy", "crash_rollover"
    };

    // find selected object
    WorldObject* target = nullptr;
    for (auto& o : map.objects)
        if (o.id == editor.selected_id){ target = &o; break; }

    if (!target){
        editor.mode = MODE_OBJECT;
        return;
    }

    // helper: get pointer to the correct slot string by index
    auto get_slot = [&](WorldObject& o, int slot) -> std::string& {
        switch(slot){
            case 0: return o.audio_impact;
            case 1: return o.audio_proximity;
            case 2: return o.audio_hail;
            case 3: return o.audio_pickup;
            case 4: return o.audio_yap;
            case 5: return o.audio_dropoff_good;
            case 6: return o.audio_dropoff_bad;
            case 7: return o.audio_crash_mild;
            case 8: return o.audio_crash_heavy;
            case 9: return o.audio_crash_rollover;
            default: return o.audio_impact;
        }
    };

    // Tab cycles audio slots (within audio mode only, not the global tab)
    bool tab_down = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tab_down && !s_audio_tab_last){
        editor.audio_slot = (editor.audio_slot + 1) % AUDIO_SLOT_COUNT;
        editor.audio_file_page = 0;
        std::cout << "[audio] slot -> " << AUDIO_SLOT_NAMES[editor.audio_slot] << "\n";
    }
    s_audio_tab_last = tab_down;

    // up/down scroll file list
    bool au = glfwGetKey(window, GLFW_KEY_UP)   == GLFW_PRESS;
    bool ad = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    if (au && !s_arr_up_last){
        editor.audio_file_page = std::max(0, editor.audio_file_page - 1);
    }
    if (ad && !s_arr_down_last){
        int max_page = std::max(0, (int)editor.audio_file_list.size() - AUDIO_PAGE_SIZE);
        editor.audio_file_page = std::min(max_page, editor.audio_file_page + 1);
    }
    s_arr_up_last  = au;
    s_arr_down_last = ad;

    // 1-8 select file from current page and assign to slot
    static const int num_keys[8] = {
        GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
        GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8
    };
    for (int i = 0; i < AUDIO_PAGE_SIZE; i++){
        if (glfwGetKey(window, num_keys[i]) == GLFW_PRESS){
            int idx = editor.audio_file_page + i;
            if (idx < (int)editor.audio_file_list.size()){
                get_slot(*target, editor.audio_slot) = editor.audio_file_list[idx];
                map_dirty = true;
                std::cout << "[audio] slot=" << AUDIO_SLOT_NAMES[editor.audio_slot]
                          << " -> " << editor.audio_file_list[idx] << "\n";
            }
        }
    }

    // [ / ] adjust proximity radius when that slot is active
    if (editor.audio_slot == 1){
        bool brk_l = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
        bool brk_r = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
        if (brk_l){ target->audio_radius = std::max(1.0f, target->audio_radius - 4.0f * dt); map_dirty = true; }
        if (brk_r){ target->audio_radius = std::min(200.0f, target->audio_radius + 4.0f * dt); map_dirty = true; }
    }

    // DEL clears current slot
    bool del = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
    if (del && !s_del_last){
        get_slot(*target, editor.audio_slot).clear();
        map_dirty = true;
        std::cout << "[audio] slot=" << AUDIO_SLOT_NAMES[editor.audio_slot] << " cleared\n";
    }
    s_del_last = del;

    // Ctrl+S saves
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        world_map_save(map, g_maps.loaded_dir + "/map.txt");
}