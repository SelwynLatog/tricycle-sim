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

    // tricycle model path
    inline constexpr const char* TRIKE_MODEL_PATH = "../assets/TRAYSIKEL.obj";

    // cam
    inline constexpr float CAM_YAW_DEFAULT= 180.0f;
    inline constexpr float CAM_PITCH_DEFAULT= 25.0f;
    inline constexpr float CAM_DIST_DEFAULT= 6.0f;
    inline constexpr float CAM_PITCH_MIN= 5.0f;
    inline constexpr float CAM_PITCH_MAX= 85.0f;
    inline constexpr float CAM_YAW_SPEED= 60.0f;  // degrees/sec
    inline constexpr float CAM_PITCH_SPEED= 40.0f;  // degrees/sec
    inline constexpr float CAM_FOV= 55.0f;  // degrees
    inline constexpr float CAM_NEAR= 0.1f;
    inline constexpr float CAM_FAR= 200.0f;
    inline constexpr float CAM_ORBIT_TARGET_Y= 0.5f;

    // lighting
    inline constexpr float LIGHT_DIR_X= 1.0f;
    inline constexpr float LIGHT_DIR_Y= 2.0f;
    inline constexpr float LIGHT_DIR_Z= 1.0f;
    inline constexpr float LIGHT_AMBIENT= 0.55f;
    inline constexpr float LIGHT_DIFF= 0.85f;

    // ground & terrain
    inline constexpr float GROUND_HALF_EXTENT= 200.0f;
    inline constexpr float GROUND_Y_OFFSET= -0.002f;
    inline constexpr float GROUND_KD= 0.18f;   // uniform RGB, dark asphalt

    // model
    inline constexpr float MODEL_NORMALIZE_SIZE= 2.0f;  // auto-scale longest axis to this (metres)
    inline constexpr float MODEL_FLOOR_FUDGE= 0.03f; // nudge model down so wheels kiss ground

    // ground grid
    inline constexpr float GROUND_GRID_TILE_SIZE= 4.0f;
    inline constexpr float GROUND_KD_ALT= 0.22f;

    // axis gizmo cords
    inline constexpr float GIZMO_LENGTH = 3.0f;  // metres
}