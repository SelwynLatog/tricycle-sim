#include "../renderer/render_textures.hpp"
#include "../world/npc.hpp"
#include "input_object.hpp"
#include "const.hpp"
#include "raycast.hpp"
#include "asset_scan.hpp"
#include "map_manager.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

/**********************************************************************
INPUT OBJECT
Responsibilities
- Tool switching (translate/rotate/scale)
- Transform of selected object
- Placement, deletion
- Behavior cycling + dynamic physics preset cycling
- Copy/paste
- Pedestrian (NPC) config: type, hail flag, weight, walk points, drop point
- Prop/entity palette selection + paging
- F5 asset rescan

How to 101 in case you struggle in operating in the program:
- LMB places the currently selected prop, or selects an object if you
  click on one instead
- CTRL+LMB selects the smallest object under the cursor (useful when
  objects overlap)
- SHIFT+LMB places directly on top of the currently selected object
- DEL deletes the selected object
- T / R / Y switch tool to translate / rotate / scale
- arrow keys move/rotate/scale the selected object, PgUp/PgDn nudges
  height in translate mode
- hold SHIFT for fine steps
- B cycles the selected object's behavior: STATIC > DYNAMIC > DECORATION
  > PEDESTRIAN > (back to STATIC)
- N cycles the physics preset when a DYNAMIC object is selected
- Ctrl+C copies the selected object, Ctrl+V pastes it at the cursor
- number keys 1-9 pick a prop from the current palette page, [ / ] flips pages
- Ctrl+S saves the map, F5 rescans assets/props for new files

PEDESTRIAN-only (select a pedestrian first):
- J cycles the NPC type (person/chicken/cow/etc)
- G toggles whether it can hail a ride
- + / - adjusts its weight
- I sets its first walk point, U sets its second, X sets its drop-off point

NOTE: 
- The editor is flexible and does not constraint object as model-type behavior
- what that means is you can set a traffic-cone to Behavior:PEDESTRIAN
- it will behave the way a normal pedestrian will
- you want a chicken to hail and mount the trike? you can absolutely do that

**********************************************************************/

// dynamic physics preset table
// adding a new type = one new row here + a const in const.hpp
// N key cycles through this when selected object is DYNAMIC
// much easier for me to scale than slopping down on else statements
struct DynPreset {
    const char* name;
    float mass;
    float restitution;
    float friction;
};

static const DynPreset DYN_PRESETS[] = {
    { "CONE",       Const::DYN_CONE_MASS,          Const::DYN_CONE_RESTITUTION,         Const::DYN_CONE_FRICTION         },
    { "BIN",        Const::DYN_BIN_MASS,           Const::DYN_BIN_RESTITUTION,          Const::DYN_BIN_FRICTION          },
    { "BAG",        Const::DYN_BAG_MASS,           Const::DYN_BAG_RESTITUTION,          Const::DYN_BAG_FRICTION          },
    { "CART",       Const::DYN_CART_MASS,          Const::DYN_CART_RESTITUTION,         Const::DYN_CART_FRICTION         },
    { "MOTORCYCLE", Const::DYN_MOTORCYCLE_MASS,    Const::DYN_MOTORCYCLE_RESTITUTION,   Const::DYN_MOTORCYCLE_FRICTION   },
    { "POLE",       Const::DYN_POLE_MASS,          Const::DYN_POLE_RESTITUTION,         Const::DYN_POLE_FRICTION         },
    { "TRIKE",      Const::DYN_TRIKE_MASS,         Const::DYN_TRIKE_RESTITUTION,        Const::DYN_TRIKE_FRICTION        },
    { "RAILING",    Const::DYN_RAILING_MASS,       Const::DYN_RAILING_RESTITUTION,      Const::DYN_RAILING_FRICTION      },
    { "STALL",      Const::DYN_STALL_MASS,         Const::DYN_STALL_RESTITUTION,        Const::DYN_STALL_FRICTION        },
    { "BARREL",     Const::DYN_BARREL_MASS,        Const::DYN_BARREL_RESTITUTION,       Const::DYN_BARREL_FRICTION       },
    { "CAR",        Const::DYN_CAR_MASS,           Const::DYN_CAR_RESTITUTION,          Const::DYN_CAR_FRICTION          },
    { "TRUCK",      Const::DYN_TRUCK_MASS,         Const::DYN_TRUCK_RESTITUTION,        Const::DYN_TRUCK_FRICTION        },
    { "FRUIT",      Const::DYN_FRUIT_MASS,         Const::DYN_FRUIT_RESTITUTION,        Const::DYN_FRUIT_FRICTION        },
    { "BUS",        Const::DYN_BUS_MASS,           Const::DYN_BUS_RESTITUTION,          Const::DYN_BUS_FRICTION          },
    { "CHAIR",      Const::DYN_CHAIR_MASS,         Const::DYN_CHAIR_RESTITUTION,        Const::DYN_CHAIR_FRICTION        },
    { "BASKETBALL", Const::DYN_BBALL_MASS,         Const::DYN_BBALL_RESTITUTION,        Const::DYN_BBALL_FRICTION        },
    { "DEFAULT",    Const::DYN_DEFAULT_MASS,       Const::DYN_DEFAULT_RESTITUTION,      Const::DYN_DEFAULT_FRICTION      },
};
static const int DYN_PRESET_COUNT = (int)(sizeof(DYN_PRESETS) / sizeof(DYN_PRESETS[0]));

void editor_input_object(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty){

    static bool s_del_last = false;
    static bool s_lmb_last = false;
    static bool s_b_last = false;
    static bool s_n_last = false;
    static bool s_c_last = false;
    static bool s_v_last = false;
    static bool s_f5_last = false;
    static bool s_arr_left_last = false;
    static bool s_arr_right_last = false;
    static bool s_arr_up_last = false;
    static bool s_arr_down_last = false;
    static bool s_pgup_last = false;
    static bool s_pgdn_last = false;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    // prop palette
    // num keys 1-9 select from curr page of prop_list
    // page flips with [ and ] so we scroll past 9 props
    {
        // resolve active list first
        // PEDESTRIAN selected = entity palette, else prop palette
        bool ped_selected = false;
        for (const auto& o : map.objects)
            if (o.id == editor.selected_id && o.behavior == PEDESTRIAN) { ped_selected = true; break; }

        const std::vector<std::string>& active_list = ped_selected ? editor.entity_list : editor.prop_list;
        int& active_page = ped_selected ? editor.entity_page : editor.prop_page;

        // page scroll 
        // operates on whichever list is active
        static bool s_page_pgup_last = false;
        static bool s_page_pgdn_last = false;
        bool pgup = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
        bool pgdn = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
        if (pgup && !s_page_pgup_last && active_page > 0) active_page--;
        if (pgdn && !s_page_pgdn_last){
            int list_size = (int)active_list.size();
            int max_page = list_size > 0 ? (list_size - 1) / Const::EDITOR_PAGE_SIZE : 0;
            if (active_page < max_page) active_page++;
        }
        s_page_pgup_last = pgup;
        s_page_pgdn_last = pgdn;

        // 1-9 select from active list
        static const int num_keys[9] = {
            GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
            GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6,
            GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9
        };

        for (int i = 0; i < Const::EDITOR_PAGE_SIZE; i++){
            if (glfwGetKey(window, num_keys[i]) == GLFW_PRESS){
                int idx = active_page * Const::EDITOR_PAGE_SIZE + i;
                if (idx < (int)active_list.size()){
                    editor.selected_model = active_list[idx];
                    if (!ped_selected) editor.selected_id = -1;
                    std::cout << "[editor] selected " << (ped_selected ? "entity" : "prop")
                              << ": " << editor.selected_model << "\n";
                }
            }
        }
    }

    // tool switching
    // tool key + <- -> to control
    // eg. R + <- -> to rotate mesh
    // probably should have added this to make it more clear
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) editor.tool = TOOL_TRANSLATE;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) editor.tool = TOOL_ROTATE;
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) editor.tool = TOOL_SCALE;

    // rotate selected object
    if (editor.selected_id != -1){
        for(auto& o : map.objects){
            if (o.id != editor.selected_id) continue;

            if (editor.tool == TOOL_TRANSLATE){
                bool al = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
                bool ar = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
                bool au = glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS;
                bool ad = glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS;
                bool pgup = glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS;
                bool pgdn = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;

                // alt held = fine mode: 5cm steps instead of grid snap
                bool alt = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
                float step = alt ? Const::EDITOR_GRID_SNAP_FINE : Const::EDITOR_GRID_SNAP;

                // xz movement
                if (al && !s_arr_left_last) o.position.x -= step;
                if (ar && !s_arr_right_last) o.position.x += step;
                if (au && !s_arr_up_last) o.position.z -= step;
                if (ad && !s_arr_down_last) o.position.z += step;

                // y nudge for vert pos adjustment
                if (pgup && !s_pgup_last) o.position.y += step;
                if (pgdn && !s_pgdn_last) o.position.y -= step;


                if (al || ar || au || ad || pgup || pgdn) map_dirty = true;
                s_arr_left_last = al;
                s_arr_right_last = ar;
                s_arr_up_last = au;
                s_arr_down_last = ad;
                s_pgup_last = pgup;
                s_pgdn_last = pgdn;
            }

            if (editor.tool == TOOL_ROTATE){
                if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) { o.rotation.y -= Const::EDITOR_ROTATE_SPEED * dt; map_dirty = true; }
                if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { o.rotation.y += Const::EDITOR_ROTATE_SPEED * dt; map_dirty = true; }
            }

           if (editor.tool == TOOL_SCALE){
                bool alt = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
                float sspeed = alt ? Const::EDITOR_SCALE_SPEED_FINE : Const::EDITOR_SCALE_SPEED;
                bool sl = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
                bool sr = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
                if (sl) o.scale -= sspeed * dt;
                if (sr) o.scale += sspeed * dt;
                o.scale = glm::max(o.scale, glm::vec3(0.005f));
                if (sl || sr) map_dirty = true;
            }
            break;
        }
    }

    // LMB place object
    bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    if (lmb && !s_lmb_last){

        // shift+click = force place on top of selected object, skip raycast
        // prevents buggy lmb spam clicking
        if (shift && editor.selected_id != -1 && editor.placement_valid && !editor.selected_model.empty()){
            // fall through to place block below with selected_id intact
        }
        // check first if we clicked an existing object
        else {
            // ctrl held = small object priority: picks smallest AABB volume hit
            // select smaller objects for easier placement
            // when putting smaller objects inside a bigger object
            bool ctrl_held = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            int hit = editor_raycast_objects(mx, my, view, proj, screen_w, screen_h, map, er, ctrl_held);
            if (hit != -1){
                editor.selected_id = hit;
                std::cout << "editor selected id=" << hit << "\n";
                s_lmb_last = lmb;
                return;
            }
        }

        if(editor.placement_valid && !editor.selected_model.empty()){
            // no object hit
            // place new one
            WorldObject o;
            o.position = editor.ghost_pos;
            o.model_path = editor.selected_model;
            o.y_floor_offset = editor_get_y_floor_offset(er, editor.selected_model);

            // ground/road surfaces default to DECORATION — no collision
            // everything else defaults to STATIC
            // extend this list as new ground types are added
            static const char* GROUND_TYPES[] = {
                "asphalt", "gravel", "dirt", "sand", "grass", "cement"
            };
            o.behavior = STATIC;
            for (const char* gt : GROUND_TYPES){
                if (o.model_path.find(gt) != std::string::npos){
                    o.behavior = DECORATION;
                    break;
                }
            }

            // vertical stacking
            // scan objects sharing this XZ grid cell
            // if any exist just land on top of highes one
            float stack_y = 0.0f;
            if (editor.selected_id != -1){
                for (const auto& other : map.objects){
                    if (other.id != editor.selected_id) continue;

                    // compute world-space top using real mesh bounds when cached
                    float obj_height = other.scale.y;
                    auto bit = er.prop_bounds.find(other.model_path);
                    if (bit != er.prop_bounds.end()){
                        float yoff = er.prop_y_offset.count(other.model_path)
                            ? er.prop_y_offset.at(other.model_path) : 0.0f;
                        obj_height = (bit->second.local_max.y + yoff) * other.scale.y;
                    }
                    stack_y = other.position.y + obj_height;
                    break;
                }
            }
            o.position.y = stack_y;

            WorldObject& placed = world_map_place(map, o);
            editor.selected_id = placed.id;
            map_dirty = true;
        }
    }
    s_lmb_last = lmb;

    // Del remove selected object
    bool del = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
    if (del && !s_del_last){
        if (editor.selected_id != -1){
            world_map_remove(map, editor.selected_id);
            editor.selected_id = -1;
            map_dirty = true;
        }
    }
    
    // PEDESTRIAN config for selected object
    // only active when selected object is PEDESTRIAN
    if (editor.selected_id != -1){
        for (auto& o : map.objects){
            if (o.id != editor.selected_id) continue;
            if (o.behavior != PEDESTRIAN) break;

            bool ped_shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            float step = ped_shift ? 0.05f : 0.5f;

            // J key cycles npc type
            static bool s_j_last = false;
            bool j_down = glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS;
            if (j_down && !s_j_last){
                o.npc_type = (o.npc_type + 1) % NPC_TYPE_COUNT;
                std::cout << "[npc] type -> " << NPC_TYPE_NAMES[o.npc_type] << "\n";
                map_dirty = true;
            }
            s_j_last = j_down;

            // G toggles can_hail
            // only meaningful for PED-type
            // but go ahead and go ham
            // you can set an object to have PEDESTRIAN-behavior
            // and set it to hail
            static bool s_g_last = false;
            bool g_down = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
            if (g_down && !s_g_last){
                o.npc_can_hail = !o.npc_can_hail;
                std::cout << "[npc] can_hail -> " << (o.npc_can_hail ? "YES" : "NO") << "\n";
                map_dirty = true;
            }
            s_g_last = g_down;

            // + / - adjust weight
            bool plus  = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS;
            bool minus = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;
            if (plus)  { o.npc_weight += 5.0f * dt; map_dirty = true; }
            if (minus) { o.npc_weight = std::max(0.5f, o.npc_weight - 5.0f * dt); map_dirty = true; }

            // set walk_a / walk_b / drop_point from current ghost pos
            // I = set walk_a, U = set walk_b, X = set drop point
            static bool s_walka_last = false;
            static bool s_u_last = false;
            static bool s_x_last = false;
            bool walka_down = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
            bool u_down = glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS;
            bool x_down = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;

            if (walka_down && !s_walka_last && editor.placement_valid){
                o.npc_walk_a = editor.ghost_pos;
                std::cout << "[npc] walk_a set to ("
                          << o.npc_walk_a.x << ", " << o.npc_walk_a.z << ")\n";
                map_dirty = true;
            }
            if (u_down && !s_u_last && editor.placement_valid){
                o.npc_walk_b = editor.ghost_pos;
                std::cout << "[npc] walk_b set to ("
                          << o.npc_walk_b.x << ", " << o.npc_walk_b.z << ")\n";
                map_dirty = true;
            }
            if (x_down && !s_x_last && editor.placement_valid){
                o.npc_drop_point = editor.ghost_pos;
                std::cout << "[npc] drop_point set to ("
                          << o.npc_drop_point.x << ", " << o.npc_drop_point.z << ")\n";
                map_dirty = true;
            }
            s_walka_last = walka_down;
            s_u_last = u_down;
            s_x_last = x_down;
            break;
        }
    }


    // B cycle behavior on selected object
    bool b_down = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    if (b_down && !s_b_last && editor.selected_id != -1){
        for (auto& o : map.objects){
            if (o.id != editor.selected_id) continue;
            switch (o.behavior){
                case STATIC: o.behavior = DYNAMIC; break;
                case DYNAMIC: o.behavior = DECORATION; break;
                case DECORATION: o.behavior = PEDESTRIAN; break;
                case PEDESTRIAN: o.behavior = STATIC; break;
            }
            // on becoming DYNAMIC apply current preset immediately
            if (o.behavior == DYNAMIC){
                const DynPreset& p = DYN_PRESETS[editor.dyn_preset_index];
                o.mass = p.mass;
                o.restitution = p.restitution;
                o.friction = p.friction;
                std::cout << "[editor] id=" << o.id << " DYNAMIC preset=" << p.name
                          << " mass=" << p.mass << "\n";
            } 
            else {
                o.mass = 999.0f;
                o.restitution = 0.10f;
                o.friction = 0.99f;
                std::cout << "[editor] id=" << o.id << " behavior -> " << o.behavior << "\n";
            }
            break;
        }
    }
    s_b_last = b_down;

    // N cycle dynamic preset on selected DYNAMIC object
    bool n_down = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;
    if (n_down && !s_n_last && editor.selected_id != -1){
        for (auto& o : map.objects){
            if (o.id != editor.selected_id) continue;
            if (o.behavior != DYNAMIC) break; // N only works on DYNAMIC objects
            editor.dyn_preset_index = (editor.dyn_preset_index + 1) % DYN_PRESET_COUNT;
            const DynPreset& p = DYN_PRESETS[editor.dyn_preset_index];
            o.mass = p.mass;
            o.restitution = p.restitution;
            o.friction    = p.friction;
            std::cout << "[editor] id=" << o.id << " preset -> " << p.name
                      << " mass=" << p.mass << "\n";
            break;
        }
    }
    s_n_last = n_down;

    // Ctrl+C copy selected object into clipboard
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    bool c_down = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    bool v_down = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;

    if (ctrl && c_down && !s_c_last && editor.selected_id != -1){
        for (const auto& o : map.objects){
            if (o.id != editor.selected_id) continue;
            editor.clipboard = o;
            editor.has_clipboard = true;
            std::cout << "[editor] copied id=" << o.id << " model=" << o.model_path << "\n";
            break;
        }
    }
    s_c_last = c_down;

    // Ctrl+V paste clipboard at current ghost pos
    if (ctrl && v_down && !s_v_last && editor.has_clipboard){
        if (editor.placement_valid){
            WorldObject o = editor.clipboard;
            o.position = editor.ghost_pos;
            // y stays at ghost ground level + whatever floor offset the model needs
            o.position.y = editor.clipboard.position.y; // preserve vertical nudge from original
            static const char* GROUND_TYPES[] = {
                "asphalt", "gravel", "dirt", "sand", "grass", "cement"
            };
            for (const char* gt : GROUND_TYPES){
                if (o.model_path.find(gt) != std::string::npos){
                    o.behavior = DECORATION;
                    break;
                }
            }
            
            WorldObject& placed = world_map_place(map, o);
            editor.selected_id = placed.id;
            std::cout << "[editor] pasted " << o.model_path << " id=" << placed.id
                      << " at (" << o.position.x << ", " << o.position.z << ")\n";
        }
    }
    s_v_last = v_down;

    // save map
    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        world_map_save(map, g_maps.loaded_dir + "/map.txt");
    }

    // F5 rescan assets/props for new/removed props
    bool f5_down = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    if (f5_down && !s_f5_last){
        editor_scan_props(editor, "../assets/props");
        editor.prop_page = 0;
        std::cout<< "[editor] assets refreshed total=" << editor.prop_list.size() << "\n";
     }
    s_f5_last = f5_down;
}