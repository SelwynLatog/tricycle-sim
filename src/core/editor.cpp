#include "editor.hpp"
#include "editor_cam.hpp"
#include "editor_input.hpp"
#include "../renderer/scene_daytime_weather.hpp"
#include "../renderer/scene_shadow.hpp"
#include "../renderer/scene_draw.hpp"
#include "const.hpp"
#include "settings.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

void editor_input_settings(EditorState& editor, GLFWwindow* window);

/******************************************************************************
 EDITOR MODE
******************************************************************************/
void app_run_editor(App& app, float dt){
    if (!app.editor.settings_open)
        editor_cam_update(app.editor, app.window.handle, dt);

    glClearColor(Const::CLEAR_R, Const::CLEAR_G, Const::CLEAR_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = editor_cam_get_view(app.editor);
    glm::mat4 proj = glm::perspective(
        glm::radians(Const::CAM_FOV),
        (float)Const::WINDOW_WIDTH / (float)Const::WINDOW_HEIGHT,
        Const::CAM_NEAR, Const::CAM_FAR);

    if (!app.editor.settings_open)
        editor_input_update(app.editor, app.map, app.editor_renderer,
            app.window.handle, view, proj,
            Const::WINDOW_WIDTH, Const::WINDOW_HEIGHT, dt, app.obstacles_dirty);

    // SHADOW PASS
    scene_update_daytime(app.scene, dt);
    app.editor_renderer.sun_dir = app.scene.sun_dir;
    app.editor_renderer.light_color = app.scene.light_color;
    app.editor_renderer.ambient = app.scene.ambient;
    app.editor_renderer.diff_intensity = app.scene.diff_intensity;
    app.editor_renderer.shadow_cull_center = app.editor.cam_pos;
    scene_shadow_pass(app.scene, app.obstacles, app.editor.cam_pos);

    if (my_settings.render_shadows){
        glBindFramebuffer(GL_FRAMEBUFFER, app.scene.shadow_fbo);
        glViewport(0, 0, my_settings.shadow_map_size, my_settings.shadow_map_size);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        editor_renderer_shadow_pass(app.editor_renderer, app.map, app.scene.light_space_mat, app.dynamic_sims);
        scene_trike_shadow_draw(app.scene, app.trike);
        driver_model_draw(app.scene.driver_model, app.player, app.trike,
            app.scene.shadow_shader, app.scene.light_space_mat, glm::mat4(1.0f),
            app.editor.pose_quat, app.editor.pose_offset, app.editor.pose_seat);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else {
        glBindFramebuffer(GL_FRAMEBUFFER, app.scene.shadow_fbo);
        glViewport(0, 0, my_settings.shadow_map_size, my_settings.shadow_map_size);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    {
        int fb_w, fb_h;
        glfwGetFramebufferSize(app.window.handle, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
    }

    // push scene lighting to editor_renderer
    app.editor_renderer.shadow_depth_tex = app.scene.shadow_depth_tex;
    app.editor_renderer.light_space_mat = app.scene.light_space_mat;
    app.editor_renderer.night_factor = app.scene.night_factor;
    app.editor_renderer.fog_color = app.scene.fog_color;
    app.editor_renderer.fog_near = app.scene.fog_near;
    app.editor_renderer.fog_far = app.scene.fog_far;

    // -1 hides pose overlay unless we're actively in pose mode
    app.editor_renderer.pose_npc_id = (app.editor.mode == MODE_POSE) ? app.editor.pose_npc_id : -1;

    // PLANAR REFLECTION PASS
    // same mirrored-world trick as drive mode
    if (app.map.ocean.enabled){
        glm::mat4 refl_view = scene_build_reflect_view(view, app.map.ocean.y_level);
        app.scene.reflect_view_proj = proj * refl_view;

        glBindFramebuffer(GL_FRAMEBUFFER, app.scene.reflect_fbo);
        glViewport(0, 0, app.scene.reflect_w, app.scene.reflect_h);
        glClearColor(Const::CLEAR_R, Const::CLEAR_G, Const::CLEAR_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glFrontFace(GL_CW);

        scene_draw_sky(app.scene, refl_view, proj);
        scene_draw(app.scene, app.trike, app.obstacles, app.map.lights, refl_view, proj, false);
        editor_renderer_draw_roads(app.editor_renderer, app.map.roads, refl_view, proj);
        editor_renderer_draw_ocean(app.editor_renderer, app.map.ocean, app.map.terrain, refl_view, proj, dt,
            app.map.terrain.origin.x,
            app.map.terrain.origin.x + app.map.terrain.cols * app.map.terrain.cell_size,
            app.map.terrain.origin.z,
            app.map.terrain.origin.z + app.map.terrain.rows * app.map.terrain.cell_size);
        editor_renderer_draw_terrain_surface(app.editor_renderer, app.map.terrain, refl_view, proj, app.map.ocean);
        editor_renderer_draw_props(app.editor_renderer, app.map, refl_view, proj,
            {}, app.dynamic_sims, app.map.lights, true);

        glFrontFace(GL_CCW);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int fb_w, fb_h;
        glfwGetFramebufferSize(app.window.handle, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
    }
    app.editor_renderer.reflect_tex = app.scene.reflect_color_tex;
    app.editor_renderer.reflect_view_proj = app.scene.reflect_view_proj;

    // MAIN DRAW
    scene_draw_sky(app.scene, view, proj);
    scene_draw(app.scene, app.trike, app.obstacles, app.map.lights, view, proj,
        app.editor.show_hitboxes);

    // driver visible in editor except pose mode (pose mode has its own isolated draw)
    if (app.editor.mode != MODE_POSE){
        PlayerState fake_driving;
        fake_driving.mode = PLAYER_DRIVING;
        scene_draw_driver(app.scene, fake_driving, app.trike, view, proj,
            app.editor_renderer.obj_shader,
            app.editor.pose_quat, app.editor.pose_offset, app.editor.pose_seat);
    }

    if (app.editor.mode == MODE_TERRAIN || app.editor.mode == MODE_ROAD)
        editor_renderer_draw_terrain(app.editor_renderer, app.map.terrain, view, proj,
            app.editor.ghost_pos, app.editor.brush_radius, app.editor.placement_valid);

    editor_renderer_draw_roads(app.editor_renderer, app.map.roads, view, proj);
    editor_renderer_draw_terrain_surface(app.editor_renderer, app.map.terrain, view, proj,
        app.map.ocean);
    editor_renderer_draw_ocean(app.editor_renderer, app.map.ocean, app.map.terrain, view, proj, dt,
        app.map.terrain.origin.x,
        app.map.terrain.origin.x + app.map.terrain.cols * app.map.terrain.cell_size,
        app.map.terrain.origin.z,
        app.map.terrain.origin.z + app.map.terrain.rows * app.map.terrain.cell_size);
    editor_renderer_draw(app.editor_renderer, app.editor, app.map, view, proj,
        app.editor.show_hitboxes, app.map.lights);

    if (app.editor.mode == MODE_POSE){
        DriverModel* pose_npc_model = nullptr;
        for (const auto& o : app.map.objects){
            if (o.id != app.editor.pose_npc_id) continue;
            auto mit = app.npc_model_cache.find(o.model_path);
            if (mit != app.npc_model_cache.end())
                pose_npc_model = &mit->second;
            break;
        }
        editor_renderer_draw_pose_mode(app.editor_renderer, app.editor,
            app.scene.driver_model, app.scene.trike_model, view, proj,
            pose_npc_model, app.map);
    }

    if (!app.editor.settings_open){
        editor_renderer_draw_hud(app.editor_renderer, app.editor, app.map);
        font_draw(app.editor_renderer.font,
            "[TAB] drive  [L CLICK] place/select  [DEL] delete  [B] behavior  [Ctrl+S] save",
            10, Const::WINDOW_HEIGHT - 40, 2, 0.7f, 0.7f, 0.7f);
        font_draw(app.editor_renderer.font,
            "[T] translate  [R] rotate  [Y] scale  [PgUp/PgDn] Y nudge  [1-9] prop  [/] page",
            10, Const::WINDOW_HEIGHT - 20, 2, 0.7f, 0.7f, 0.7f);
    }
    if (app.editor.settings_open){
        editor_input_settings(app.editor, app.window.handle);
        editor_renderer_draw_settings_menu(app.editor_renderer, app.editor);
    }

    // pedestrian config overlay for selected object
    if (!app.editor.settings_open && app.editor.selected_id != -1){
        for (const auto& o : app.map.objects){
            if (o.id != app.editor.selected_id) continue;
            if (o.behavior != PEDESTRIAN) break;
            char buf[256];
            snprintf(buf, sizeof(buf),
                "[PEDESTRIAN] type:%s  hail:%s  weight:%.1fkg  [J]=type [G]=hail [+/-]=weight",
                NPC_TYPE_NAMES[o.npc_type], o.npc_can_hail ? "YES" : "NO", o.npc_weight);
            font_draw(app.editor_renderer.font, buf,
                10, Const::WINDOW_HEIGHT - 60, 2, 1.0f, 0.85f, 0.3f);
            snprintf(buf, sizeof(buf),
                "walk_a:(%.1f,%.1f)  walk_b:(%.1f,%.1f)  drop:(%.1f,%.1f)  [I]=A [U]=B [X]=drop",
                o.npc_walk_a.x, o.npc_walk_a.z,
                o.npc_walk_b.x, o.npc_walk_b.z,
                o.npc_drop_point.x, o.npc_drop_point.z);
            font_draw(app.editor_renderer.font, buf,
                10, Const::WINDOW_HEIGHT - 80, 2, 1.0f, 0.85f, 0.3f);
            break;
        }
    }

    window_swap_buffers(app.window);
    window_poll_events();
}