#include "app.hpp"
#include "const.hpp"
#include "../physics/trike_aabb.hpp"
#include "../renderer/shader.hpp"
#include "../renderer/mesh.hpp"
#include "../renderer/mesh_builder.hpp"
#include "../renderer/obj_loader.hpp"
#include "../renderer/obj_mesh.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <cstdio>

//  shaders
//  vertex: transforms pos + normal into clip space + world space
//  fragment: lambert diffuse (dot(N, L)) * Kd + small ambient lift

static const char* VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
// normal matrix: transpose of inverse of model — keeps normals correct under non-uniform scale
uniform mat3 u_normal_mat;

out vec3 v_world_normal;
out vec3 v_world_pos;

void main(){
    vec4 world= u_model * vec4(a_pos, 1.0);
    gl_Position = u_proj * u_view * world;
    // transform normal into world space
    v_world_normal = normalize(u_normal_mat * a_normal);
    v_world_pos= world.xyz;
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in  vec3 v_world_normal;
in  vec3 v_world_pos;
out vec4 frag_color;

uniform vec3 u_kd;          // diffuse color from MTL
uniform vec3 u_kd_alt;
uniform vec3 u_light_dir;   // normalized direction TOWARD the light (world space)
uniform float u_checker_scale;
uniform int u_use_checker;

void main(){
    // Lambert diffuse: how much does this surface face the light?
    float diff = max(dot(normalize(v_world_normal), u_light_dir), 0.0);

    // ambient: a base brightness so nothing goes fully black
    float ambient = 0.55;

    vec3 lit = u_kd * (ambient + diff * 0.85);

    // checkboard ground tile
    // alternate tile color based on world XZ pos
    // only applied when u_use_checker=1 
    if (u_use_checker==1){
        vec2 tile = floor(v_world_pos.xz * u_checker_scale);
        float parity = mod(tile.x + tile.y, 2.0);
        vec3 kd= mix(u_kd, u_kd_alt, parity);
        lit= kd * (ambient + diff * 0.85);
    }

    frag_color= vec4(lit,1.0);
}
)";

static const char* GIZMO_VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec3 v_color;
void main(){
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
    v_color = a_color;
}
)";

static const char* GIZMO_FRAG_SRC = R"(
#version 330 core
in  vec3 v_color;
out vec4 frag_color;
void main(){
    frag_color = vec4(v_color, 1.0);
}
)";

// app state

static Shader  s_shader;
static ObjMesh s_trike;
static Mesh s_ground;    // flat asphalt quad, pos+normal layout

static Shader s_gizmo_shader;
static Mesh s_gizmo;

// orbit camera
static float s_cam_yaw= Const::CAM_YAW_DEFAULT;
static float s_cam_pitch= Const::CAM_PITCH_DEFAULT;
static float s_cam_dist= Const::CAM_DIST_DEFAULT;
static glm::vec3 s_model_center= glm::vec3(0.0f);
static float s_model_scale= 1.0f;
static float s_debug_print_timer = 0.0f;
static glm::vec3 s_cam_pos = glm::vec3(-6.0f, 3.0f, 0.0f); //smoothed cam world
static const glm::vec3 LIGHT_DIR = glm::normalize(glm::vec3(Const::LIGHT_DIR_X, Const::LIGHT_DIR_Y, Const::LIGHT_DIR_Z));
static bool s_free_cam = false;
static bool s_f_pressed_last = false;
static float s_model_half_height = 1.0f;

// helpers
static void shader_set_vec3_v(const Shader& s, const char* name, glm::vec3 v){
    glUniform3f(glGetUniformLocation(s.id, name), v.x, v.y, v.z);
}

static void shader_set_mat3(const Shader& s, const char* name, const float* value){
    glUniformMatrix3fv(glGetUniformLocation(s.id, name), 1, GL_FALSE, value);
}

// app_init

void app_init(App& app){
    window_init(app.window,
        Const::WINDOW_WIDTH,
        Const::WINDOW_HEIGHT,
        Const::WINDOW_TITLE);

    glEnable(GL_DEPTH_TEST);

    shader_init(s_shader, VERT_SRC, FRAG_SRC);

    // load trike OBJ
    ObjData data;
    if (!obj_load("../assets/TRAYSIKEL.obj", data))
        std::cerr << "[app] failed to load tricycle OBJ\n";
    obj_mesh_init(s_trike, std::move(data));

    // compute bounding box to auto-center and auto-scale any model
    float minX=1e9,maxX=-1e9,minY=1e9,maxY=-1e9,minZ=1e9,maxZ=-1e9;
    for (int i = 0; i < (int)s_trike.data.vertices.size(); i += 6) {
        float x = s_trike.data.vertices[i];
        float y = s_trike.data.vertices[i+1];
        float z = s_trike.data.vertices[i+2];
        minX=std::min(minX,x); maxX=std::max(maxX,x);
        minY=std::min(minY,y); maxY=std::max(maxY,y);
        minZ=std::min(minZ,z); maxZ=std::max(maxZ,z);
    }
    std::cout << "[bbox] X: " << minX << " to " << maxX << "\n";
    std::cout << "[bbox] Y: " << minY << " to " << maxY << "\n";
    std::cout << "[bbox] Z: " << minZ << " to " << maxZ << "\n";

    // auto-center: middle of the bounding box
    s_model_center = glm::vec3(
        (minX + maxX) * 0.5f,
         minY,
        (minZ + maxZ) * 0.5f
    );

    // temp: nudge manually for now
    // blender models have a slight gap on lowest vertex
    s_model_center.y -= Const::MODEL_FLOOR_FUDGE;

    // auto-scale: normalize longest axis to 2 metres
    float longest = std::max(maxX-minX, std::max(maxY-minY, maxZ-minZ));
    s_model_scale = (longest > 0.0f) ? Const::MODEL_NORMALIZE_SIZE / longest : 1.0f;

    // scaled half-height: distance from model center to top/bottom in world units
    float raw_half_height = (maxY - minY) * 0.5f;
    s_model_half_height = raw_half_height * s_model_scale;

    std::cout << "[bbox] center: ("
        << s_model_center.x << ", "
        << s_model_center.y << ", "
        << s_model_center.z << ")\n";
    std::cout << "[bbox] auto scale: " << s_model_scale << "\n";

    // ground plane - large flat quad at Y=0 using pos+normal layout
    // shares the same shader as the trike, driven by u_kd uniform at draw time
    std::vector<float> ground_verts;
    push_ground_quad(ground_verts, 200.0f);
    mesh_init(s_ground, ground_verts);

    // axis gizmo at world origin
    shader_init(s_gizmo_shader, GIZMO_VERT_SRC, GIZMO_FRAG_SRC);
    std::vector<float> gizmo_verts;
    push_axis_gizmo(gizmo_verts, Const::GIZMO_LENGTH);
    mesh_init(s_gizmo, gizmo_verts);

    // spawn static obstacles
    // position in center-bottom
    // half extents is half width/height/depth
    app.obstacles.push_back(make_obstacle({10.0f, 0.0f,  0.0f}, {0.75f, 1.0f, 0.75f})); // dead ahead
    app.obstacles.push_back(make_obstacle({ 0.0f, 0.0f, 10.0f}, {1.0f,  1.5f, 0.5f}));  // off to the side
    app.obstacles.push_back(make_obstacle({15.0f, 0.0f,  8.0f}, {0.5f,  0.8f, 0.5f}));  // further out


    hud_init(app.hud, Const::WINDOW_WIDTH, Const::WINDOW_HEIGHT);

    app.last_time= (float)glfwGetTime();
    app.accumulator= 0.0f;
    app.running= true;
}

// app_run
void app_run(App& app){
    while (!window_should_close(app.window))
    {
        // delta time
        float now = (float)glfwGetTime();
        float dt  = now - app.last_time;
        app.last_time = now;
        if (dt > Const::MAX_DELTA) dt = Const::MAX_DELTA;
        
        // driving input
        TrikeInput input;
        input.throttle = (glfwGetKey(app.window.handle, GLFW_KEY_W) == GLFW_PRESS) ? 1.0f : 0.0f;
        input.brake    = (glfwGetKey(app.window.handle, GLFW_KEY_S) == GLFW_PRESS) ? 1.0f : 0.0f;
        input.steer    = 0.0f;
        if (glfwGetKey(app.window.handle, GLFW_KEY_A) == GLFW_PRESS) input.steer -= 1.0f;
        if (glfwGetKey(app.window.handle, GLFW_KEY_D) == GLFW_PRESS) input.steer += 1.0f;
        bool f_down = glfwGetKey(app.window.handle, GLFW_KEY_F) == GLFW_PRESS;
        if (f_down && !s_f_pressed_last) s_free_cam = !s_free_cam;
        s_f_pressed_last = f_down;

        // camera orbit input
        if (glfwGetKey(app.window.handle, GLFW_KEY_LEFT)  == GLFW_PRESS) s_cam_yaw   -= Const::CAM_YAW_SPEED   * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS) s_cam_yaw   += Const::CAM_YAW_SPEED   * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_UP)    == GLFW_PRESS) s_cam_pitch += Const::CAM_PITCH_SPEED * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_DOWN)  == GLFW_PRESS) s_cam_pitch -= Const::CAM_PITCH_SPEED * dt;
        s_cam_pitch = glm::clamp(s_cam_pitch, Const::CAM_PITCH_MIN, Const::CAM_PITCH_MAX);

        // fixed timestep
        app.accumulator += dt;
        while (app.accumulator >= Const::FIXED_TIMESTEP){
            trike_physics_update(app.trike, input, Const::FIXED_TIMESTEP);
            app.accumulator -= Const::FIXED_TIMESTEP;
        }

        // update AABB to match current physics state
        aabb_update(app.trike.aabb, app.trike.position, app.trike.heading);
        
        // camera matrices
        float yaw_r   = glm::radians(s_cam_yaw);
        float pitch_r = glm::radians(s_cam_pitch);

        // build trike world transform from physics state
        // order: scale -> center -> physics rotation -> physics pos
        // for current model the default obj is sideways as I thought when I added axis indicators it was goofy as hell
        // for now the easiest fix is just adding an extra 90 degrees rotation
        // TODO: Add model import config- small per model descriptor that sits
        // alongside the OBJ file and specifies axis remapping, scale override,and floor fudge
        float wheel_radius = 0.28f * s_model_scale;

        // TEMP: there is no collision implementation so hitboxes are nonexistent
        // rollover is goofy because it goes under y ground plane
        // in the future this will be cleaned alright its already late
        glm::vec3 render_pos = app.trike.position;
        if (app.trike.is_tipping) {
            render_pos.y = s_model_half_height * std::abs(std::cos(app.trike.roll_angle));
        } else if (app.trike.is_rolled_over) {
            render_pos.y = 0.0f;
        }

        // scaled center in model space
        glm::vec3 scaled_center = s_model_center * s_model_scale;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), render_pos)
                        * glm::rotate(glm::mat4(1.0f), app.trike.heading, glm::vec3(0, 1, 0))
                        * glm::rotate(glm::mat4(1.0f), app.trike.roll_angle, glm::vec3(1, 0, 0))
                        * glm::rotate(glm::mat4(1.0f), glm::radians(Const::TRIKE_MODEL_YAW_OFFSET), glm::vec3(0, 1, 0))
                        * glm::translate(glm::mat4(1.0f), -scaled_center)
                        * glm::scale(glm::mat4(1.0f), glm::vec3(s_model_scale));
        
        // chase cam: orbit origin is behind the trike based on its heading
        // yaw_r is the manual orbit offset on top of the trike's heading
        // added lerp + look ahead

        // why this? instead of just app.trike.heading + yaw_r?
        // because the OBJ model is stupidly set to side orientation by default
        // this causes the trike to look like its sliding even though direction is set forward
        // if i can actually model a decent mesh this wouldn't be a problem, but I can't (skill issue)
        // so i had to download a free trike model
        float cam_yaw_world = app.trike.heading + yaw_r+ glm::radians(180.0f);

        glm::vec3 cam_origin = app.trike.position + glm::vec3(0.0f, Const::CAM_ORBIT_TARGET_Y, 0.0f);

        glm::vec3 ideal_eye = cam_origin + glm::vec3(
            s_cam_dist * cosf(pitch_r) * cosf(cam_yaw_world),
            s_cam_dist * sinf(pitch_r),
            s_cam_dist * cosf(pitch_r) * sinf(cam_yaw_world)
        );

        // lerp cam toward ideal position
        // this smooths out snapping on sharp turns
        s_cam_pos= glm::mix(s_cam_pos, ideal_eye, Const::CAM_LERP_SPEED * dt);

        // look-ahead
        // shift target forward proportional to speed
        // so at high speed cam see the road ahead, not just the trike's back
        float fwd_angle = app.trike.heading;
        glm::vec3 forward = glm::vec3(cosf(fwd_angle), 0.0f, sinf(fwd_angle));
        float lookahead= (app.trike.speed / Const::TRIKE_MAX_SPEED) * Const::CAM_LOOKAHEAD;
        glm::vec3 target= cam_origin + forward * lookahead;
        
        glm::mat4 view;
        if (s_free_cam) {
            glm::vec3 debug_eye = app.trike.position + glm::vec3(0.0f, 15.0f, 0.0f);
            view = glm::lookAt(debug_eye, app.trike.position, glm::vec3(1, 0, 0));
        } else {
            view = glm::lookAt(s_cam_pos, target, glm::vec3(0, 1, 0));
        }

        glm::mat4 proj = glm::perspective(
            glm::radians(Const::CAM_FOV),
            (float)Const::WINDOW_WIDTH / (float)Const::WINDOW_HEIGHT,
            Const::CAM_NEAR, Const::CAM_FAR
        );

        glm::mat3 normal_mat = glm::mat3(glm::transpose(glm::inverse(model)));

        // render
        glClearColor(Const::CLEAR_R, Const::CLEAR_G, Const::CLEAR_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_bind(s_shader);
        shader_set_mat4(s_shader, "u_view", glm::value_ptr(view));
        shader_set_mat4(s_shader, "u_proj", glm::value_ptr(proj));
        shader_set_vec3_v(s_shader, "u_light_dir", LIGHT_DIR);

        // draw ground
        // ground sits at world origin, no model transform needed
        // had a hard time rendering ground, was conceptually correct
        // issue was how transformed was applied
        // fix is to push ground down by smallest epsilon to avoid z-fighting
        glm::mat4 ground_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, Const::GROUND_Y_OFFSET, 0.0f));
        glm::mat3 ground_normal_mat = glm::mat3(1.0f);
        shader_set_mat4(s_shader, "u_model", glm::value_ptr(ground_model));
        shader_set_mat3(s_shader, "u_normal_mat", glm::value_ptr(ground_normal_mat));
        shader_set_vec3_v(s_shader, "u_kd", glm::vec3(Const::GROUND_KD)); // dark asphalt

        // draw ground grid
        glUniform3f(glGetUniformLocation(s_shader.id, "u_kd_alt"),
            Const::GROUND_KD_ALT, Const::GROUND_KD_ALT, Const::GROUND_KD_ALT);
        glUniform1f(glGetUniformLocation(s_shader.id, "u_checker_scale"),
            1.0f / Const::GROUND_GRID_TILE_SIZE);
        glUniform1i(glGetUniformLocation(s_shader.id, "u_use_checker"), 1);

        glBindVertexArray(s_ground.vao);
        glDrawArrays(GL_TRIANGLES, 0, s_ground.count);
        glBindVertexArray(0);

        glUniform1i(glGetUniformLocation(s_shader.id, "u_use_checker"), 0);

        // draw axis gizmo at world origin
        shader_bind(s_gizmo_shader);
        shader_set_mat4(s_gizmo_shader, "u_view", glm::value_ptr(view));
        shader_set_mat4(s_gizmo_shader, "u_proj", glm::value_ptr(proj));
        glBindVertexArray(s_gizmo.vao);
        glDrawArrays(GL_LINES, 0, s_gizmo.count);
        glBindVertexArray(0);
        shader_bind(s_shader);

        // draw trike
        shader_set_mat4(s_shader, "u_model", glm::value_ptr(model));
        shader_set_mat3(s_shader, "u_normal_mat", glm::value_ptr(normal_mat));

        for (int i = 0; i < (int)s_trike.data.groups.size(); ++i)
        {
            const ObjGroup& grp = s_trike.data.groups[i];
            const ObjMaterial* mat = obj_find_material(s_trike.data, grp.mat_name);

            glm::vec3 kd = mat ? mat->kd : glm::vec3(0.8f);
            shader_set_vec3_v(s_shader, "u_kd", kd);

            obj_mesh_draw_group(s_trike, i);
        }

        // draw AABB wireframe
        // draw obstacle AABB
        auto draw_aabb_wire= [&](const Aabb& box, glm::vec3 color){
            glm::vec3 lo = box.min, hi = box.max;

            glm::vec3 corners[8] = {
                {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z},
                {hi.x, lo.y, hi.z}, {lo.x, lo.y, hi.z},
                {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z},
                {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z},
            };
            int edges[24] = {
                0,1, 1,2, 2,3, 3,0,
                4,5, 5,6, 6,7, 7,4,
                0,4, 1,5, 2,6, 3,7
            };

            std::vector<float> wire_verts;
            for (int e = 0; e < 24; e += 2){
                glm::vec3 a = corners[edges[e]];
                glm::vec3 b = corners[edges[e+1]];
                wire_verts.insert(wire_verts.end(), {a.x,a.y,a.z, color.r,color.g,color.b});
                wire_verts.insert(wire_verts.end(), {b.x,b.y,b.z, color.r,color.g,color.b});
            }
               
            Mesh wire_mesh;
            mesh_init(wire_mesh, wire_verts);
            shader_bind(s_gizmo_shader);
            shader_set_mat4(s_gizmo_shader, "u_view", glm::value_ptr(view));
            shader_set_mat4(s_gizmo_shader, "u_proj", glm::value_ptr(proj));
            glBindVertexArray(wire_mesh.vao);
            glDrawArrays(GL_LINES, 0, wire_mesh.count);
            glBindVertexArray(0);
            mesh_destroy(wire_mesh);
            shader_bind(s_shader);
        };

        draw_aabb_wire(app.trike.aabb, {0.0f, 1.0f, 0.3f});

        for (const auto& obs : app.obstacles)
             draw_aabb_wire(obs.aabb, {1.0f, 0.9f, 0.0f});
             
        hud_draw(app.hud, app.trike);

        window_swap_buffers(app.window);
        window_poll_events();
    }
}

// app_shutdown
void app_shutdown(App& app){
    hud_destroy(app.hud);
    mesh_destroy(s_gizmo);
    shader_destroy(s_gizmo_shader);
    mesh_destroy(s_ground);
    obj_mesh_destroy(s_trike);
    shader_destroy(s_shader);
    window_destroy(app.window);
    app.running = false;
}