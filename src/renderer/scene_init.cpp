#include "scene_init.hpp"
#include "../core/settings.hpp"
#include "../core/const.hpp"
#include "mesh_builder.hpp"
#include "obj_loader.hpp"
#include "../../vendor/stb/stb_image.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <cmath>

/**********************************************************************
SCENE INIT
Responsibilities
- Shader compilation (scene, gizmo, sky, shadow)
- Sky/rain/night texture loading
- Shadow map FBO + planar reflection FBO construction
- Trike/driver model or procedural mesh loading
- Ground + gizmo mesh construction
- Uniform location caching (scene/gizmo/sky/shadow/point-light)
- Shadow map resize + GL resource teardown
**********************************************************************/

void scene_init(SceneState& scene){

    shader_init_from_file(scene.shader, "../assets/shaders/scene_lit.vert", "../assets/shaders/scene_lit.frag");
    shader_init_from_file(scene.gizmo_shader, "../assets/shaders/gizmo.vert", "../assets/shaders/gizmo.frag");
    shader_init_from_file(scene.sky_shader, "../assets/shaders/sky.vert", "../assets/shaders/sky.frag");
    {
        // fullscreen triangle-pair quad, NDC coords only, no normals needed
        std::vector<float> sq = {
            -1,-1,  1,-1,  1, 1,
            -1,-1,  1, 1, -1, 1
        };
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sq.size()*sizeof(float), sq.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        scene.sky_quad.vao = vao;
        scene.sky_quad.vbo = vbo;
        scene.sky_quad.count = 6;
    }

    // load sky texture
    if (Const::SKY_IMAGE_PATH[0] != '\0'){
        stbi_set_flip_vertically_on_load(0); // equirectangular: no flip
        int w, h, ch;
        unsigned char* px = stbi_load(Const::SKY_IMAGE_PATH, &w, &h, &ch, 3);
        if (px){
            glGenTextures(1, &scene.sky_tex);
            glBindTexture(GL_TEXTURE_2D, scene.sky_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            stbi_image_free(px);
            std::cout << "[sky] loaded " << w << "x" << h << " " << Const::SKY_IMAGE_PATH << "\n";
        } else {
            std::cerr << "[sky] failed to load: " << Const::SKY_IMAGE_PATH << "\n";
        }
    }


     // load rain/overcast sky
    {
        const char* rain_path = "../assets/props/sky_rain.jpg";
        stbi_set_flip_vertically_on_load(0);
        int w, h, ch;
        unsigned char* px = stbi_load(rain_path, &w, &h, &ch, 3);
        if (px){
            glGenTextures(1, &scene.sky_rain_tex);
            glBindTexture(GL_TEXTURE_2D, scene.sky_rain_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            stbi_image_free(px);
            std::cout << "[sky] loaded rain " << w << "x" << h << "\n";
        }
    }

    // load night sky
    {
        const char* night_path = "../assets/props/sky_night.jpg";
        stbi_set_flip_vertically_on_load(0);
        int w, h, ch;
        unsigned char* px = stbi_load(night_path, &w, &h, &ch, 3);
        if (px){
            glGenTextures(1, &scene.sky_night_tex);
            glBindTexture(GL_TEXTURE_2D, scene.sky_night_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            stbi_image_free(px);
            std::cout << "[sky] loaded night " << w << "x" << h << "\n";
        }
    }


    // shadow map FBO
    shader_init_from_file(scene.shadow_shader, "../assets/shaders/depth.vert", "../assets/shaders/depth.frag");
    glGenFramebuffers(1, &scene.shadow_fbo);
    glGenTextures(1, &scene.shadow_depth_tex);
    glBindTexture(GL_TEXTURE_2D, scene.shadow_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        Const::SHADOW_MAP_SIZE, Const::SHADOW_MAP_SIZE,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, scene.shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D, scene.shadow_depth_tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // planar reflection FBO
    scene.reflect_w = Const::WINDOW_WIDTH / 2;
    scene.reflect_h = Const::WINDOW_HEIGHT / 2;
    glGenFramebuffers(1, &scene.reflect_fbo);
    glGenTextures(1, &scene.reflect_color_tex);
    glBindTexture(GL_TEXTURE_2D, scene.reflect_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, scene.reflect_w, scene.reflect_h,
        0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &scene.reflect_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, scene.reflect_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, scene.reflect_w, scene.reflect_h);

    glBindFramebuffer(GL_FRAMEBUFFER, scene.reflect_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene.reflect_color_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, scene.reflect_depth_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[reflect] framebuffer incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    
    // toggle between using proc mesh or a OBJ model
    // I'll use it for debugging purposes
    // I want to get the physics close to "realistic" because
    // at the moment still has some shitty issues that I can't scratch my head around
    if constexpr (Const::USE_PROC_MESH){
        std::vector<float> proc_verts;
        build_tricycle_mesh(proc_verts);
        mesh_init(scene.proc_mesh, proc_verts);

        scene.model_center = glm::vec3(0.0f);
        scene.model_scale = 1.0f;
        scene.model_half_height = 0.625f;
    } 
    else {
        trike_model_init(scene.trike_model);
        driver_model_init(scene.driver_model);

        const auto& verts = scene.trike_model.mesh.data.vertices;

        float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f,minZ=1e9f,maxZ=-1e9f;
        for (int i = 0; i < (int)verts.size(); i += 8){
            float x = verts[i];
            float y = verts[i+1];
            float z = verts[i+2];
            minX=std::min(minX,x); maxX=std::max(maxX,x);
            minY=std::min(minY,y); maxY=std::max(maxY,y);
            minZ=std::min(minZ,z); maxZ=std::max(maxZ,z);
        }

        scene.model_center = glm::vec3(
            (minX+maxX)*0.5f, minY, (minZ+maxZ)*0.5f);
        scene.model_center.y -= Const::MODEL_FLOOR_FUDGE;

        float longest = std::max(maxX-minX, std::max(maxY-minY, maxZ-minZ));
        scene.model_scale = (longest > 0.0f) ? Const::MODEL_NORMALIZE_SIZE / longest : 1.0f;
        scene.model_half_height = (maxY-minY) * 0.5f * scene.model_scale;
    }


    // ground
    std::vector<float> gv;
    push_ground_quad(gv, Const::GROUND_HALF_EXTENT);
    mesh_init(scene.ground, gv);

    // axis gizmo
    std::vector<float> av;
    push_axis_gizmo(av, Const::GIZMO_LENGTH);
    mesh_init(scene.gizmo, av);

    // CACHE UNIFORMS FOR LOC ONCE
    {
        GLuint id = scene.shader.id;
        auto& L = scene.shader_loc;
        L.view = glGetUniformLocation(id, "u_view");
        L.proj = glGetUniformLocation(id, "u_proj");
        L.model = glGetUniformLocation(id, "u_model");
        L.normal_mat = glGetUniformLocation(id, "u_normal_mat");
        L.light_dir = glGetUniformLocation(id, "u_light_dir");
        L.light_color = glGetUniformLocation(id, "u_light_color");
        L.light_space = glGetUniformLocation(id, "u_light_space");
        L.ambient = glGetUniformLocation(id, "u_ambient");
        L.diff_intensity = glGetUniformLocation(id, "u_diff_intensity");
        L.shadow_bias = glGetUniformLocation(id, "u_shadow_bias");
        L.shadow_map = glGetUniformLocation(id, "u_shadow_map");
        L.kd = glGetUniformLocation(id, "u_kd");
        L.kd_alt = glGetUniformLocation(id, "u_kd_alt");
        L.checker_scale = glGetUniformLocation(id, "u_checker_scale");
        L.use_checker = glGetUniformLocation(id, "u_use_checker");
        L.fog_color = glGetUniformLocation(id, "u_fog_color");
        L.fog_near = glGetUniformLocation(id, "u_fog_near");
        L.fog_far = glGetUniformLocation(id, "u_fog_far");
        L.fog_cam_pos = glGetUniformLocation(id, "u_cam_pos_fog");
    }
    {
        GLuint id = scene.gizmo_shader.id;
        auto& L = scene.gizmo_loc;
        L.view  = glGetUniformLocation(id, "u_view");
        L.proj  = glGetUniformLocation(id, "u_proj");
        L.model = glGetUniformLocation(id, "u_model");
    }
    {
        GLuint id = scene.sky_shader.id;
        auto& L = scene.sky_loc;
        L.inv_view_proj = glGetUniformLocation(id, "u_inv_view_proj");
        L.sky_tex = glGetUniformLocation(id, "u_sky_tex");
        L.sky_night_tex = glGetUniformLocation(id, "u_sky_night_tex");
        L.tint_a = glGetUniformLocation(id, "u_tint_a");
        L.tint_b = glGetUniformLocation(id, "u_tint_b");
        L.flip_a = glGetUniformLocation(id, "u_flip_a");
        L.flip_b = glGetUniformLocation(id, "u_flip_b");
        L.blend = glGetUniformLocation(id, "u_blend");
        L.uv_offset = glGetUniformLocation(id, "u_uv_offset");
        L.use_night_b = glGetUniformLocation(id, "u_use_night_b");
        L.rain_blend = glGetUniformLocation(id, "u_rain_blend");
        L.sky_rain_tex = glGetUniformLocation(id, "u_sky_rain_tex");
        L.night_factor = glGetUniformLocation(id, "u_night_factor");
    }
    {
        GLuint id = scene.shadow_shader.id;
        scene.shadow_loc.light_space = glGetUniformLocation(id, "u_light_space");
        scene.shadow_loc.model = glGetUniformLocation(id, "u_model");
    }
    {
        GLuint id = scene.shader.id;
        auto& LL = scene.light_loc;
        LL.count = glGetUniformLocation(id, "u_light_count");
        char buf[64];
        for (int i = 0; i < Const::MAX_POINT_LIGHTS; i++){
            snprintf(buf, sizeof(buf), "u_light_pos[%d]", i);
            LL.pos[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_color_pt[%d]", i);
            LL.color[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_radius[%d]", i);
            LL.radius[i] = glGetUniformLocation(id, buf);
            snprintf(buf, sizeof(buf), "u_light_intensity[%d]", i);
            LL.intensity[i] = glGetUniformLocation(id, buf);
        }
    }

    // persistent line batch for hitbox wireframes
    std::vector<float> empty(6, 0.0f);
    mesh_init(scene.line_batch, empty);
    scene.line_verts.reserve(128 * 24 * 6);
}

void scene_shadow_resize(SceneState& scene){
    // delete old depth tex and recreate at new size
    // FBO itself stays valid
    // just reattach the new tex
    if (scene.shadow_depth_tex){
        glDeleteTextures(1, &scene.shadow_depth_tex);
        scene.shadow_depth_tex = 0;
    }

    int size = my_settings.shadow_map_size;

    glGenTextures(1, &scene.shadow_depth_tex);
    glBindTexture(GL_TEXTURE_2D, scene.shadow_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, scene.shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, scene.shadow_depth_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void scene_destroy(SceneState& scene){
    if (scene.sky_tex) glDeleteTextures(1, &scene.sky_tex);
    if (scene.sky_night_tex) glDeleteTextures(1, &scene.sky_night_tex);
    if (scene.sky_rain_tex)  glDeleteTextures(1, &scene.sky_rain_tex);
    shader_destroy(scene.sky_shader);
    mesh_destroy(scene.sky_quad);
    shader_destroy(scene.shadow_shader);
    if (scene.shadow_fbo) glDeleteFramebuffers(1, &scene.shadow_fbo);
    if (scene.shadow_depth_tex) glDeleteTextures(1, &scene.shadow_depth_tex);
    if (scene.reflect_fbo) glDeleteFramebuffers(1, &scene.reflect_fbo);
    if (scene.reflect_color_tex) glDeleteTextures(1, &scene.reflect_color_tex);
    if (scene.reflect_depth_rbo) glDeleteRenderbuffers(1, &scene.reflect_depth_rbo);
    trike_model_destroy(scene.trike_model);
    obj_mesh_destroy(scene.trike_mesh);
    mesh_destroy(scene.proc_mesh);
    mesh_destroy(scene.ground);
    mesh_destroy(scene.gizmo);
    shader_destroy(scene.shader);
    shader_destroy(scene.gizmo_shader);
}