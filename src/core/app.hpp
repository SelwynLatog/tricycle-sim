#pragma once
#include "window.hpp"
#include "../physics/trike_state.hpp"
#include "../physics/trike_physics.hpp"
#include "../renderer/hud.hpp"

struct App {
    Window window;
    TrikeState trike;
    Hud hud;
    float  last_time = 0.0f;
    float  accumulator = 0.0f;
    bool   running = false;
};

void app_init(App& app);
void app_run(App& app);
void app_shutdown(App& app);