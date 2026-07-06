#include "input_terrain.hpp"
#include "const.hpp"
#include "map_manager.hpp"

/**********************************************************************
INPUT TERRAIN
Responsibilities
- Terrain sculpt (raise/lower/smooth) + surface paint mode
- Brush radius adjustment
- Undo stack + flatten
- Save

How to 101 in case you struggle in operating in the program:
- H key to activate TERRAIN MODE
- LMB depresses terrain surface, RMB raises it
- CTRL + LMB/RMB is a finer depress/raise tune
- Shift is terrain smoothener
- [/] keys adjust brush size
- [CTRL+Z] undo last sculpt stroke
- P key toggles PAINT MODE - TERRAIN SUB MODE
- number keys [0-7] pick the surface (0 = erase/none, 1-7 = asphalt through rock, see SurfaceType)
- [CTRL+SHIFT+W] wipes the entire painted surface back to blank
- [CTRL+SHIFT+F] flatten the whole terrain to y=0 (undoable) NOTE: UNDOABLE, TREAD WITH CAUTION IF YOU'RE
WORKING WITH AN ACTUAL MAP
- [CTRL+S] save MAP
**********************************************************************/

void editor_input_terrain(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, float dt){

    bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS;
    bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    // [ and ] adjust brush radius in terrain mode
    bool brk_l = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
    bool brk_r = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (brk_l) editor.brush_radius = std::max(Const::TERRAIN_BRUSH_RADIUS_MIN, editor.brush_radius - 6.0f * dt);
    if (brk_r) editor.brush_radius = std::min(Const::TERRAIN_BRUSH_RADIUS_MAX, editor.brush_radius + 6.0f * dt);

    // *******************************
    // PAINT MODE
    // *******************************
    if (editor.paint_mode){
        static const int surf_keys[7] = {
            GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
            GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7
        };
        for (int i = 0; i < 7; i++){
            if (glfwGetKey(window, surf_keys[i]) == GLFW_PRESS)
                editor.paint_surface = (SurfaceType)(i + 1);
        }
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
            editor.paint_surface = SURFACE_NONE;

        // Ctrl+Shift+W wipes entire surface map back to blank canvas
        static bool s_wipe_last = false;
        bool wipe = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                 && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)   == GLFW_PRESS
                 && glfwGetKey(window, GLFW_KEY_W)            == GLFW_PRESS;
        if (wipe && !s_wipe_last){
            map.terrain.surface.assign(map.terrain.rows * map.terrain.cols, (uint8_t)SURFACE_NONE);
            er.terrain_surface_dirty = true;
            std::cout << "[paint] canvas wiped\n";
        }
        s_wipe_last = wipe;
    }

    if (editor.placement_valid){
        float cx = editor.ghost_pos.x;
        float cz = editor.ghost_pos.z;
        static bool s_sculpt_pushed = false;
        bool sculpting = (lmb || rmb) && !shift && !editor.paint_mode;
        bool smoothing = (lmb || rmb) &&  shift && !editor.paint_mode;
        if ((sculpting || smoothing) && !s_sculpt_pushed){
            heightfield_push_undo(map.terrain);
            s_sculpt_pushed = true;
        }
        if (!lmb && !rmb) s_sculpt_pushed = false;

        if (editor.paint_mode){
            if (lmb){
                if (!s_sculpt_pushed){
                    heightfield_push_undo(map.terrain);
                    s_sculpt_pushed = true;
                }
                heightfield_paint(map.terrain, cx, cz,
                    editor.brush_radius, editor.paint_surface);
                er.terrain_surface_dirty = true;
                er.terrain_mesh_dirty    = false;
            }
        }
        else {
            if (shift){
                // shift + any button = smooth brush
                if (lmb || rmb)
                    heightfield_smooth(map.terrain, cx, cz,
                        editor.brush_radius, Const::TERRAIN_BRUSH_SMOOTH_STRENGTH * dt);
            }
            else if (lmb){
                bool fine = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                float strength = fine ? Const::TERRAIN_BRUSH_STRENGTH * 0.1f : Const::TERRAIN_BRUSH_STRENGTH;
                heightfield_sculpt(map.terrain, cx, cz,
                    editor.brush_radius, strength * dt * 60.0f);
            }
            else if (rmb){
                bool fine = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                float strength = fine ? Const::TERRAIN_BRUSH_STRENGTH * 0.1f : Const::TERRAIN_BRUSH_STRENGTH;
                heightfield_sculpt(map.terrain, cx, cz,
                    editor.brush_radius, -strength * dt * 60.0f);
            }

            // clamp terrain to designed limits after every sculpt
            if (lmb || rmb)
                heightfield_clamp(map.terrain,
                    Const::TERRAIN_MIN_Y, Const::TERRAIN_MAX_Y);

            // mark terrain mesh for rebuild next draw
            if (lmb || rmb) er.terrain_mesh_dirty = true;
        }
    }

    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    bool shift_held = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    // Ctrl+Z undo last sculpt stroke
    static bool s_z_last = false;
    bool z_down = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (ctrl && z_down && !s_z_last){
        heightfield_pop_undo(map.terrain);
        er.terrain_mesh_dirty    = true;
        er.terrain_surface_dirty = true;
        std::cout << "[terrain] undo stack remaining=" << map.terrain.undo_stack.size() << "\n";
    }
    s_z_last = z_down;

    // Ctrl+Shift+F flatten entire terrain to y=0
    static bool s_f_last = false;
    bool f_down = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (ctrl && shift_held && f_down && !s_f_last){
        heightfield_push_undo(map.terrain); // allow undoing the flatten too
        heightfield_flatten(map.terrain);
        er.terrain_mesh_dirty = true;
        std::cout << "[terrain] flattened\n";
    }
    s_f_last = f_down;

    // Ctrl+S saves
    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        world_map_save(map, g_maps.loaded_dir + "/map.txt");
}