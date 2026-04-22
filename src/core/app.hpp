#pragma once
#include "window.hpp"

struct App {
    Window window;
    float  last_time = 0.0f;
    float  accumulator = 0.0f;
    bool   running = false;
};

void app_init(App& app);
void app_run(App& app);
void app_shutdown(App& app);