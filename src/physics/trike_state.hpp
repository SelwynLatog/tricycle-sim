#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// full kinematic state of the tricycle
// all values are in SI units (meters, radians, seconds)
// this struct acts as the single source of truth
struct TrikeState{
    
    glm::vec3 position = glm::vec3(0.0f); //world position of rear axle, ground level
    float heading = 0.0f; // yaw in radians, 0 = facing +X axis
    float speed = 0.0f; // signed scalar m/s, positive = forward
    float steer_angle = 0.0f; // current float wheel angle, radians 
};
