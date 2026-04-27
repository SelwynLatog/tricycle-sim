#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// full dynamic state of the tricycle
// all values are in SI units (meters, radians, seconds)
// this struct acts as the single source of truth
struct TrikeState{
    
    glm::vec3 position = glm::vec3(0.0f); //world position of rear axle, ground level
    float heading = 0.0f; // yaw in radians, 0 = facing +X axis
    float speed = 0.0f; // signed scalar m/s, positive = forward longitudinal
    float steer_angle = 0.0f; // current float wheel angle, radians
    float lateral_speed= 0.0f;
    
    // roll dynamics
    float roll_angle= 0.0f; // body roll, radians.positive= rolling right
    float roll_rate= 0.0f; // roll angular velocity, radians/sec
    
    // rollover state
    bool is_rolled_over= false; // true means trike has tipped, physics is frozen
    float rollover_timer= 0.0f; // count up after tip, respawn at threshold
};
