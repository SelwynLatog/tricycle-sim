#include "dynamics.hpp"
#include <cmath>

// RIGID BODY DYNAMICS INIT
// builds initial physics state for all DYNAMIC objects from their placed world positions
// skips objects already in the sim map so editor->drive transitions are non-destructive
void init_dynamic_sims(App& app){
    for (const auto& o : app.map.objects){
        if (o.behavior != DYNAMIC) continue;
        if (app.dynamic_sims.count(o.id)) continue;

        DynamicSim sim;
        sim.position = o.position;
        sim.yaw      = 0.0f;

        auto bit = app.editor_renderer.prop_bounds.find(o.model_path);
        if (bit != app.editor_renderer.prop_bounds.end()){
            float yoff = app.editor_renderer.prop_y_offset.count(o.model_path)
                ? app.editor_renderer.prop_y_offset.at(o.model_path) : 0.0f;
            glm::vec3 lmin = bit->second.local_min;
            glm::vec3 lmax = bit->second.local_max;
            glm::vec3 smin = { lmin.x*o.scale.x, (lmin.y+yoff)*o.scale.y, lmin.z*o.scale.z };
            glm::vec3 smax = { lmax.x*o.scale.x, (lmax.y+yoff)*o.scale.y, lmax.z*o.scale.z };
            float c = std::cos(o.rotation.y), s = std::sin(o.rotation.y);
            glm::vec3 wmin( 1e9f), wmax(-1e9f);
            for (int k = 0; k < 8; k++){
                glm::vec3 corner = {
                    (k & 1) ? smax.x : smin.x,
                    (k & 2) ? smax.y : smin.y,
                    (k & 4) ? smax.z : smin.z,
                };
                glm::vec3 world = sim.position + glm::vec3(
                    c * corner.x - s * corner.z, corner.y, s * corner.x + c * corner.z);
                wmin = glm::min(wmin, world);
                wmax = glm::max(wmax, world);
            }
            sim.aabb = { wmin, wmax };
        }
        else {
            glm::vec3 half = o.scale * 0.5f;
            sim.aabb = { sim.position - half, sim.position + half };
        }
        app.dynamic_sims[o.id] = sim;
    }
}