#include "trike_physics.hpp"
#include "../core/const.hpp"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

void trike_physics_update(TrikeState& state, const TrikeInput& input, float dt){

    // Steer
    // movement current stear angle toward the target at a fixed rate
    float steer_target = input.steer * glm::radians(Const::TRIKE_MAX_STEER_ANGLE);
    float steer_delta = glm::radians(Const::TRIKE_STEER_SPEED) * dt;
    float steer_diff = steer_target - state.steer_angle;

    // clamp the step so we can never overshoot the target
    state.steer_angle  += std::clamp(steer_diff, -steer_delta, steer_delta);

    // Longitudinal forces
    float engine = input.throttle * Const::TRIKE_ENGINE_FORCE;
    float brake = input.brake * Const::TRIKE_BRAKE_FORCE;

    // brake always opposes current motion
    float brake_force = brake * (state.speed>= 0.0f ?  -1.0f : 1.0f);

    // rolling friction : proportional to speed, always opposes motion
    float friction = -state.speed * Const::TRIKE_FRICTION;

    float net_force = engine + brake_force + friction;
    float acceleration = net_force / Const::TRIKE_MASS;
    state.speed += acceleration * dt;

    // clamp to max speed in both directions
    state.speed = std::clamp(state.speed, 
        -Const::TRIKE_MAX_SPEED * 0.3f, // reverse is limited to 30% of max
        Const::TRIKE_MAX_SPEED);

        // dead stop : prevent creeping at near-zero speed with no input
        if (input.throttle == 0.0f && input.brake == 0.0f && std::abs(state.speed)< 0.05f)
            state.speed = 0.0f;

        // Bicycle steering model
        // turning radius from wheelbase and steer angle
        // R = wheelbase/ tan(steer_angle)
        // heading change per second = speed/ R = speed * tan(steer_engle)/ wheelbase
        if (std::abs(state.steer_angle)> 0.001f){
            float turn_rate = (state.speed * std::tan(state.steer_angle))/ Const::TRIKE_WHEELBASE;

            // speed sensitivity- prevent trike from spinning like a beyblade at same speeds
            // low speed turn is tight
            // high speed turn is very loose
            // dividing by speed-based factor prevents beyblade spinning at low speed
            float speed_factor = 1.0f + std::abs(state.speed) * 0.15f;
            turn_rate /= speed_factor;

            // sidecar drag:: asymmetric resistance
            // sidecar is on the right (+Z), so left turns (positive turn_rate) are harder
            float sidecar_resist = (turn_rate > 0.0f) ? 0.6f : 1.0f;
            turn_rate *= sidecar_resist;

            state.heading += turn_rate * dt;
        }
        
        // wrap heading to [-pi, pi]
        state.heading = std::remainder(state.heading, 2.0f * glm::pi<float>());

        //integrate position
        // heading 0 = facing +X,
        // so forward vec is (cos(heading), 0 , sin(heading))
        glm::vec3 forward = {
            std::cos(state.heading),
            0.0f,
            std::sin(state.heading)
        };
        state.position += forward * state.speed * dt;
    
}