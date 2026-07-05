#pragma once
#include "app.hpp"

// build DynamicSim entries for any DYNAMIC object not already simulated
// called on init:
// editor->drive switch
// map hot-reload
// R-key reset
void init_dynamic_sims(App& app);