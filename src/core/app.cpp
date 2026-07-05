#include "../renderer/editor_renderer.hpp"
#include "../renderer/render_init.hpp"
#include "../renderer/render_textures.hpp"
#include "../world/world_map.hpp"
#include "../world/road_builder.hpp"
#include "../world/ambience_zone.hpp"
#include "../world/rain.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "app.hpp"
#include "app_global_keys.hpp"
#include "editor.hpp"
#include "drive.hpp"
#include "editor_input.hpp"
#include "npc_update.hpp"
#include "const.hpp"
#include "settings.hpp"
#include "map_manager.hpp"
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>

/*****************************************************
 MAIN APPLICATION - init, loop, shutdown
 subsystem calls logic lives in:
   collision.cpp       - static + dynamic collision
   npc_update.cpp       - NPC per-frame logic
   cam.cpp              - camera computation
   app_global_keys.cpp  - H/TAB/ESC toggles + map hot-reload
   editor.cpp            - editor mode frame
   drive.cpp             - drive mode frame
******************************************************/

/********************************************
 APP_INIT
 subsystem init order:
 1. window + gl
 2. scene, editor, road_builder, terrain
 3. prop bounds
 4. obstacles, dynamic rigid body, npcs
 5. id lookup
 6. driver pose, hud, audio
 7. cam seed
********************************************/
void app_init(App& app){
    window_init(app.window, Const::WINDOW_WIDTH, Const::WINDOW_HEIGHT, Const::WINDOW_TITLE);

    glEnable(GL_DEPTH_TEST);
    int fb_w, fb_h;
    glfwGetFramebufferSize(app.window.handle, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);

    scene_init(app.scene);
    editor_renderer_init(app.editor_renderer);
    road_builder_init("../assets/props");

    heightfield_init(app.map.terrain,
        Const::TERRAIN_ROWS, Const::TERRAIN_COLS,
        Const::TERRAIN_CELL_SIZE,
        glm::vec3(-(Const::TERRAIN_COLS * Const::TERRAIN_CELL_SIZE) * 0.5f, 0.0f,
                  -(Const::TERRAIN_ROWS * Const::TERRAIN_CELL_SIZE) * 0.5f));

    editor_scan_props(app.editor, "../assets/props");
    
    /*****************************************************
     LEGACY MAP MIGRATION

    Converts pre-map-manager saves into the
    modern maps/<name>/ folder layout.

    Runs once on startup.
    ******************************************************/

    map_manager_scan();
    {
        std::string legacy_map = "../assets/backup/map.txt";
        std::string legacy_amb = "../assets/backup/_ambience.amb";
        std::string target_dir = "../assets/maps/poblacion";
        if (std::filesystem::exists(legacy_map) && !std::filesystem::exists(target_dir)){
            std::filesystem::create_directories(target_dir);
            std::filesystem::rename(legacy_map, target_dir + "/map.txt");
            if (std::filesystem::exists(legacy_amb))
                std::filesystem::rename(legacy_amb, target_dir + "/_ambience.amb");
            // migrate all sibling files: _terrain.hf, _roads.rd, _ocean.oc, _audio.au, _lights.lt, _npc_poses.np
            for (const char* suffix : {"_terrain.hf","_roads.rd","_ocean.oc","_audio.au","_lights.lt","_npc_poses.np"}){
                std::string src = "../assets/map" + std::string(suffix);
                std::string dst = target_dir + "/map" + suffix;
                if (std::filesystem::exists(src))
                    std::filesystem::rename(src, dst);
            }
            map_write_name(target_dir, "Poblacion");
            map_manager_scan();
            std::cout << "[maps] migrated legacy files to maps/barangay\n";
        }
        if (g_maps.maps.empty()) map_manager_new("Poblacion");
    }
    
    world_map_load(app.map, g_maps.map_path());
    g_maps.loaded_index = g_maps.active_index;
    g_maps.loaded_dir = g_maps.maps[g_maps.active_index].dir;
    ambience_load(app.map.ambience_zones, app.map.ambience_count,
        Const::MAX_AMBIENCE_ZONES, g_maps.ambience_path().c_str());
    for (auto& r : app.map.roads)
        road_spline_build_mesh(r, &app.map.terrain);

    // trigger prop bound caching before building obstacles
    for (const auto& o : app.map.objects){
        if (!o.model_path.empty())
            editor_get_y_floor_offset(app.editor_renderer, o.model_path);
    }
    world_map_to_obstacles(app);
    init_dynamic_sims(app);
    init_npcs(app);

    app.wo_by_id.clear();
    for (const auto& o : app.map.objects)
        app.wo_by_id[o.id] = &o;

    editor_renderer_preload_textures(app.editor_renderer);

    // load saved driver pose so drive mode is correct from the start
    {
        std::ifstream pf("../assets/entity/driver_pose.txt");
        if (pf.is_open()){
            std::string tag;
            while (pf >> tag){
                if (tag == "seat"){
                    pf >> app.editor.pose_seat.x >> app.editor.pose_seat.y >> app.editor.pose_seat.z;
                }
                else if (tag == "quat"){
                    int idx; pf >> idx;
                    if (idx >= 0 && idx < 6)
                        pf >> app.editor.pose_quat[idx].w >> app.editor.pose_quat[idx].x
                           >> app.editor.pose_quat[idx].y >> app.editor.pose_quat[idx].z;
                }
                else if (tag == "offset"){
                    int idx; pf >> idx;
                    if (idx >= 0 && idx < 6)
                        pf >> app.editor.pose_offset[idx].x >> app.editor.pose_offset[idx].y
                           >> app.editor.pose_offset[idx].z;
                }
            }
            std::cout << "[app] loaded driver pose from driver_pose.txt\n";
        }
    }

    hud_init(app.hud, Const::WINDOW_WIDTH, Const::WINDOW_HEIGHT);
    audio_init(app.audio, "../assets");
    settings_load();
    rain_init(app.rain, app.trike.position);

    app.cam.yaw   = Const::CAM_YAW_DEFAULT;
    app.cam.pitch = Const::CAM_PITCH_DEFAULT;
    app.cam.dist  = Const::CAM_DIST_DEFAULT;
    cam_seed(app.cam, app);

    app.last_time   = (float)glfwGetTime();
    app.accumulator = 0.0f;
    app.running     = true;

    window_show(app.window);
}

/********************************************
 MAIN LOOP
 per-frame work is fully delegated:
   app_handle_global_keys - H/TAB/ESC + hot-reload
   app_run_editor          - editor mode frame
   app_run_drive           - drive mode frame
********************************************/
void app_run(App& app){
    while (!window_should_close(app.window)){

        // delta time clamped so a slow frame or debugger pause doesn't explode physics
        float now = (float)glfwGetTime();
        float dt  = glm::min(now - app.last_time, Const::MAX_DELTA);
        app.last_time = now;

        app_handle_global_keys(app, dt);

        if (app.editor.active)
            app_run_editor(app, dt);
        else
            app_run_drive(app, dt);
    }
}

/*****************************************************
 APP_SHUTDOWN
*****************************************************/
void app_shutdown(App& app){
    rain_destroy(app.rain);
    settings_save();
    audio_shutdown(app.audio);
    hud_destroy(app.hud);
    scene_destroy(app.scene);
    window_destroy(app.window);
    world_map_save(app.map, g_maps.loaded_dir + "/map.txt");
    ambience_save(app.map.ambience_zones, app.map.ambience_count,
        (g_maps.loaded_dir + "/_ambience.amb").c_str());    
    editor_renderer_destroy(app.editor_renderer);
    app.running = false;
}