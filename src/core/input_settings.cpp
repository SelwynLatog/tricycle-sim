#include "input_settings.hpp"
#include "const.hpp"
#include "settings.hpp"
#include "map_manager.hpp"
#include <algorithm>

/**********************************************************************
INPUT SETTINGS
Responsibilities
- Settings menu nav:
 * main
 * graphics
 * controls 
 * maps pages
- Map rename text entry
- Graphics preset + individual setting adjustment


How to 101 in case you struggle in operating in the program:
- ESC key activates settings page
- arrow based input and is pretty much self explanatory in the program
**********************************************************************/

void editor_input_settings(EditorState& editor, GLFWwindow* window){

    static bool s_up_last = false;
    static bool s_dn_last = false;
    static bool s_lt_last = false;
    static bool s_rt_last = false;
    static bool s_en_last = false;

    bool up = glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS;
    bool dn = glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS;
    bool lt = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
    bool rt = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool en = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;

    // MAIN PAGE
    if (editor.settings_page == SETTINGS_PAGE_MAIN){
        // 4 items: GRAPHICS=0, CONTROLS=1, MAPS=2, QUIT=3
        if (up && !s_up_last) editor.settings_cursor = std::max(0, editor.settings_cursor - 1);
        if (dn && !s_dn_last) editor.settings_cursor = std::min(3, editor.settings_cursor + 1);

        if (en && !s_en_last){
            if (editor.settings_cursor == 0){
                editor.settings_page  = SETTINGS_PAGE_GRAPHICS;
                editor.settings_cursor = 0;
            }
            else if (editor.settings_cursor == 1){
                editor.settings_page  = SETTINGS_PAGE_CONTROLS;
                editor.settings_cursor = 0;
            }
            else if (editor.settings_cursor == 2){
                editor.settings_page  = SETTINGS_PAGE_MAPS;
                editor.settings_cursor = g_maps.active_index;
                g_maps.rename_mode = false;
                map_manager_scan();
            }
            else {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
    }
    // CONTROLS PAGE
    // cursor = sub-page index, left/right flips pages
    else if (editor.settings_page == SETTINGS_PAGE_CONTROLS){
        static const int CTRL_PAGES = 5;
        if (lt && !s_lt_last) editor.settings_cursor = std::max(0, editor.settings_cursor - 1);
        if (rt && !s_rt_last) editor.settings_cursor = std::min(CTRL_PAGES - 1, editor.settings_cursor + 1);

        if (en && !s_en_last){
            // enter returns to main
            editor.settings_page   = SETTINGS_PAGE_MAIN;
            editor.settings_cursor = 1; // leave cursor on CONTROLS
        }
    }

    // MAPS PAGE
    else if (editor.settings_page == SETTINGS_PAGE_MAPS){
        int total = (int)g_maps.maps.size();

        if (g_maps.rename_mode){
            // collect printable chars into rename_buf, backspace, enter to confirm
            // GLFW doesn't give us a char callback here so poll A-Z 0-9 space
            // good enough for map names
            static bool s_bs_last = false;
            bool bs = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
            if (bs && !s_bs_last && !g_maps.rename_buf.empty())
                g_maps.rename_buf.pop_back();
            s_bs_last = bs;

            // poll letter keys
            static bool s_key_last[26] = {};
            for (int i = 0; i < 26; i++){
                bool kd = glfwGetKey(window, GLFW_KEY_A + i) == GLFW_PRESS;
                if (kd && !s_key_last[i] && g_maps.rename_buf.size() < 28)
                    g_maps.rename_buf += (char)('A' + i);
                s_key_last[i] = kd;
            }
            // poll 0-9
            static bool s_num_last[10] = {};
            for (int i = 0; i < 10; i++){
                bool kd = glfwGetKey(window, GLFW_KEY_0 + i) == GLFW_PRESS;
                if (kd && !s_num_last[i] && g_maps.rename_buf.size() < 28)
                    g_maps.rename_buf += (char)('0' + i);
                s_num_last[i] = kd;
            }
            // space
            static bool s_sp_last = false;
            bool sp = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (sp && !s_sp_last && g_maps.rename_buf.size() < 28)
                g_maps.rename_buf += ' ';
            s_sp_last = sp;

            if (en && !s_en_last){
                // confirm rename - resolve by dir identity
                // NOT by settings_cursor,
                // since settings_cursor can go stale if a rescan reordered g_maps.maps
                // while the person was typing
                if (!g_maps.rename_buf.empty()){
                    for (auto& m : g_maps.maps){
                        if (m.dir != g_maps.rename_target_dir) continue;
                        m.name = g_maps.rename_buf;
                        map_write_name(m.dir, g_maps.rename_buf);
                        break;
                    }
                }
                g_maps.rename_mode = false;
            }
        }
        else {
            if (up && !s_up_last) editor.settings_cursor = std::max(0, editor.settings_cursor - 1);
            if (dn && !s_dn_last) editor.settings_cursor = std::min(total, editor.settings_cursor + 1);
            // total+1 because last row is [NEW MAP]

            if (en && !s_en_last){
                if (editor.settings_cursor < total){
                    // switch to selected map
                    // caller (app.cpp) checks g_maps.active_index change and reloads
                    g_maps.active_index = editor.settings_cursor;
                    editor.settings_page  = SETTINGS_PAGE_MAIN;
                    editor.settings_cursor = 2;
                }
                else {
                    // [NEW MAP] row
                    // map_manager_new already resolves the correct index internally
                    // by matching dir identity. Re-deriving it via maps.size()-1 is
                    // wrong the moment any folder (e.g. "backup", or any future map)
                    // sorts alphabetically after the new one
                    int new_idx = map_manager_new("New Map");
                    g_maps.active_index = new_idx;
                    editor.settings_cursor = new_idx;
                    g_maps.rename_mode = true;
                    g_maps.rename_buf.clear();
                    g_maps.rename_target_dir = g_maps.maps[new_idx].dir;
                }
            }

            // F2 = rename selected map
            static bool s_f2_last = false;
            bool f2 = glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS;
            if (f2 && !s_f2_last && editor.settings_cursor < total){
                g_maps.rename_mode = true;
                g_maps.rename_buf = g_maps.maps[editor.settings_cursor].name;
                g_maps.rename_target_dir = g_maps.maps[editor.settings_cursor].dir;
            }
            s_f2_last = f2;

            // ESC / left arrow goes back to main
            if (lt && !s_lt_last){
                editor.settings_page  = SETTINGS_PAGE_MAIN;
                editor.settings_cursor = 2;
            }
        }
    }

    // GRAPHICS PAGE
    // row 0 = preset, rows 1-9 = individual settings, row 10 = BACK
    else if (editor.settings_page == SETTINGS_PAGE_GRAPHICS){
        static const int GRAPHICS_ROWS = 12; // 0=preset, 1-10=settings, 11=back
        if (up && !s_up_last) editor.settings_cursor = std::max(0, editor.settings_cursor - 1);
        if (dn && !s_dn_last) editor.settings_cursor = std::min(GRAPHICS_ROWS - 1, editor.settings_cursor + 1);

        // left/right adjust the selected row
        if (editor.settings_cursor == 0){
            // preset row: cycle presets
            if (lt && !s_lt_last){
                int p = std::max(0, (int)my_settings.preset - 1);
                settings_apply_preset((Preset)p);
            }
            if (rt && !s_rt_last){
                int p = std::min((int)CUSTOM, (int)my_settings.preset + 1);
                settings_apply_preset((Preset)p);
            }
        }
        else if (editor.settings_cursor == 11){
            // BACK row
            if (en && !s_en_last){
                settings_save();
                editor.settings_page   = SETTINGS_PAGE_MAIN;
                editor.settings_cursor = 0;
            }
        }
        else {
            // individual setting rows 1-9
            // map cursor to setting and adjust
            // nudge amounts per setting
            // shadow_map_size: *2 / /2 (power of 2 steps)
            // throttle: int +-1
            // distances: +-10m
            // particle counts: +-500
            // bools: toggle on left or right
            int row = editor.settings_cursor;
            switch(row){
                case 1: // shadow map size
                    if (lt && !s_lt_last) my_settings.shadow_map_size = std::max(128, my_settings.shadow_map_size / 2);
                    if (rt && !s_rt_last) my_settings.shadow_map_size = std::min(4096, my_settings.shadow_map_size * 2);
                    my_settings.preset = CUSTOM;
                    break;
                case 2: // shadow throttle
                    if (lt && !s_lt_last) my_settings.shadow_throttle_frame = std::max(1,my_settings.shadow_throttle_frame - 1);
                    if (rt && !s_rt_last) my_settings.shadow_throttle_frame = std::min(12, my_settings.shadow_throttle_frame + 1);
                    my_settings.preset = CUSTOM;
                    break;
                case 3: // prop cull dist
                    if (lt && !s_lt_last) my_settings.prop_cull_dist = std::max(30.0f, my_settings.prop_cull_dist - 10.0f);
                    if (rt && !s_rt_last) my_settings.prop_cull_dist = std::min(300.0f, my_settings.prop_cull_dist + 10.0f);
                    my_settings.preset = CUSTOM;
                    break;
                case 4: // npc cull dist
                    if (lt && !s_lt_last) my_settings.npc_cull_dist = std::max(40.0f, my_settings.npc_cull_dist - 10.0f);
                    if (rt && !s_rt_last) my_settings.npc_cull_dist = std::min(300.0f, my_settings.npc_cull_dist + 10.0f);
                    my_settings.preset = CUSTOM;
                    break;
                case 5: // light cull dist
                    if (lt && !s_lt_last) my_settings.light_cull_dist = std::max(40.0f, my_settings.light_cull_dist - 10.0f);
                    if (rt && !s_rt_last) my_settings.light_cull_dist = std::min(300.0f, my_settings.light_cull_dist + 10.0f);
                    my_settings.preset = CUSTOM;
                    break;
                case 6: // rain particles
                    if (lt && !s_lt_last) my_settings.rain_particle_count = std::max(0, my_settings.rain_particle_count - 500);
                    if (rt && !s_rt_last) my_settings.rain_particle_count = std::min(8000, my_settings.rain_particle_count + 500);
                    my_settings.preset = CUSTOM;
                    break;
                case 7: // rain splashes
                    if (lt && !s_lt_last) my_settings.rain_splash_max = std::max(0, my_settings.rain_splash_max - 100);
                    if (rt && !s_rt_last) my_settings.rain_splash_max = std::min(1200, my_settings.rain_splash_max + 100);
                    my_settings.preset = CUSTOM;
                    break;
                case 8: // render shadows bool
                    if ((lt && !s_lt_last) || (rt && !s_rt_last)){
                        my_settings.render_shadows = !my_settings.render_shadows;
                        my_settings.preset = CUSTOM;
                    }
                    break;
                case 9: // show hud bool
                    if ((lt && !s_lt_last) || (rt && !s_rt_last)){
                        my_settings.show_hud = !my_settings.show_hud;
                        my_settings.preset = CUSTOM;
                    }
                    break;
                case 10: // render fog bool
                     if ((lt && !s_lt_last) || (rt && !s_rt_last)){
                        my_settings.render_fog = !my_settings.render_fog;
                        my_settings.preset = CUSTOM;
                    }
                    break;
            }
        }
    }

    s_up_last = up;
    s_dn_last = dn;
    s_lt_last = lt;
    s_rt_last = rt;
    s_en_last = en;
}