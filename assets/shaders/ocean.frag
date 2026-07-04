// Vertex stage stacks four Gerstner waves to displace the mesh and compute normals
// frag does fresnel based color mixing, specular highlight, sun directional sparkle
// via voronoi noise, screen space caustics, foam at wave crests, and horizon fade


#version 330 core
in vec3 v_world_pos;
in vec3 v_normal;
out vec4 frag_color;

uniform vec3  u_light_dir;
uniform vec3  u_cam_pos;
uniform float u_time;
uniform vec3  u_light_color;
uniform float u_ambient;
uniform float u_diff_intensity;
uniform sampler2D u_reflect_tex;  // mirrored scene render, see app.cpp reflection pass
uniform mat4  u_refl_view_proj; // projects world pos into u_reflect_tex UV space
uniform sampler2D u_normal_tex; // tiling water normal map (tuxalin/water-shader ref)
uniform sampler2D u_foam_tex; // deep/open-water foam pattern
uniform sampler2D u_foam_shore_tex; // shore foam pattern

float hash(vec2 p){
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

float vnoise(vec2 p){
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    vec2 u  = fp * fp * (3.0 - 2.0 * fp);
    return mix(
        mix(hash(ip),           hash(ip+vec2(1,0)), u.x),
        mix(hash(ip+vec2(0,1)), hash(ip+vec2(1,1)), u.x),
        u.y);
}

// Unpack a tangent-space normal map sample ([0,1] -> [-1,1])
vec3 unpackNormal(vec3 texSample){
    return normalize(texSample * 2.0 - 1.0);
}

// Simplified tangent basis for a mostly-horizontal water surface.
// Safe here since world-space UVs are just xz, so tangent is always +X-ish.
mat3 buildTangentFrame(vec3 n){
    vec3 t = normalize(cross(vec3(0.0, 0.0, 1.0), n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

void main(){
    vec3 view = normalize(u_cam_pos - v_world_pos);
    vec3 ldir = normalize(u_light_dir);
    vec2 xz = v_world_pos.xz;
    float dist  = length(u_cam_pos - v_world_pos);

    // Micro-normal (normal-map driven, replaces vnoise blob normals cause theyre freaking ugly)
    // Two UV sets at different scale/speed - turns one tileable map into
    // non-repeating, chaotic detail instead of flat blobby noise.
    float tiling1 = 0.5;
    float tiling2 = 2.2; // higher = finer detail layer, fills in the "flat" mid-distance
    // slight x/z stretch breaks isotropy - gives wind-aligned streaking instead of uniform ripple
    vec2 nuv1 = xz * vec2(tiling1 * 1.4, tiling1 * 0.8) + vec2( u_time * 0.020,  u_time * 0.014);
    vec2 nuv2 = xz * vec2(tiling2 * 1.2, tiling2 * 0.7) + vec2(-u_time * 0.031,  u_time * 0.023);
    vec3 nsample1 = unpackNormal(texture(u_normal_tex, nuv1).rgb);
    vec3 nsample2 = unpackNormal(texture(u_normal_tex, nuv2).rgb);

    // Whiteout-style blend this avoids flattening compared to a straight lerp
    vec3 blended_ts = normalize(vec3(nsample1.xy + nsample2.xy, nsample1.z * nsample2.z));

    mat3 tbn = buildTangentFrame(normalize(v_normal));
    float normal_strength = 0.85; // tune 0.5-1.2
    vec3 micro_n = normalize(v_normal + tbn * blended_ts * normal_strength - vec3(0,1,0) * (1.0 - normal_strength));

    // Fresnel
    // fade detail strength with distance more gently than sparkle does -
    // keeps fine texture visible further out instead of washing to flat mid-distance
    float detail_fade = 1.0 - smoothstep(40.0, 140.0, dist);
    micro_n = normalize(mix(v_normal, micro_n, mix(0.5, 1.0, detail_fade)));

    float NdotV   = max(dot(micro_n, view), 0.0);
    float fresnel = mix(0.04, 1.0, pow(1.0 - NdotV, 5.0));
    fresnel = clamp(fresnel, 0.0, 0.92); // prevent total whiteout at grazing angles

    // Base color
    vec3 col_shallow = vec3(0.08, 0.60, 0.72);
    vec3 col_deep = vec3(0.02, 0.20, 0.50);
    vec3 water_col = mix(col_deep, col_shallow, pow(NdotV, 0.6));

    // Sky + nearby-object reflection
    // project this fragment through the mirrored camera's view-proj to find
    // where it lands in the planar reflection texture (rendered in app.cpp:
    // sky + terrain + props + trike, all mirrored across the water plane)
    vec4 refl_clip = u_refl_view_proj * vec4(v_world_pos, 1.0);
    vec2 refl_uv = (refl_clip.xy / refl_clip.w) * 0.5 + 0.5;
    // ripple the sample point with the same micro-normal driving sparkle/foam -
    // this is what sells it as water instead of a flat mirror
    refl_uv += micro_n.xz * 0.035;

    vec3 sky_col;
    if (refl_clip.w > 0.0 && refl_uv.x > 0.001 && refl_uv.x < 0.999
        && refl_uv.y > 0.001 && refl_uv.y < 0.999){
        sky_col = texture(u_reflect_tex, refl_uv).rgb;
    } 
    else {
        // steep angle / outside the reflection buffer -> fall back to the old
        // fake horizon gradient so edges never sample garbage
        float h_t = pow(1.0 - NdotV, 3.0);
        vec3 sky_near = vec3(0.30, 0.65, 0.95);
        vec3 sky_horiz = vec3(0.76, 0.90, 1.00);
        sky_col = mix(sky_near, sky_horiz, h_t);
    }
  
    water_col = mix(water_col, sky_col, fresnel);

    // Light tint applied once after all color composition
    float diff = max(dot(micro_n, ldir), 0.0);
    float light = u_ambient + diff * u_diff_intensity * 0.08;
    light = max(light, 0.15);
    water_col = water_col * u_light_color * light;

    // Depth variation
    float wave_depth = smoothstep(-0.35, 0.35, v_world_pos.y);
    vec3 trough_tint = mix(water_col, col_deep,    0.07);
    vec3 crest_tint = mix(water_col, col_shallow, 0.05);
    water_col = mix(trough_tint, crest_tint, wave_depth);

    // Specular / sun glitter - no separate sparkle system needed anymore.
    // The normal-mapped micro_n makes this term produce natural glitter on its own.
    vec3  hv   = normalize(ldir + view);
    float spec_power = 220.0;
    float spec = pow(max(dot(micro_n, hv), 0.0), spec_power);

    float sun_strength = clamp(dot(u_light_color, vec3(0.333)), 0.0, 1.0);
    float dist_fade = 1.0 - smoothstep(20.0, 90.0, dist); // glitter fades with distance
    spec *= mix(0.4, 1.0, dist_fade) * sun_strength;
    spec = min(spec * 1.4, 1.3); // soft clamp - never blows to pure white

    water_col += vec3(1.00, 0.97, 0.88) * spec;

    // Caustics - tighter frequency, gated to shallow/near-shore water only.
    // Deep ocean shouldn't show caustics at all (nothing to bounce light off of).
    vec2 cx1 = xz * 8.0 + vec2( u_time*0.50, u_time*0.35);
    vec2 cx2 = xz * 8.0 + vec2(-u_time*0.38, u_time*0.58);
    float caustic = smoothstep(0.75, 0.95, vnoise(cx1) * vnoise(cx2) * 4.0);
    float c_fade  = 1.0 - smoothstep(0.0, 22.0, dist);
    float shallow_gate = smoothstep(-0.05, 0.30, v_world_pos.y);
    water_col += vec3(0.50, 0.84, 1.00) * caustic * c_fade * 0.14 * sun_strength * shallow_gate;   

    // Horizon fade
    float h_fade = smoothstep(35.0, 170.0, dist);
    water_col = mix(water_col, sky_col * 1.05, h_fade * 0.38);

    // Foam
    // i replaced ugly vnoise blobs for real textures
    vec2 foam_uv1 = xz * 0.6 + vec2(u_time * 0.05, u_time * 0.03);
    vec2 foam_uv2 = xz * 0.9 - vec2(u_time * 0.04, u_time * 0.06);
    float foam_tex1 = texture(u_foam_tex, foam_uv1).r;
    float foam_tex2 = texture(u_foam_shore_tex, foam_uv2).r;
    float foam_pattern = mix(foam_tex1, foam_tex2, 0.5);

    float foam_mask = smoothstep(0.32, 0.50, v_world_pos.y + 0.22);
    float foam = foam_mask * foam_pattern;
    water_col = mix(water_col, vec3(0.94, 0.97, 1.00), foam * 0.55);
    
    // Alpha
    float alpha = clamp(0.85 + fresnel * 0.12 + foam * 0.04, 0.83, 0.97);
    frag_color = vec4(water_col, alpha);
}