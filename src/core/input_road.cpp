#include "input_road.hpp"
#include "const.hpp"
#include "map_manager.hpp"
#include <algorithm>
#include <iostream>

/**********************************************************************
INPUT ROAD
Responsibilities
- Road spline point add/undo
- Road type cycling
- Selected point Y nudge
- Finish/delete spline
- Save

How to 101 in case you struggle in operating in the program:
- LMB creates a point
- a cyan line appears that connects to prev placed point
- placing a new point auto connects road
- RMB deletes previous point/spline
- ctrl+S to save
**********************************************************************/

void editor_input_road(EditorState& editor, WorldMap& map, GLFWwindow* window, float dt){
    static bool s_lmb_last = false;
    static bool s_rmb_last = false;
    static bool s_enter_last = false;
    static bool s_del_last = false;
    static bool s_brk_l_last = false;
    static bool s_brk_r_last = false;

    bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS;
    bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool enter = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    // find or create the active spline
    RoadSpline* active = nullptr;
    if (editor.active_road_id != -1){
        for (auto& r : map.roads)
            if (r.id == editor.active_road_id){ active = &r; break; }
    }

    // LMB = add control point to active spline
    if (lmb && !s_lmb_last && editor.placement_valid){
        if (!active){
            // start a new spline
            RoadSpline nr;
            nr.id    = map.next_road_id++;
            nr.type  = ROAD_ASPHALT;
            nr.width = 7.0f;
            map.roads.push_back(nr);
            active = &map.roads.back();
            editor.active_road_id = nr.id;
            std::cout << "[road] new spline id=" << nr.id << "\n";
        }
        glm::vec3 pt = editor.ghost_pos;
        active->points.push_back(pt);
        road_spline_build_mesh(*active, &map.terrain);
        std::cout << "[road] added point (" << pt.x << "," << pt.y << "," << pt.z
                  << ") total=" << active->points.size() << "\n";
    }

    // PgUp/PgDn nudge selected point Y
    // fine-tune height independent of terrain
    if (active && editor.selected_point_idx >= 0
        && editor.selected_point_idx < (int)active->points.size())
    {
        bool pgup = glfwGetKey(window, GLFW_KEY_PAGE_UP)   == GLFW_PRESS;
        bool pgdn = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;
        float step = shift ? 0.05f : 0.25f;
        if (pgup){ active->points[editor.selected_point_idx].y += step; road_spline_build_mesh(*active); }
        if (pgdn){ active->points[editor.selected_point_idx].y -= step; road_spline_build_mesh(*active); }
    }

    // [ / ] cycle road type on active spline
    bool brk_l = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
    bool brk_r = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (active){
        if (brk_l && !s_brk_l_last){
            active->type = (RoadType)(((int)active->type - 1 + ROAD_COUNT) % ROAD_COUNT);
            active->width = (active->type == ROAD_LINES) ? 0.3f : 7.0f;
            road_spline_build_mesh(*active, &map.terrain);
            std::cout << "[road] type -> " << (int)active->type << "\n";
        }
        if (brk_r && !s_brk_r_last){
            active->type = (RoadType)(((int)active->type + 1) % ROAD_COUNT);
            active->width = (active->type == ROAD_LINES) ? 0.3f : 7.0f;
            road_spline_build_mesh(*active, &map.terrain);
            std::cout << "[road] type -> " << (int)active->type << "\n";
        }
    }
    s_brk_l_last = brk_l;
    s_brk_r_last = brk_r;

    // Enter = finish spline, return to object mode
    if (enter && !s_enter_last){
        if (active && active->points.size() >= 2)
            road_spline_build_mesh(*active, &map.terrain);
        editor.active_road_id    = -1;
        editor.selected_point_idx = -1;
        editor.mode = MODE_OBJECT;
        std::cout << "[road] spline finished\n";
    }
    s_enter_last = enter;

    // RMB = undo last point
    if (rmb && !s_rmb_last && active && !active->points.empty()){
        active->points.pop_back();
        if (active->points.size() >= 2) road_spline_build_mesh(*active);
        std::cout << "[road] removed last point, remaining=" << active->points.size() << "\n";
    }
    s_rmb_last = rmb;

    // DEL = delete entire active spline
    bool del = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
    if (del && !s_del_last && editor.active_road_id != -1){
        map.roads.erase(std::remove_if(map.roads.begin(), map.roads.end(),
            [&](const RoadSpline& r){ return r.id == editor.active_road_id; }),
            map.roads.end());
        editor.active_road_id = -1;
        std::cout << "[road] deleted spline\n";
    }
    s_del_last = del;

    // Ctrl+S saves in road mode too
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        world_map_save(map, g_maps.loaded_dir + "/map.txt");

    s_lmb_last = lmb;
}