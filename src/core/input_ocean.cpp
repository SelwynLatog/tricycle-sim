#include "input_ocean.hpp"
#include "const.hpp"
#include "map_manager.hpp"

/**********************************************************************
INPUT OCEAN
Responsibilities
- Ocean Y level nudge
- Ocean enable/disable toggle
- Save

How to 101 in case you struggle in operating in the program:
- PgUp/PgDn keys moves the water level up/down
- E key toggles the ocean on/off
- ctrl+S to save
**********************************************************************/

void editor_input_ocean(EditorState& editor, WorldMap& map, EditorRenderer& er,
    GLFWwindow* window, float dt){

    static bool s_pgup_last = false;
    static bool s_pgdn_last = false;
    static bool s_e_last = false;

    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    float step = shift ? 0.05f : 0.25f;

    // PgUp/PgDn nudge global ocean Y level
    bool pgup = glfwGetKey(window, GLFW_KEY_PAGE_UP)   == GLFW_PRESS;
    bool pgdn = glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;
    if (pgup && !s_pgup_last){
        map.ocean.y_level += step;
        map.ocean.mesh_dirty = true;
        er.terrain_surface_dirty = true;
    }
    if (pgdn && !s_pgdn_last){
        map.ocean.y_level -= step;
        map.ocean.mesh_dirty = true;
        er.terrain_surface_dirty = true;
    }
    s_pgup_last = pgup;
    s_pgdn_last = pgdn;

    // E toggles ocean on/off
    bool e_down = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    if (e_down && !s_e_last){
        map.ocean.enabled = !map.ocean.enabled;
        er.terrain_surface_dirty = true;
        std::cout << "[ocean] " << (map.ocean.enabled ? "enabled" : "disabled") << "\n";
    }
    s_e_last = e_down;

    if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        world_map_save(map, g_maps.loaded_dir + "/map.txt");
}