#pragma once
#include "../core/editor_state.hpp"
#include "../world/world_map.hpp"
#include "../world/height_field.hpp"
#include "../world/road_spline.hpp"
#include "../world/ocean.hpp"
#include "../world/npc.hpp"
#include "../physics/dynamic_sim.hpp"
#include "../tricycle/driver_model.hpp"
#include "../tricycle/tricycle_model.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "font.hpp"
#include "obj_mesh.hpp"
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <unordered_map>
// all visual feedback for editor:
// uses same gizmo shader pattern as scene.cpp
struct EditorRenderer{
    Shader shader; // flat color pos + rgb
    Shader obj_shader; // lit pos + normal
    Mesh grid; // snap grid built once at init
    Font font; // editor control hud

    // load prop meshes keyed by filename eg. "balay.obj"
    std::map<std::string, ObjMesh> prop_cache;

    // GL texture objects keyed by absolute tex path
    // 0 = not loaded yet or failed
    std::map<std::string, GLuint> tex_cache;

    // y offset per prop
    // this sets mesh lowest point to 0 so no spawning below ground level
    std::map<std::string, float> prop_y_offset;

    // world space using half-extents derived from min/max at load time
    // this is used for wireframe boxes and raycast AABB so they match actual mesh
     struct PropBounds {
        glm::vec3 local_min = glm::vec3(0.0f);
        glm::vec3 local_max = glm::vec3(1.0f);
    };
    std::map<std::string, PropBounds> prop_bounds;

    // terrain wireframe mesh
    Mesh terrain_mesh;
    bool terrain_mesh_dirty = true; // set true after any sculpt op

    // terrain solid surface mesh, one triangle per half-cell
    // rebuilt when sculpt or paint changes the terrain
    Mesh terrain_surface_mesh;
    bool terrain_surface_dirty = true;
    std::vector<int> terrain_surface_offsets; // vertex start per surface type
    std::vector<int> terrain_surface_counts;  // vertex count per surface type

    // road spline shader
    Shader road_shader;

    // ocean wave shader + time accumulator
    Shader ocean_shader;
    struct {
        GLint view, proj, time, y_level;
        GLint light_dir, cam_pos;
        GLint light_color, ambient, diff_intensity;
        GLint reflect_tex, refl_view_proj;
        GLint normal_tex, foam_tex, foam_shore_tex;
        GLint max_depth;
    } ocean_loc;

    // shadow map recevied from scene
    GLuint shadow_depth_tex = 0;
    glm::mat4 light_space_mat = glm::mat4(1.0f);

    // planar reflection texture + matrix, pushed from scene.reflect_color_tex /
    // scene.reflect_view_proj each frame
    GLuint reflect_tex = 0;
    glm::mat4 reflect_view_proj = glm::mat4(1.0f);

    // water surface detail textures (tuxalin/water shader ref assets)
    GLuint ocean_normal_tex = 0;
    GLuint ocean_foam_tex = 0;
    GLuint ocean_foam_shore_tex = 0;

    // lighting set each frame from scene_update_daytime output
    glm::vec3 sun_dir = glm::vec3(1,2,1);
    glm::vec3 light_color = glm::vec3(1,1,1);
    float ambient = 0.50f;
    float diff_intensity = 0.85f;

    // depth shader for casting shadows from props
    Shader depth_shader;

    // cached uniform locations
    // obj_shader (draw_props)
    struct {
        GLint view, proj, model, normal_mat;
        GLint light_dir, light_space, shadow_bias;
        GLint ambient, diff_intensity, light_color;
        GLint shadow_map, tex, use_texture, kd;
        GLint fog_color, fog_near, fog_far, fog_cam_pos;
    } obj_loc;

    // road_shader (draw_roads + draw_terrain_surface)
    struct {
        GLint view, proj, model, normal_mat;
        GLint light_dir, light_space, shadow_bias;
        GLint ambient, diff_intensity, light_color;
        GLint shadow_map, tex, use_texture, kd;
        GLint fog_color, fog_near, fog_far, fog_cam_pos;
    } road_loc;

    // depth_shader (shadow pass)
    struct {
        GLint light_space, model;
    } depth_loc;

    // obj_shader point light locations
    struct {
        GLint count;
        GLint pos[Const::MAX_POINT_LIGHTS];
        GLint color[Const::MAX_POINT_LIGHTS];
        GLint radius[Const::MAX_POINT_LIGHTS];
        GLint intensity[Const::MAX_POINT_LIGHTS];
        GLint spot_dir[Const::MAX_POINT_LIGHTS];
        GLint cos_cutoff[Const::MAX_POINT_LIGHTS];
    } pt_light_loc;

    Mesh line_batch;
    std::vector<float> line_verts;
    glm::vec3 shadow_cull_center = glm::vec3(0.0f);

    std::vector<LightSource> last_lights;
    float night_factor = 1.0f;
    glm::vec3 fog_color = glm::vec3(0.5f, 0.6f, 0.7f);
    float fog_near = 60.0f;
    float fog_far  = 200.0f;
    int pose_npc_id = -1; // set during pose mode to suppress world render of that npc
};

// function declarations for EditorRenderer live in per-concern headers:
//   render_textures.hpp  - texture/mesh caching
//   render_init.hpp      - init + destroy
//   render_helpers.hpp   - shared draw helpers
//   render_props.hpp     - prop + shadow pass
//   render_terrain.hpp   - terrain wireframe + surface
//   render_road.hpp      - road splines
//   render_ocean.hpp     - ocean
//   render_pose.hpp      - pose mode
//   render_gizmo.hpp     - editor gizmos/overlays
//   render_hud.hpp       - HUD text + settings menu