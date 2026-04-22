#include "app.hpp"
#include "const.hpp"
#include "../renderer/shader.hpp"
#include "../renderer/mesh.hpp"
#include "../tricycle/tricycle_mesh.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

static const char* VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_color;

void main()
{
    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);
    v_color = a_color;
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in  vec3 v_color;
out vec4 frag_color;

void main(){
    frag_color = vec4(v_color, 1.0);
}
)";

// app state
static Shader s_shader;
static Mesh   s_trike_mesh;

// camera orbit state lets us spin around the trike with arrow keys
static float s_cam_yaw   = 45.0f;   // degrees around Y axis
static float s_cam_pitch = 20.0f;   // degrees above ground
static float s_cam_dist  = 5.5f;    // metres from origin

void app_init(App& app)
{
    window_init(app.window,
        Const::WINDOW_WIDTH,
        Const::WINDOW_HEIGHT,
        Const::WINDOW_TITLE);

    glEnable(GL_DEPTH_TEST);

    shader_init(s_shader, VERT_SRC, FRAG_SRC);

    std::vector<float> verts;
    build_tricycle_mesh(verts);
    mesh_init(s_trike_mesh, verts);

    app.last_time   = (float)glfwGetTime();
    app.accumulator = 0.0f;
    app.running     = true;
}

void app_run(App& app)
{
    while (!window_should_close(app.window))
    {
        // delta time
        float current_time = (float)glfwGetTime();
        float dt           = current_time - app.last_time;
        app.last_time      = current_time;
        if (dt > Const::MAX_DELTA) dt = Const::MAX_DELTA;

        // camera orbit input
        // TEMP: orbit camera with arrow keys so we can inspect the mesh
        // from all angles before we attach it to physics
        if (glfwGetKey(app.window.handle, GLFW_KEY_LEFT)  == GLFW_PRESS) s_cam_yaw   -= 60.0f * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS) s_cam_yaw   += 60.0f * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_UP)    == GLFW_PRESS) s_cam_pitch += 40.0f * dt;
        if (glfwGetKey(app.window.handle, GLFW_KEY_DOWN)  == GLFW_PRESS) s_cam_pitch -= 40.0f * dt;
        s_cam_pitch = glm::clamp(s_cam_pitch, 5.0f, 85.0f);

        // fixed timestep
        app.accumulator += dt;
        while (app.accumulator >= Const::FIXED_TIMESTEP)
        {
            app.accumulator -= Const::FIXED_TIMESTEP;
        }

        // camera matrices
        // convert yaw/pitch to a world-space eye position orbiting the origin
        // this gives us a free inspection camera with zero physics attached
        float yaw_r   = glm::radians(s_cam_yaw);
        float pitch_r = glm::radians(s_cam_pitch);

        glm::vec3 eye = {
            s_cam_dist * cosf(pitch_r) * sinf(yaw_r),
            s_cam_dist * sinf(pitch_r),
            s_cam_dist * cosf(pitch_r) * cosf(yaw_r)
        };

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(eye, glm::vec3(0.8f, 0.4f, 0.5f), glm::vec3(0,1,0));
        glm::mat4 proj  = glm::perspective(
            glm::radians(60.0f),
            (float)Const::WINDOW_WIDTH / (float)Const::WINDOW_HEIGHT,
            0.1f, 100.0f
        );

        // render
        glClearColor(Const::CLEAR_R, Const::CLEAR_G, Const::CLEAR_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_bind(s_shader);
        shader_set_mat4(s_shader, "u_model", glm::value_ptr(model));
        shader_set_mat4(s_shader, "u_view",  glm::value_ptr(view));
        shader_set_mat4(s_shader, "u_proj",  glm::value_ptr(proj));

        mesh_draw(s_trike_mesh);

        window_swap_buffers(app.window);
        window_poll_events();
    }
}

void app_shutdown(App& app)
{
    mesh_destroy(s_trike_mesh);
    shader_destroy(s_shader);
    window_destroy(app.window);
    app.running = false;
}