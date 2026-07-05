#include "app_global_keys.hpp"
#include "editor_cam.hpp"
#include "const.hpp"
#include "settings.hpp"
#include "map_manager.hpp"
#include "../world/road_builder.hpp"
#include "../world/ambience_zone.hpp"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

/*****************************************************
 GLOBAL HOTKEYS
 run in both EDITOR and DRIVE mode:
   H   - toggle AABB hitboxes
   TAB - toggle editor <-> drive
   ESC - toggle settings menu + map hot-reload
******************************************************/
void app_handle_global_keys(App& app, float dt){

    // H key show AABB hitboxes ctrl+H reserved for editor pose mode
    static bool s_h_last = false;
    bool h_down  = glfwGetKey(app.window.handle, GLFW_KEY_H) == GLFW_PRESS;
    bool ctrl_h  = glfwGetKey(app.window.handle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    if (!app.editor.settings_open && h_down && !s_h_last && !ctrl_h)
        app.editor.show_hitboxes = !app.editor.show_hitboxes;
    s_h_last = h_down;

    // TAB key toggle EDITOR <-> DRIVE (blocked in Audio submode and settings)
    static bool s_tab_pressed_last = false;
    bool tab_down = glfwGetKey(app.window.handle, GLFW_KEY_TAB) == GLFW_PRESS;
    if (!app.editor.settings_open && tab_down && !s_tab_pressed_last && app.editor.mode != MODE_AUDIO){
        app.editor.active = !app.editor.active;
        if (app.editor.active){
            app.editor.cam_pos = app.trike.position + glm::vec3(0.0f, 12.0f, 0.0f);
            editor_cam_init(app.window.handle);
        }
        else {
            // only rebuild if map changed since last switch
            if (app.obstacles_dirty){
                world_map_to_obstacles(app);
                init_dynamic_sims(app);
                init_npcs(app);
                app.wo_by_id.clear();
                for (const auto& o : app.map.objects)
                    app.wo_by_id[o.id] = &o;
            }
        }
    }
    s_tab_pressed_last = tab_down;

    // ESC - settings menu toggle
    static bool s_esc_last = false;
    bool esc_down = glfwGetKey(app.window.handle, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (esc_down && !s_esc_last){
        static int s_prev_map_idx = 0;
        static std::string s_prev_loaded_dir;
        app.editor.settings_open = !app.editor.settings_open;
        app.editor.settings_page = SETTINGS_PAGE_MAIN;
        app.editor.settings_cursor = 0;
        if (app.editor.settings_open){
            audio_pause(app.audio);
            s_prev_map_idx = g_maps.active_index;
            s_prev_loaded_dir = g_maps.loaded_dir;
        }
        else {
            audio_resume(app.audio);
            settings_save();
            scene_shadow_resize(app.scene);

            /*****************************************************
             MAP HOT RELOAD

            If the active map changed while the settings
            menu was open, rebuild all runtime state:

            - world map
            - terrain
            - roads
            - obstacles
            - dynamic sims
            - NPCs
            - lookup tables

            ******************************************************/
            if (g_maps.maps[g_maps.active_index].dir != s_prev_loaded_dir){
                world_map_save(app.map, g_maps.loaded_dir + "/map.txt");
                ambience_save(app.map.ambience_zones, app.map.ambience_count,
                   (g_maps.loaded_dir + "/_ambience.amb").c_str());

                app.map = WorldMap{};
                heightfield_init(app.map.terrain,
                    Const::TERRAIN_ROWS, Const::TERRAIN_COLS,
                    Const::TERRAIN_CELL_SIZE,
                    glm::vec3(-(Const::TERRAIN_COLS * Const::TERRAIN_CELL_SIZE) * 0.5f, 0.0f,
                              -(Const::TERRAIN_ROWS * Const::TERRAIN_CELL_SIZE) * 0.5f));
                world_map_load(app.map, g_maps.map_path());

                ambience_load(app.map.ambience_zones, app.map.ambience_count,
                    Const::MAX_AMBIENCE_ZONES, g_maps.ambience_path().c_str());
                for (auto& r : app.map.roads)
                    road_spline_build_mesh(r, &app.map.terrain);
                for (const auto& o : app.map.objects)
                    if (!o.model_path.empty())
                        editor_get_y_floor_offset(app.editor_renderer, o.model_path);
                world_map_to_obstacles(app);
                init_dynamic_sims(app);
                init_npcs(app);
                app.wo_by_id.clear();
                for (const auto& o : app.map.objects) app.wo_by_id[o.id] = &o;
                g_maps.loaded_index = g_maps.active_index;
                g_maps.loaded_dir   = g_maps.maps[g_maps.active_index].dir;
                app.obstacles_dirty = false;
                app.editor_renderer.terrain_surface_dirty = true;
                app.editor_renderer.terrain_mesh_dirty = true;
                app.trike = TrikeState{};
                app.player = PlayerState{};
                app.dynamic_sims.clear();
                std::cout << "[maps] switched to " << g_maps.maps[g_maps.active_index].name << "\n";
            }
        }
    }
    s_esc_last = esc_down;
}