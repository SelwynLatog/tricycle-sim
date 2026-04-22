#pragma once

namespace Const{
    // window
    inline constexpr int WINDOW_WIDTH = 1920;
    inline constexpr int WINDOW_HEIGHT= 1080;
    inline constexpr const char* WINDOW_TITLE="tricyce sim";
    inline constexpr bool VSYNC = true;

    // render
    inline constexpr float CLEAR_R = 0.12f;
    inline constexpr float CLEAR_G = 0.12f;
    inline constexpr float CLEAR_B = 0.12f;

    // physics
    inline constexpr float GRAVITY= 9.81f; // m/s2
    inline constexpr float FIXED_TIMESTEP= 1.0f/ 120.0f; // physics ticks at 120hz
    inline constexpr float MAX_DELTA= 0.05f; //clamp dt to avoid spiral of death!

    // tricycle
    inline constexpr float TRIKE_MASS = 180.0f; // kg + rider
    inline constexpr float TRIKE_MAX_SPEED = 12.0f; // m/s (43 km/h)
    inline constexpr float TRIKE_ENGINE_FORCE = 800.0f; // N
    inline constexpr float TRIKE_BRAKE_FORCE= 1200.0f; // N
    inline constexpr float TRIKE_FRICTION = 0.85f; // rolling resistance coefficient
    inline constexpr float TRIKE_MAX_STEER_ANGLE = 35.0f; // degrees
    inline constexpr float TRIKE_STEER_SPEED = 90.0f; //degrees /sec
    inline constexpr float TRIKE_WHEELBASE = 1.8f; // metres, front to rear angle

}