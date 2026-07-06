#include "scene_daytime_weather.hpp"
#include "../core/const.hpp"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

/**********************************************************************
SCENE DAYTIME WEATHER
Responsibilities
- Day/night clock advancement
- Sun direction + light color/ambient/diffuse blending across
  morning/afternoon/night periods
- Sky tint/flip/blend parameters for scene_draw_sky
- Fog color + distance, including golden-hour and night pull
- Rain overcast desaturation of light/fog
**********************************************************************/

void scene_update_daytime(SceneState& scene, float dt){
    // IN GAME TIME HERE:
    // 10 IRL MINUTES = 24 IN GAME HOURS
    float time_scale = 24.0f / Const::DAY_DURATION_SECONDS;
    scene.day_time += dt * time_scale;
    if (scene.day_time >= 24.0f) scene.day_time -= 24.0f;

    float t = scene.day_time;

    // sun elevation: rises at 5am, peaks at noon, sets at 19:00
    // map t=5 -> 0 deg, t=12 -> 90 deg, t=19 -> 0 deg
    // use a sine over the day window
    float day_frac = glm::clamp((t - 5.0f) / 14.0f, 0.0f, 1.0f); // 0 at 5am, 1 at 7pm
    float elevation = glm::pi<float>() * day_frac; // 0 -> pi
    float sun_y = std::sin(elevation);
    // azimuth rotates east(morning) to west(evening)
    float azimuth = glm::pi<float>() * day_frac; // east at dawn, west at dusk
    float sun_x = std::cos(azimuth);
    float sun_z = 0.3f; // slight south offset for our lat

    // at night use a dim moon direction
    bool is_night = (t < 5.0f || t >= 19.0f);
    if (is_night){
        scene.sun_dir = glm::normalize(glm::vec3(0.3f, 0.5f, 0.2f));
    } 
    else {
        scene.sun_dir = glm::normalize(glm::vec3(sun_x, sun_y, sun_z));
    }

    // determine which two periods we're blending between
    // period A = current, period B = next, blend = 0->1 over FADE_DURATION hours
    glm::vec3 col_morning = {Const::LIGHT_MORNING_R, Const::LIGHT_MORNING_G, Const::LIGHT_MORNING_B};
    glm::vec3 col_afternoon = {Const::LIGHT_AFTERNOON_R, Const::LIGHT_AFTERNOON_G, Const::LIGHT_AFTERNOON_B};
    glm::vec3 col_night = {Const::LIGHT_NIGHT_R, Const::LIGHT_NIGHT_G, Const::LIGHT_NIGHT_B};

    float amb_morning = Const::LIGHT_MORNING_AMBIENT;
    float amb_afternoon = Const::LIGHT_AFTERNOON_AMBIENT;
    float amb_night = Const::LIGHT_NIGHT_AMBIENT;
    float diff_morning = Const::LIGHT_MORNING_DIFF;
    float diff_afternoon= Const::LIGHT_AFTERNOON_DIFF;
    float diff_night = Const::LIGHT_NIGHT_DIFF;

    float fade = Const::DAY_FADE_DURATION;

    auto blend_f = [](float edge, float t, float fade) -> float {
        return glm::clamp((t - edge) / fade, 0.0f, 1.0f);
    };

    if (t >= Const::DAY_MORNING_START && t < Const::DAY_AFTERNOON_START){
        float b = glm::clamp((t - Const::DAY_MORNING_START) /
            (Const::DAY_AFTERNOON_START - Const::DAY_MORNING_START), 0.0f, 1.0f);
        scene.sky_uv_offset = glm::mix(0.0f, 0.25f, b); // slowly drifts east to south
    } 
    else if (t >= Const::DAY_AFTERNOON_START && t < Const::DAY_NIGHT_START){
        float b = glm::clamp((t - Const::DAY_AFTERNOON_START) /
            (Const::DAY_NIGHT_START - Const::DAY_AFTERNOON_START), 0.0f, 1.0f);
        scene.sky_uv_offset = glm::mix(0.25f, 0.50f, b); // south to west
    } 
    else {
        scene.sky_uv_offset = 0.0f;
    }

    if (t >= Const::DAY_NIGHT_START){
        // afternoon -> night: day flipped+orange fades to sky_night
        float b = blend_f(Const::DAY_NIGHT_START, t, fade);
        scene.light_color = glm::mix(col_afternoon, col_night, b);
        scene.ambient = glm::mix(amb_afternoon, amb_night, b);
        scene.diff_intensity = glm::mix(diff_afternoon, diff_night, b);
        scene.sky_tint_a = glm::vec3(1.0f, 0.55f, 0.25f); // golden afternoon
        scene.sky_tint_b = glm::vec3(1.0f);
        scene.sky_flip_a = 1;
        scene.sky_flip_b = 0;
        scene.sky_use_night_b = 1;
        scene.sky_blend = b;
    }
    else if (t >= Const::DAY_AFTERNOON_START){
        // morning -> afternoon: day normal fades to day flipped+orange
        float b = blend_f(Const::DAY_AFTERNOON_START, t, fade);
        scene.light_color = glm::mix(col_morning, col_afternoon, b);
        scene.ambient = glm::mix(amb_morning, amb_afternoon, b);
        scene.diff_intensity = glm::mix(diff_morning, diff_afternoon, b);
        scene.sky_tint_a = glm::vec3(1.0f); // normal day
        scene.sky_tint_b = glm::vec3(1.0f, 0.55f, 0.25f); // golden tint
        scene.sky_flip_a = 0;
        scene.sky_flip_b = 1;
        scene.sky_use_night_b = 0;
        scene.sky_blend = b;
    }
    else if (t >= Const::DAY_MORNING_START){
        // night -> morning: sky_night fades to day
        float b = blend_f(Const::DAY_MORNING_START, t, fade);
        scene.light_color = glm::mix(col_night, col_morning, b);
        scene.ambient = glm::mix(amb_night, amb_morning, b);
        scene.diff_intensity = glm::mix(diff_night, diff_morning, b);
        scene.sky_tint_a = glm::vec3(1.0f); // night tex as-is
        scene.sky_tint_b = glm::vec3(1.0f); // day tex
        scene.sky_flip_a = 0;
        scene.sky_flip_b = 0;
        scene.sky_use_night_b = 0; // A=night B=day, but A uses night tex
        scene.sky_use_night_b = 0;
        scene.sky_tint_a = glm::vec3(1.0f); // day
        scene.sky_tint_b = glm::vec3(1.0f); // night
        scene.sky_flip_a = 0;
        scene.sky_flip_b = 0;
        scene.sky_use_night_b = 1; // B=night, blend goes 1->0 (night fades out)
        scene.sky_blend = 1.0f - b; // inverted: starts at night, fades to day
    }
    else {
        // deep night
        scene.light_color = col_night;
        scene.ambient = amb_night;
        scene.diff_intensity = diff_night;
        scene.sky_tint_a = glm::vec3(1.0f);
        scene.sky_tint_b = glm::vec3(1.0f);
        scene.sky_flip_a = 0;
        scene.sky_flip_b = 0;
        scene.sky_use_night_b = 1;
        scene.sky_blend = 1.0f; // fully night
    }

    // night_factor: 0 at peak day (noon), 1 at full night
    // ramps up from DAY_AFTERNOON_START, fully on by DAY_NIGHT_START
    // ramps down from DAY_MORNING_START, fully off by DAY_MORNING_START + fade
    float nf = 0.0f;
    if (t >= Const::DAY_NIGHT_START)
        nf = glm::clamp((t - Const::DAY_NIGHT_START) / Const::DAY_FADE_DURATION, 0.0f, 1.0f);
    else if (t < Const::DAY_MORNING_START)
        nf = 1.0f;
    else if (t < Const::DAY_MORNING_START + Const::DAY_FADE_DURATION)
        nf = 1.0f - glm::clamp((t - Const::DAY_MORNING_START) / Const::DAY_FADE_DURATION, 0.0f, 1.0f);
    scene.night_factor = nf;

    // FOG DEFS HERE    
    glm::vec3 fog_day = glm::vec3(0.68f, 0.78f, 0.90f); // pale blue sky haze
    glm::vec3 fog_golden  = glm::vec3(0.95f, 0.45f, 0.10f); // deep warm orange
    glm::vec3 fog_night = glm::vec3(0.04f, 0.05f, 0.10f); // deep night

    // golden hour factor: peaks at DAY_AFTERNOON_START, fades by DAY_NIGHT_START
    float golden_t = 0.0f;
    if (t >= Const::DAY_AFTERNOON_START && t < Const::DAY_NIGHT_START) {
        float span = Const::DAY_NIGHT_START - Const::DAY_AFTERNOON_START;
        float mid  = Const::DAY_AFTERNOON_START + span * 0.5f;
        golden_t = 1.0f - std::abs(t - mid) / (span * 0.5f);
        golden_t = glm::clamp(golden_t, 0.0f, 1.0f);
        golden_t = golden_t * golden_t; // ease in
    }

    float fog_blend = glm::clamp(scene.night_factor * 1.2f, 0.0f, 1.0f);
    glm::vec3 fog_base = glm::mix(fog_day, fog_night, fog_blend);
    fog_base = glm::mix(fog_base, fog_golden, golden_t * 0.85f);

    scene.fog_color = fog_base * scene.light_color * 1.2f;
    scene.fog_color = glm::clamp(scene.fog_color, glm::vec3(0.0f), glm::vec3(1.0f));

    
    float day_far_base = 520.0f;
    float day_near_base = 180.0f;

    float golden_pull_near = golden_t * 100.0f;
    float golden_pull_far  = golden_t * 220.0f;

    float night_pull_near = scene.night_factor * 160.0f;
    float night_pull_far = scene.night_factor * 360.0f;

    float effective_golden_near = golden_pull_near * (1.0f - scene.sky_rain_blend);
    float effective_golden_far  = golden_pull_far  * (1.0f - scene.sky_rain_blend);
    float rain_fog_near_pull = glm::mix(140.0f, 60.0f, scene.night_factor);
    float rain_fog_far_pull  = glm::mix(320.0f, 120.0f, scene.night_factor);
    scene.fog_near = day_near_base - effective_golden_near - night_pull_near - scene.sky_rain_blend * rain_fog_near_pull;
    scene.fog_far  = day_far_base  - effective_golden_far  - night_pull_far - scene.sky_rain_blend * rain_fog_far_pull;
    // hard floor so fog never starts at or behind the camera
    scene.fog_near = glm::max(scene.fog_near, 8.0f);
    scene.fog_far = glm::max(scene.fog_far,  scene.fog_near + 40.0f);

    // RAIN COLOR DESATURATION
    // grey-blue overcast light, kills the warm morning/golden tones
    if (scene.sky_rain_blend > 0.01f){
        // day: cool blue-grey overcast light
        // night: don't touch light_color much, just boost ambient so scene stays readable
        glm::vec3 rain_light_day = glm::vec3(0.72f, 0.78f, 0.88f); // cool overcast day
        glm::vec3 rain_light_night = glm::vec3(0.30f, 0.33f, 0.38f); // dim tint
        glm::vec3 rain_light = glm::mix(rain_light_day, rain_light_night, scene.night_factor);

        // day: flatten diffuse shadows. night: boost ambient so objects are readable
        float target_ambient = glm::mix(0.55f, 0.40f, scene.night_factor);
        float target_diff = glm::mix(0.25f, 0.18f, scene.night_factor);
        scene.light_color = glm::mix(scene.light_color, rain_light, scene.sky_rain_blend);
        scene.ambient = glm::mix(scene.ambient, target_ambient, scene.sky_rain_blend);
        scene.diff_intensity = glm::mix(scene.diff_intensity, target_diff, scene.sky_rain_blend);

        // fog: day pulls in hard, night pulls in even harder for that claustrophobic wet feel
        glm::vec3 rain_fog_day = glm::vec3(0.48f, 0.52f, 0.58f);
        glm::vec3 rain_fog_night = glm::vec3(0.06f, 0.07f, 0.09f);
        glm::vec3 rain_fog = glm::mix(rain_fog_day, rain_fog_night, scene.night_factor);
        scene.fog_color = glm::mix(scene.fog_color, rain_fog, scene.sky_rain_blend);
        scene.fog_color = glm::clamp(scene.fog_color, glm::vec3(0.0f), glm::vec3(1.0f));
    }

    // lerp rain sky blend toward target
    float rain_lerp_speed = (scene.sky_rain_target > scene.sky_rain_blend) ? 0.012f : 0.006f;
    scene.sky_rain_blend = glm::mix(scene.sky_rain_blend, scene.sky_rain_target, rain_lerp_speed);

    static float s_last_printed = -1.0f;
    if ((int)scene.day_time != (int)s_last_printed){
        std::cout << "[day] t=" << scene.day_time << " night_factor=" << nf << "\n";
        s_last_printed = scene.day_time;
    }
}