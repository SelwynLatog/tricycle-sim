#include "app.hpp"
#include "const.hpp"
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
#include <iostream>

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

void main(){
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
    // transform normal into world space
    v_world_normal = normalize(u_normal_mat * a_normal);
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in  vec3 v_world_normal;
out vec4 frag_color;

uniform vec3 u_kd;          // diffuse color from MTL
uniform vec3 u_light_dir;   // normalized direction TOWARD the light (world space)

void main(){
    // Lambert diffuse: how much does this surface face the light?
    float diff = max(dot(normalize(v_world_normal), u_light_dir), 0.0);

    // ambient: a base brightness so nothing goes fully black
    float ambient = 0.55;

    vec3 color = u_kd * (ambient + diff * 0.85);
    frag_color = vec4(color, 1.0);
}
)";

// app state

static Shader  s_shader;
static ObjMesh s_trike;
static Mesh s_ground;    // flat asphalt quad, pos+normal layout

// orbit camera
static float s_cam_yaw= 45.0f;
static float s_cam_pitch= 25.0f;
static float s_cam_dist= 5.0f;
static glm::vec3 s_model_center= glm::vec3(0.0f);
static float s_model_scale= 1.0f;

// light direction: coming from upper-front-right in world space
static const glm::vec3 LIGHT_DIR = glm::normalize(glm::vec3(1.0f, 2.0f, 1.0f));

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
    s_model_center.y -= 0.03f;

    // auto-scale: normalize longest axis to 2 metres
    float longest = std::max(maxX-minX, std::max(maxY-minY, maxZ-minZ));
    s_model_scale = (longest > 0.0f) ? 2.0f / longest : 1.0f;

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

        // camera orbit input
        if (glfwGetKey(app.window.handle, GLFW_KEY_LEFT)  == GLFW_PRESS) s_cam_yaw   -= 60.0f * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS) s_cam_yaw   += 60.0f * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_UP)    == GLFW_PRESS) s_cam_pitch += 40.0f * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_DOWN)  == GLFW_PRESS) s_cam_pitch -= 40.0f * dt;
        s_cam_pitch = glm::clamp(s_cam_pitch, 5.0f, 85.0f);

        // fixed timestep (physics stub)
        app.accumulator += dt;
        while (app.accumulator >= Const::FIXED_TIMESTEP)
            app.accumulator -= Const::FIXED_TIMESTEP;

        // camera matrices
        float yaw_r   = glm::radians(s_cam_yaw);
        float pitch_r = glm::radians(s_cam_pitch);

        // scale then translate to origin — model always fits in 2m regardless of source units
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(s_model_scale));
        model = glm::translate(glm::mat4(1.0f), -s_model_center * s_model_scale) * model;

        glm::vec3 eye = {
            s_cam_dist * cosf(pitch_r) * sinf(yaw_r),
            s_cam_dist * sinf(pitch_r),
            s_cam_dist * cosf(pitch_r) * cosf(yaw_r)
        };

        glm::vec3 target = glm::vec3(0.0f, 0.5f, 0.0f);
        glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(
            glm::radians(55.0f),
            (float)Const::WINDOW_WIDTH / (float)Const::WINDOW_HEIGHT,
            0.1f, 200.0f
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
        glm::mat4 ground_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.002f, 0.0f));
        glm::mat3 ground_normal_mat = glm::mat3(1.0f);
        shader_set_mat4(s_shader, "u_model", glm::value_ptr(ground_model));
        shader_set_mat3(s_shader, "u_normal_mat", glm::value_ptr(ground_normal_mat));
        shader_set_vec3_v(s_shader, "u_kd", glm::vec3(0.18f, 0.18f, 0.18f)); // dark asphalt
        glBindVertexArray(s_ground.vao);
        glDrawArrays(GL_TRIANGLES, 0, s_ground.count);
        glBindVertexArray(0);

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

        window_swap_buffers(app.window);
        window_poll_events();
    }
}

// app_shutdown
void app_shutdown(App& app){
    mesh_destroy(s_ground);
    obj_mesh_destroy(s_trike);
    shader_destroy(s_shader);
    window_destroy(app.window);
    app.running = false;
}