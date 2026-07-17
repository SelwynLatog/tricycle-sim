#pragma once
#include "app.hpp"
#include "npc_update.hpp"
// handles global hotkeys that apply in both editor and drive mode:
// H (hitbox toggle)
// TAB (editor/drive switch)
// ESC (settings menu + map hot-reload)
void app_handle_global_keys(App& app, float dt);
