#include "input_light.hpp"
#include "const.hpp"
#include "map_manager.hpp"

/**********************************************************************
INPUT LIGHT
Responsibilities
- Light placement + selection
- Move XZ / Y, radius, intensity, RGB tint
- Delete
- Save

How to 101 in case you struggle in operating in the program:
- LMB places a new light, or selects an existing one if you click near it
- arrow keys move the selected light left/right/forward/back
- PgUp/PgDn moves it up/down
- [ / ] shrinks/grows the light's radius
- + / - adjusts brightness
- Q/E, Z/X, C/V nudge red, green, blue tint down/up
- DEL deletes the selected light
- ctrl+S to save
**********************************************************************/

void editor_input_light(EditorState& editor, WorldMap& map, GLFWwindow* window,
    const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, float dt, bool& map_dirty){

    static bool s_lmb_last = false;
    static bool s_del_last = false;
    static bool s_pgup_last = false;
    static bool s_pgdn_last = false;
    static bool s_arr_left_last = false;
    static bool s_arr_right_last = false;
    static bool s_arr_up_last = false;
    static bool s_arr_down_last = false;

    bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS;

    // LMB: try select existing light first, else place new one
    if (lmb && !s_lmb_last && editor.placement_valid){
        // find closest light within 2m of click pos
        int closest_id = -1;
        float closest_dist = 2.0f;
        for (const auto& l : map.lights){
            glm::vec3 d = l.position - editor.ghost_pos;
            d.y = 0.0f;
            float dist = glm::length(d);
            if (dist < closest_dist){ closest_dist = dist; closest_id = l.id; }
        }

        if (closest_id != -1){
            // select existing light and load its values into staging
            editor.selected_light_id = closest_id;
            for (const auto& l : map.lights){
                if (l.id != closest_id) continue;
                editor.light_edit_color = l.color;
                editor.light_edit_radius = l.radius;
                editor.light_edit_intensity = l.intensity;
                break;
            }
            std::cout << "[light] selected id=" << closest_id << "\n";
        }
        else {
            // place new light at cursor
            LightSource l;
            l.position  = editor.ghost_pos;
            l.position.y += 3.0f; // default streetlight height
            l.color = editor.light_edit_color;
            l.radius = editor.light_edit_radius;
            l.intensity = editor.light_edit_intensity;
            world_map_light_place(map, l);
            editor.selected_light_id = map.lights.back().id;
            map_dirty = true;
            std::cout << "[light] placed id=" << editor.selected_light_id
                      << " at (" << l.position.x << "," << l.position.z << ")\n";
        }
    }
    s_lmb_last = lmb;

    // edit selected light properties
    if (editor.selected_light_id != -1){
        for (auto& l : map.lights){
            if (l.id != editor.selected_light_id) continue;

            float step = shift ? 0.05f : 0.25f;

            // PgUp/PgDn nudge Y
            bool pgup = glfwGetKey(window, GLFW_KEY_PAGE_UP)   == GLFW_PRESS;
            bool pgdn = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;
            if (pgup && !s_pgup_last){ l.position.y += step; map_dirty = true; }
            if (pgdn && !s_pgdn_last){ l.position.y -= step; map_dirty = true; }
            s_pgup_last = pgup;
            s_pgdn_last = pgdn;

            // arrow keys move XZ
            bool al = glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS;
            bool ar = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
            bool au = glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS;
            bool ad = glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS;
            if (al && !s_arr_left_last)  { l.position.x -= step; map_dirty = true; }
            if (ar && !s_arr_right_last) { l.position.x += step; map_dirty = true; }
            if (au && !s_arr_up_last)    { l.position.z -= step; map_dirty = true; }
            if (ad && !s_arr_down_last)  { l.position.z += step; map_dirty = true; }
            s_arr_left_last  = al; s_arr_right_last = ar;
            s_arr_up_last    = au; s_arr_down_last  = ad;

            // [ / ] adjust radius
            bool brk_l = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
            bool brk_r = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
            if (brk_l){ l.radius = std::max(1.0f, l.radius - 4.0f * dt); map_dirty = true; }
            if (brk_r){ l.radius = std::min(80.0f, l.radius + 4.0f * dt); map_dirty = true; }

            // + / - adjust intensity (using = and - keys)
            bool plus  = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS;
            bool minus = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;
            if (plus)  { l.intensity = std::min(10.0f, l.intensity + 1.5f * dt); map_dirty = true; }
            if (minus) { l.intensity = std::max(0.0f, l.intensity - 1.5f * dt); map_dirty = true; }

            // R/G/B tint with numpad or Q/E/Z
            // Q/E = red down/up, Z/X = green, C/V = blue
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) l.color.r = std::max(0.0f, l.color.r - dt);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) l.color.r = std::min(1.0f, l.color.r + dt);
            if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) l.color.g = std::max(0.0f, l.color.g - dt);
            if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) l.color.g = std::min(1.0f, l.color.g + dt);
            if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) l.color.b = std::max(0.0f, l.color.b - dt);
            if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) l.color.b = std::min(1.0f, l.color.b + dt);

            // sync staging to live values
            editor.light_edit_color = l.color;
            editor.light_edit_radius = l.radius;
            editor.light_edit_intensity = l.intensity;
            break;
        }

        // DEL deletes selected light
        bool del = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
        if (del && !s_del_last){
            world_map_light_remove(map, editor.selected_light_id);
            editor.selected_light_id = -1;
            map_dirty = true;
            std::cout << "[light] deleted\n";
        }
        s_del_last = del;
    }

    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        world_map_save(map, g_maps.loaded_dir + "/map.txt");
}