#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;

uniform mat4  u_view;
uniform mat4  u_proj;
uniform float u_time;
uniform float u_y_level;

out vec3 v_world_pos;
out vec3 v_normal;

const float PI = 3.14159265;

vec3 gerstner(vec2 xz, vec2 dir, float wavelength, float amplitude, float speed) {
    float k     = 2.0 * PI / wavelength;
    float w     = sqrt(9.8 * k);
    float phase = dot(dir, xz) * k - w * u_time * speed;
    float Q     = 0.5;
    return vec3(
        dir.x * Q * amplitude * cos(phase),
        amplitude * sin(phase),
        dir.y * Q * amplitude * cos(phase)
    );
}

vec3 gerstner_normal(vec2 xz, vec2 dir, float wavelength, float amplitude, float speed) {
    float k     = 2.0 * PI / wavelength;
    float w     = sqrt(9.8 * k);
    float phase = dot(dir, xz) * k - w * u_time * speed;
    float Q     = 0.5;
    float wa    = k * amplitude;
    return vec3(
        -dir.x * wa * cos(phase),
        1.0 - Q * wa * sin(phase),
        -dir.y * wa * cos(phase)
    );
}

// simplex-ish 2D noise, used to jitter wave parameters over time so
// waves never exactly repeat
vec3 permute3(vec3 x){ return mod(((x*34.0)+1.0)*x, 289.0); }
float snoise(vec2 v){
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
                        -0.577350269189626, 0.024390243902439);
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod(i, 289.0);
    vec3 p = permute3(permute3(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
    m = m*m;
    m = m*m;
    vec3 x = 2.0 * fract(p * 0.024390243902439) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0 + h*h);
    vec3 g;
    g.x  = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

// two-octave noise: slow/large drift + fast/small jitter, matches ref shader's GetNoise
vec2 wave_noise(vec2 pos, vec2 timed_wind){
    float n1 = snoise(pos * 0.015 + timed_wind * 0.0005);
    float n2 = snoise(pos * 0.1  + timed_wind * 0.002);
    return clamp(vec2(n1, n2), 0.0, 1.0);
}

// sine wave
// used for small detail-scale chop
vec3 sine_wave(vec2 xz, vec2 dir, float wavelength, float amplitude, float speed) {
    float k = 2.0 * PI / wavelength;
    float w = sqrt(9.8 * k);
    float phase = dot(dir, xz) * k - w * u_time * speed;
    return vec3(0.0, amplitude * sin(phase), 0.0);
}
vec3 sine_wave_normal(vec2 dir, float wavelength, float amplitude, float speed, float phase_cos) {
    float k   = 2.0 * PI / wavelength;
    float w   = sqrt(9.8 * k);
    float wac = w * amplitude * phase_cos;
    return vec3(-dir.x * wac, 1.0, -dir.y * wac);
}

void main() {
    vec2 xz = a_pos.xz;

    // wind direction shared by all waves - matches ref shader's single windDir
    vec2 wind_dir = normalize(vec2(1.0, 0.6));
    vec2 timed_wind = wind_dir * u_time;

    // two-octave noise jitters wavelength/amplitude every frame so waves
    // never exactly repeat - this is the main fix for the "cartoonish" look
    vec2 noise = wave_noise(xz, timed_wind);
    float jitter = noise.x - noise.x * 0.2 + noise.y * 0.1;

    // base wavelengths/amplitudes, perturbed by jitter (ref shader: half4(1,4,3,6) etc)
    float wl0 = 18.0 + jitter * 6.0;   // swell (gerstner)
    float wl1 = 11.0 + jitter * 4.0;   // swell (gerstner)
    float wl2 =  7.0 + jitter * 2.0;   // chop (sine)
    float wl3 =  5.0 + jitter * 1.5;   // chop (sine)

    float amp0 = 0.18 + jitter * 0.04;
    float amp1 = 0.10 + jitter * 0.03;
    float amp2 = 0.06 + jitter * 0.02;
    float amp3 = 0.04 + jitter * 0.015;

    vec3 d = vec3(0.0);
    vec3 n = vec3(0.0);

    // swell: Gerstner (has sideways displacement -> sharper peaks, rolling look)
    d += gerstner(xz, wind_dir, wl0, amp0, 1.0);
    d += gerstner(xz, normalize(vec2(-0.7, 1.0)), wl1, amp1, 1.2);
    n += gerstner_normal(xz, wind_dir, wl0, amp0, 1.0);
    n += gerstner_normal(xz, normalize(vec2(-0.7, 1.0)), wl1, amp1, 1.2);

    // chop: sine (vertical only, cheaper, reads as fine detail rather than swell)
    d += sine_wave(xz, normalize(vec2(0.3, -1.0)), wl2, amp2, 0.9);
    d += sine_wave(xz, normalize(vec2(-1.0, -0.4)), wl3, amp3, 1.4);

    float k2 = 2.0 * PI / wl2, w2 = sqrt(9.8 * k2);
    float phase2 = dot(normalize(vec2(0.3, -1.0)), xz) * k2 - w2 * u_time * 0.9;
    n += sine_wave_normal(normalize(vec2(0.3, -1.0)), wl2, amp2, 0.9, cos(phase2));

    float k3 = 2.0 * PI / wl3, w3 = sqrt(9.8 * k3);
    float phase3 = dot(normalize(vec2(-1.0, -0.4)), xz) * k3 - w3 * u_time * 1.4;
    n += sine_wave_normal(normalize(vec2(-1.0, -0.4)), wl3, amp3, 1.4, cos(phase3));

    vec3 world  = a_pos + d;
    v_world_pos = world;
    v_normal    = normalize(n);
    gl_Position = u_proj * u_view * vec4(world, 1.0);
}