#include "drive.hpp"
#include "const.hpp"
#include "../renderer/scene_daytime_weather.hpp"
#include "../renderer/scene_shadow.hpp"
#include "../renderer/scene_draw.hpp"
#include "settings.hpp"
#include "../physics/trike_aabb.hpp"
#include "../physics/collision.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <map>
#include <string>

// editor_input_settings - the settings menu while in drive
// is called here too, so we add a forward dec at top
void editor_input_settings(EditorState& editor, GLFWwindow* window);

/******************************************************************************
 DRIVE MODE
******************************************************************************/
void app_run_drive(App& app, float dt){

    glm::mat4 proj = glm::perspective(
        glm::radians(Const::CAM_FOV),
        (float)Const::WINDOW_WIDTH / (float)Const::WINDOW_HEIGHT,
        Const::CAM_NEAR, Const::CAM_FAR);
    glm::mat4 view = cam_update(app.cam, app, dt, false);

    // gate gameplay to freeze when opening settings
    if (!app.editor.settings_open){
        // TRIKE INPUT
        // S: brake if moving forward, reverse if stopped
        TrikeInput input = {};
        if (app.player.mode == PLAYER_DRIVING || app.player.mode == PLAYER_MOUNTING){
            input.throttle = (glfwGetKey(app.window.handle, GLFW_KEY_W) == GLFW_PRESS) ? 1.0f : 0.0f;
            bool s_held   = glfwGetKey(app.window.handle, GLFW_KEY_S) == GLFW_PRESS;
            input.brake   = (s_held && app.trike.speed >  0.5f) ? 1.0f : 0.0f;
            input.reverse = (s_held && app.trike.speed <= 0.5f) ? 1.0f : 0.0f;
            input.steer   = 0.0f;
            if (glfwGetKey(app.window.handle, GLFW_KEY_A) == GLFW_PRESS) input.steer -= 1.0f;
            if (glfwGetKey(app.window.handle, GLFW_KEY_D) == GLFW_PRESS) input.steer += 1.0f;
        }
        else {
            // FOOT MODE - A/D orbit camera, W/S move along cam facing
            float move   = 0.0f;
            bool a_held  = glfwGetKey(app.window.handle, GLFW_KEY_A) == GLFW_PRESS;
            bool d_held  = glfwGetKey(app.window.handle, GLFW_KEY_D) == GLFW_PRESS;
            if (glfwGetKey(app.window.handle, GLFW_KEY_W) == GLFW_PRESS) move =  1.0f;
            if (glfwGetKey(app.window.handle, GLFW_KEY_S) == GLFW_PRESS) move = -1.0f;
            if (a_held) app.cam.yaw -= 120.0f * dt;
            if (d_held) app.cam.yaw += 120.0f * dt;

            // +180 so W moves toward where the camera is looking
            float cam_world_angle = glm::radians(app.cam.yaw) + glm::radians(180.0f);
            glm::vec3 fwd_dir = { std::cos(cam_world_angle), 0.0f, std::sin(cam_world_angle) };

            float walk_speed = 4.0f;
            glm::vec3 walk_vel = glm::vec3(0.0f);
            if (move != 0.0f){
                walk_vel = fwd_dir * (move * walk_speed);
                app.player.yaw = std::atan2(walk_vel.z, walk_vel.x);
            }

            // wrap yaw diff to [-pi, pi] to always take the short arc
            float yaw_diff = app.player.yaw - app.player.visual_yaw;
            while (yaw_diff >  glm::pi<float>()) yaw_diff -= glm::two_pi<float>();
            while (yaw_diff < -glm::pi<float>()) yaw_diff += glm::two_pi<float>();
            app.player.visual_yaw += yaw_diff * glm::clamp(12.0f * dt, 0.0f, 1.0f);

            app.player.pos += walk_vel * dt;
            app.player.speed = glm::length(walk_vel);
            app.player.pos.y = heightfield_sample(app.map.terrain, app.player.pos.x, app.player.pos.z);
            app.player.anim_timer += (app.player.speed > 0.1f ? app.player.speed * 1.8f : 1.0f) * dt;

            // footstep SFX mapped to surface type
            if (app.player.speed > 0.1f){
                static const char* STEP_PATHS[(int)SURFACE_COUNT] = {
                    "",
                    "audio/footstep/concrete",
                    "audio/footstep/gravel",
                    "audio/footstep/dirt",
                    "audio/footstep/sand",
                    "audio/footstep/grass",
                    "audio/footstep/concrete",
                    "audio/footstep/rock"
                };
                SurfaceType surf = heightfield_get_surface(app.map.terrain,
                    app.player.pos.x, app.player.pos.z);
                int si = glm::clamp((int)surf, 0, (int)SURFACE_COUNT - 1);
                if (si > 0)
                    audio_trigger_step(app.audio,
                        std::string("../assets/") + STEP_PATHS[si], app.player.speed * dt);
            }
        }

        // F key - free cam
        static bool s_f_pressed_last = false;
        bool f_down = glfwGetKey(app.window.handle, GLFW_KEY_F) == GLFW_PRESS;
        if (f_down && !s_f_pressed_last) app.cam.free_cam = !app.cam.free_cam;
        s_f_pressed_last = f_down;

        // Q key -  confirm passenger pickup
        static bool s_q_last = false;
        bool q_down    = glfwGetKey(app.window.handle, GLFW_KEY_Q) == GLFW_PRESS;
        bool s_q_pickup = (q_down && !s_q_last);
        s_q_last = q_down;

        // E  key - mount/dismount
        static bool s_e_last = false;
        bool e_down = glfwGetKey(app.window.handle, GLFW_KEY_E) == GLFW_PRESS;
        if (e_down && !s_e_last && app.player.mode != PLAYER_MOUNTING){
            if (app.player.mode == PLAYER_DRIVING){
                float side = app.trike.heading + glm::half_pi<float>();
                app.player.pos = app.trike.position + glm::vec3(std::cos(side), 0.0f, std::sin(side)) * 1.2f;
                app.player.yaw = app.trike.heading;
                app.player.visual_yaw = app.trike.heading;
                app.cam.yaw = 0.0f;
                app.cam.pos = app.player.pos + glm::vec3(0.0f, 4.0f, 0.0f);
                app.cam.needs_snap = true;
                app.player.mode = PLAYER_FOOT;
                app.player.headlights_on = false;
                if (app.audio.radio_on) audio_radio_toggle(app.audio);
            }
            else {
                // 3m mount radius, stored as squared distance to avoid sqrt
                glm::vec3 delta = app.trike.position - app.player.pos;
                delta.y = 0.0f;
                if (glm::dot(delta, delta) < 9.0f){
                    app.player.mode = PLAYER_MOUNTING;
                    app.player.mount_timer = 0.3f;
                }
            }
        }
        s_e_last = e_down;

        // brief mount transition so the driver snap doesn't look instant
        if (app.player.mode == PLAYER_MOUNTING){
            app.player.mount_timer -= dt;
            if (app.player.mount_timer <= 0.0f)
                app.player.mode = PLAYER_DRIVING;
        }

        // R key - full reset: player, dynamic objects, cam
        static bool s_r_last = false;
        bool r_down = glfwGetKey(app.window.handle, GLFW_KEY_R) == GLFW_PRESS;
        if (r_down && !s_r_last){
            app.trike = TrikeState{};
            app.player = PlayerState{};
            app.cam.yaw = Const::CAM_YAW_DEFAULT;
            app.cam.pitch = Const::CAM_PITCH_DEFAULT;
            app.cam.dist = Const::CAM_DIST_DEFAULT;
            app.dynamic_sims.clear();
            init_dynamic_sims(app);
            for (auto& obs : app.obstacles) obs.hit_timer = 0.0f;
        }
        s_r_last = r_down;

        // L key - headlight toggle (driving only)
        static bool s_l_last = false;
        bool l_down = glfwGetKey(app.window.handle, GLFW_KEY_L) == GLFW_PRESS;
        if (l_down && !s_l_last && app.player.mode == PLAYER_DRIVING){
            app.player.headlights_on = !app.player.headlights_on;
            audio_trigger_voice_local(app.audio, "../assets/audio/misc/headlight_switch.wav");
        }
        s_l_last = l_down;

        // P key - radio toggle (driving only)
        static bool s_p_last = false;
        bool p_down = glfwGetKey(app.window.handle, GLFW_KEY_P) == GLFW_PRESS;
        if (p_down && !s_p_last && app.player.mode == PLAYER_DRIVING)
            audio_radio_toggle(app.audio);
        s_p_last = p_down;

        // / key - next radio track
        static bool s_next_last = false;
        bool next_down = glfwGetKey(app.window.handle, GLFW_KEY_SLASH) == GLFW_PRESS;
        if (next_down && !s_next_last && app.audio.radio_on)
            audio_radio_next(app.audio);
        s_next_last = next_down;

        // arrow keys orbit camera
        if (glfwGetKey(app.window.handle, GLFW_KEY_LEFT)  == GLFW_PRESS) app.cam.yaw   -= Const::CAM_YAW_SPEED   * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS) app.cam.yaw   += Const::CAM_YAW_SPEED   * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_UP)    == GLFW_PRESS) app.cam.pitch += Const::CAM_PITCH_SPEED * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_DOWN)  == GLFW_PRESS) app.cam.pitch -= Const::CAM_PITCH_SPEED * dt;
        app.cam.pitch = glm::clamp(app.cam.pitch, Const::CAM_PITCH_MIN, Const::CAM_PITCH_MAX);

        bool arrow_held = glfwGetKey(app.window.handle, GLFW_KEY_LEFT)  == GLFW_PRESS
                    || glfwGetKey(app.window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS
                    || glfwGetKey(app.window.handle, GLFW_KEY_UP)    == GLFW_PRESS
                    || glfwGetKey(app.window.handle, GLFW_KEY_DOWN)  == GLFW_PRESS;

        // exp decay spring-back to behind trike while driving
        if (app.player.mode == PLAYER_DRIVING && std::abs(app.trike.speed) > 0.3f && !arrow_held)
            app.cam.yaw = glm::mix(app.cam.yaw, 0.0f, 1.0f - std::exp(-3.5f * dt));

        /******************************************************************************
         FIXED-TIMESTEP PHYSICS (120 Hz)
        real time accumulated and consumed in fixed chunks for stable sim
        ******************************************************************************/
        app.accumulator += dt;
        while (app.accumulator >= Const::FIXED_TIMESTEP){
            trike_physics_update(app.trike, input, app.map.terrain, Const::FIXED_TIMESTEP);
            app.accumulator -= Const::FIXED_TIMESTEP;
        }
        trike_model_update(app.scene.trike_model, app.trike.speed, dt);

        if (!app.trike.is_tipping && !app.trike.is_rolled_over)
            aabb_update(app.trike.aabb, app.trike.position, app.trike.heading);

        if (app.trike.impact_timer > 0.0f) app.trike.impact_timer -= dt;

        float pre_collision_speed = app.trike.speed;
        bool any_collision = false;
        collision_static_update(app, dt, pre_collision_speed, any_collision);

        // clamp speed to pre-collision value after all responses
        // without this, throttle held during wall contact gives free acceleration
        if (any_collision){
            float max_spd = std::abs(pre_collision_speed);
            if (std::abs(app.trike.speed) > max_spd + 0.5f)
                app.trike.speed = std::copysign(max_spd, app.trike.speed);
            app.trike.lateral_speed = 0.0f;
        }

        collision_dynamic_update(app, dt);
        collision_dynamic_vs_dynamic(app);

        npc_update(app, dt, s_q_pickup);

        /******************************************************************************
         CAMERA
        ******************************************************************************/
        proj = glm::perspective(
            glm::radians(Const::CAM_FOV),
            (float)Const::WINDOW_WIDTH / (float)Const::WINDOW_HEIGHT,
            Const::CAM_NEAR, Const::CAM_FAR);
        view = cam_update(app.cam, app, dt, arrow_held);

        /******************************************************************************
         AUDIO UPDATE
        ******************************************************************************/
        {
            glm::vec3 lis_pos = (app.player.mode == PLAYER_FOOT)
                ? app.player.pos : app.trike.position;
            glm::vec3 lis_fwd = glm::vec3(std::cos(app.trike.heading), 0.0f, std::sin(app.trike.heading));
            bool driving = (app.player.mode == PLAYER_DRIVING || app.player.mode == PLAYER_MOUNTING);
            audio_update(app.audio, dt, lis_pos, lis_fwd,
                app.trike.speed, Const::TRIKE_MAX_SPEED, driving);
            audio_update_env(app.audio, dt, lis_pos,
                app.map.ambience_zones, app.map.ambience_count, app.scene.night_factor);
        }

    } // end !settings_open input+physics+audio gate

    /******************************************************************************
     RENDER
    ******************************************************************************/
    glClearColor(Const::CLEAR_R, Const::CLEAR_G, Const::CLEAR_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // SHADOW PASS
    scene_update_daytime(app.scene, dt);
    app.editor_renderer.sun_dir        = app.scene.sun_dir;
    app.editor_renderer.light_color    = app.scene.light_color;
    app.editor_renderer.ambient        = app.scene.ambient;
    app.editor_renderer.diff_intensity = app.scene.diff_intensity;
    app.editor_renderer.shadow_cull_center = app.trike.position;
    scene_shadow_pass(app.scene, app.obstacles, app.trike.position);

    if (my_settings.render_shadows){
        glBindFramebuffer(GL_FRAMEBUFFER, app.scene.shadow_fbo);
        glViewport(0, 0, my_settings.shadow_map_size, my_settings.shadow_map_size);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        editor_renderer_shadow_pass(app.editor_renderer, app.map,
            app.scene.light_space_mat, app.dynamic_sims);
        scene_trike_shadow_draw(app.scene, app.trike);
        driver_model_draw(app.scene.driver_model, app.player, app.trike,
            app.scene.shadow_shader, app.scene.light_space_mat, glm::mat4(1.0f),
            app.editor.pose_quat, app.editor.pose_offset, app.editor.pose_seat);

        for (const auto& npc : app.npcs){
            glm::vec3 dnpc = npc.position - app.trike.position;
            dnpc.y = 0.0f;
            float npc_cull_sq = my_settings.npc_cull_dist * my_settings.npc_cull_dist;
            if (glm::dot(dnpc, dnpc) > npc_cull_sq) continue;
            auto it = app.npc_model_cache.find(npc.model_path);
            DriverModel* mdl = (it != app.npc_model_cache.end())
                ? &it->second : &app.scene.driver_model;
            npc_draw(npc, *mdl, app.scene.shadow_shader,
                app.scene.light_space_mat, glm::mat4(1.0f));
        }
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else {
        // wipe the depth map to white (1.0) so the lit shader sees no shadow
        // without this, the stale FBO content projects ghost shadows when toggled off
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

    app.editor_renderer.shadow_depth_tex = app.scene.shadow_depth_tex;
    app.editor_renderer.light_space_mat  = app.scene.light_space_mat;
    app.editor_renderer.night_factor = app.scene.night_factor;
    app.editor_renderer.fog_color = app.scene.fog_color;
    app.editor_renderer.fog_near = app.scene.fog_near;
    app.editor_renderer.fog_far = app.scene.fog_far;

    std::vector<LightSource> frame_lights = app.map.lights;
    if (app.player.headlights_on && app.player.mode == PLAYER_DRIVING)
        frame_lights.push_back(trike_headlight(app.trike.position, app.trike.heading));

    // build flash map from current hit timers
    std::map<int, float> flash_map;
    for (const auto& obs : app.obstacles)
        if (obs.world_id != -1 && obs.hit_timer > 0.0f)
            flash_map[obs.world_id] = obs.hit_timer;
    for (const auto& [id, sim] : app.dynamic_sims)
        if (sim.hit_timer > 0.0f)
            flash_map[id] = sim.hit_timer;

    // PLANAR REFLECTION PASS
    // renders the world mirrored across the water plane into scene.reflect_fbo
    // ocean.frag samples this texture for nearby prop/trike reflections
    if (app.map.ocean.enabled){
        glm::mat4 refl_view = scene_build_reflect_view(view, app.map.ocean.y_level);
        app.scene.reflect_view_proj = proj * refl_view;

        glBindFramebuffer(GL_FRAMEBUFFER, app.scene.reflect_fbo);
        glViewport(0, 0, app.scene.reflect_w, app.scene.reflect_h);
        glClearColor(Const::CLEAR_R, Const::CLEAR_G, Const::CLEAR_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // mirroring the view flips triangle winding, cull the opposite face
        glFrontFace(GL_CW);

        scene_draw_sky(app.scene, refl_view, proj);
        scene_draw(app.scene, app.trike, app.obstacles, frame_lights, refl_view, proj, false);
        editor_renderer_draw_roads(app.editor_renderer, app.map.roads, refl_view, proj);
        editor_renderer_draw_terrain_surface(app.editor_renderer, app.map.terrain, refl_view, proj, app.map.ocean);
        editor_renderer_draw_props(app.editor_renderer, app.map, refl_view, proj,
            flash_map, app.dynamic_sims, frame_lights, true);
        scene_draw_driver(app.scene, app.player, app.trike, refl_view, proj,
            app.editor_renderer.obj_shader,
            app.editor.pose_quat, app.editor.pose_offset, app.editor.pose_seat);
        for (const auto& npc : app.npcs){
            glm::vec3 d = npc.position - app.trike.position;
            d.y = 0.0f;
            float npc_cull_sq = my_settings.npc_cull_dist * my_settings.npc_cull_dist;
            if (glm::dot(d, d) > npc_cull_sq) continue;
            auto it = app.npc_model_cache.find(npc.model_path);
            DriverModel* mdl = (it != app.npc_model_cache.end()) ? &it->second : &app.scene.driver_model;
            npc_draw(npc, *mdl, app.editor_renderer.obj_shader, refl_view, proj);
        }

        glFrontFace(GL_CCW);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int fb_w, fb_h;
        glfwGetFramebufferSize(app.window.handle, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
    }
    app.editor_renderer.reflect_tex = app.scene.reflect_color_tex;
    app.editor_renderer.reflect_view_proj = app.scene.reflect_view_proj;

    scene_draw_sky(app.scene, view, proj);
    scene_draw(app.scene, app.trike, app.obstacles, frame_lights, view, proj, app.editor.show_hitboxes);

    editor_renderer_draw_roads(app.editor_renderer, app.map.roads, view, proj);
    editor_renderer_draw_terrain_surface(app.editor_renderer, app.map.terrain, view, proj,
        app.map.ocean);
    editor_renderer_draw_ocean(app.editor_renderer, app.map.ocean, app.map.terrain, view, proj, dt,
        app.map.terrain.origin.x,
        app.map.terrain.origin.x + app.map.terrain.cols * app.map.terrain.cell_size,
        app.map.terrain.origin.z,
        app.map.terrain.origin.z + app.map.terrain.rows * app.map.terrain.cell_size);
    editor_renderer_draw_props(app.editor_renderer, app.map, view, proj,
        flash_map, app.dynamic_sims, frame_lights, true);
    scene_draw_driver(app.scene, app.player, app.trike, view, proj,
        app.editor_renderer.obj_shader,
        app.editor.pose_quat, app.editor.pose_offset, app.editor.pose_seat);

    for (const auto& npc : app.npcs){
        glm::vec3 d = npc.position - app.trike.position;
        d.y = 0.0f;
        float npc_cull_sq = my_settings.npc_cull_dist * my_settings.npc_cull_dist;
        if (glm::dot(d, d) > npc_cull_sq) continue;
        auto it = app.npc_model_cache.find(npc.model_path);
        DriverModel* mdl = (it != app.npc_model_cache.end())
            ? &it->second : &app.scene.driver_model;
        npc_draw(npc, *mdl, app.editor_renderer.obj_shader, view, proj);
    }

    // DESTINATION MARKER - glowing ring + HUD arrow while passenger is riding
    if (app.passenger_npc_id != -1){
        for (const auto& npc : app.npcs){
            if (npc.id != app.passenger_npc_id) continue;

            glm::vec3 drop = npc.drop_point;
            drop.y = heightfield_sample(app.map.terrain, drop.x, drop.z);
            float pulse = 0.85f + 0.15f * std::sin((float)glfwGetTime() * 3.0f);
            scene_draw_drop_marker(app.scene, drop, pulse, view, proj);

            glm::vec3 to_drop = drop - app.trike.position;
            to_drop.y = 0.0f;
            float dist_to_drop = glm::length(to_drop);
            if (dist_to_drop > 5.0f){
                glm::vec3 trike_fwd = { std::cos(app.trike.heading), 0.0f, std::sin(app.trike.heading) };
                glm::vec3 dir_to_drop = to_drop / dist_to_drop;
                float dot    = glm::dot(trike_fwd, dir_to_drop);
                float cross_y = trike_fwd.x * dir_to_drop.z - trike_fwd.z * dir_to_drop.x;
                hud_draw_direction_arrow(app.hud, dot, cross_y, dist_to_drop);
            }
            break;
        }
    }

    // RAIN
    glm::vec3 rain_origin = (app.player.mode == PLAYER_FOOT) ? app.player.pos : app.trike.position;
    float rain_speed = (app.player.mode == PLAYER_FOOT) ? app.player.speed : app.trike.speed;
    if (!app.editor.settings_open){
        app.scene.sky_rain_target = app.rain.active ? 1.0f : 0.0f;
        rain_tick_trigger(app.rain, dt);
        audio_rain_set(app.audio, app.rain.active);
        rain_tick_thunder(app.rain, dt, "../assets");
        if (app.rain.thunder_boom_pending && app.rain.thunder_audio_delay <= 0.0f){
            app.rain.thunder_boom_pending = false;
            audio_trigger_voice_local(app.audio, "../assets/audio/ambience/thunder.wav");
        }
        rain_update(app.rain, dt, rain_origin, rain_speed, app.trike.heading, app.map.terrain);
    }
    rain_draw(app.rain, view, proj, rain_origin, rain_speed, app.trike.heading);

    std::string radio_track = (app.audio.radio_on && app.audio.radio_index < (int)app.audio.radio_playlist.size())
        ? app.audio.radio_playlist[app.audio.radio_index] : "";
    bool driving = (app.player.mode == PLAYER_DRIVING || app.player.mode == PLAYER_MOUNTING);
    if (my_settings.show_hud && driving) hud_draw(app.hud, app.trike, app.passenger_npc_id != -1, app.passenger_fare, app.audio.radio_on, radio_track);
    if (app.editor.settings_open){
        editor_input_settings(app.editor, app.window.handle);
        editor_renderer_draw_settings_menu(app.editor_renderer, app.editor);
    }
    window_swap_buffers(app.window);
    window_poll_events();
}