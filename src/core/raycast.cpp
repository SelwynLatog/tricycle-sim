#include "raycast.hpp"
#include "const.hpp"
#include <algorithm>
#include <cmath>

/**********************************************************************
RAYCAST
Responsibilities
- Screen-to-world ray unprojection
- Ground hit test NOTE: heightfield march or flat y=0 fallback
- Object AABB hit test closest-t or smallest-volume selection
**********************************************************************/

bool editor_raycast_ground( double mx, double my,
    const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, glm::vec3& out_pos,
    const HeightField* terrain){

    // convert screen pixel to NDC [-1, 1]
    float ndc_x = (2.0f * (float)mx / screen_w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * (float)my / screen_h);

    // unproject two points to NDC z=-1 and z=1 to get ray direction
    glm::mat4 inv = glm::inverse(proj * view);

    glm::vec4 near_pt = inv * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 far_pt = inv * glm::vec4(ndc_x, ndc_y,  1.0f, 1.0f);

    near_pt /= near_pt.w;
    far_pt /= far_pt.w;

    glm::vec3 ray_origin = glm::vec3(near_pt);
    glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt) - ray_origin);

    if (terrain && terrain->rows > 0){
        // iterative ray march against heightfield
        // step along ray until we dip below terrain surface
        // then binary search the crossing for precision
        float step = terrain->cell_size * 0.5f;
        float t = 0.0f;
        float t_max = Const::CAM_FAR;
        float prev_t = 0.0f;

        while (t < t_max){
            glm::vec3 p = ray_origin + ray_dir * t;
            float ground = heightfield_sample(*terrain, p.x, p.z);
            if (p.y <= ground){
                float lo =  prev_t, hi = t;
                for (int i = 0; i<8; i++){
                    float mid = (lo + hi) * 0.5f;
                    glm::vec3 mp = ray_origin + ray_dir * mid;
                    if (mp.y <= heightfield_sample(*terrain, mp.x, mp.z)) hi = mid;
                    else lo = mid;
                }
                out_pos = ray_origin + ray_dir * ((lo+hi) * 0.5f);
                out_pos.y = heightfield_sample(*terrain, out_pos.x, out_pos.z);
                return true;
            }
            prev_t = t;
            t += step;
        }
        return false;
    }

    // flat y=0 fallback when no terrain
    if (std::abs(ray_dir.y) < 1e-6f) return false;
    float t = -ray_origin.y / ray_dir.y;
    if (t < 0.0f) return false;
    out_pos = ray_origin + ray_dir * t;
    return true;

}

int editor_raycast_objects(double mx, double my, const glm::mat4& view, const glm::mat4& proj,
    int screen_w, int screen_h, const WorldMap& map, const EditorRenderer& er,
    bool prefer_small){

    float ndc_x = (2.0f * (float)mx / screen_w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * (float)my / screen_h);

    glm::mat4 inv = glm::inverse(proj * view);
    glm::vec4 near_pt = inv * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv * glm::vec4(ndc_x, ndc_y,  1.0f, 1.0f);
    near_pt /= near_pt.w;
    far_pt  /= far_pt.w;

    glm::vec3 ray_origin = glm::vec3(near_pt);
    glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt) - ray_origin);

    float closest_t = 1e9f;
    float smallest_vol = 1e9f;
    int hit_id = -1;

    for (const auto& o : map.objects){
        glm::vec3 aabb_min, aabb_max;
        auto bit = er.prop_bounds.find(o.model_path);
        if (bit != er.prop_bounds.end()){
            float yoff = er.prop_y_offset.count(o.model_path)
                ? er.prop_y_offset.at(o.model_path) : 0.0f;
            
            float yaw = o.rotation.y;
            glm::vec3 lmin = bit->second.local_min;
            glm::vec3 lmax = bit->second.local_max;
            glm::vec3 smin = { lmin.x * o.scale.x, (lmin.y + yoff) * o.scale.y, lmin.z * o.scale.z };
            glm::vec3 smax = { lmax.x * o.scale.x, (lmax.y + yoff) * o.scale.y, lmax.z * o.scale.z };
            float c = std::cos(yaw), s = std::sin(yaw);
            aabb_min = glm::vec3( 1e9f);
            aabb_max = glm::vec3(-1e9f);
            for (int k = 0; k < 8; k++){
                glm::vec3 corner = {
                    (k & 1) ? smax.x : smin.x,
                    (k & 2) ? smax.y : smin.y,
                    (k & 4) ? smax.z : smin.z,
                };
                glm::vec3 world = o.position + glm::vec3(
                    c * corner.x - s * corner.z, corner.y, s * corner.x + c * corner.z);
                aabb_min = glm::min(aabb_min, world);
                aabb_max = glm::max(aabb_max, world);
            }
        }
        else {
            glm::vec3 half = o.scale * 0.5f;
            aabb_min = o.position + glm::vec3(-half.x, 0.0f, -half.z);
            aabb_max = o.position + glm::vec3( half.x, o.scale.y, half.z);
        }

        float tmin = 0.0f, tmax = 1e9f;
        for (int i = 0; i < 3; i++){
            float inv_d = 1.0f / ray_dir[i];
            float t0 = (aabb_min[i] - ray_origin[i]) * inv_d;
            float t1 = (aabb_max[i] - ray_origin[i]) * inv_d;
            if (inv_d < 0.0f) std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
            if (tmax < tmin) goto next_object;
        }

        if (prefer_small){
            glm::vec3 sz  = aabb_max - aabb_min;
            float vol = sz.x * sz.y * sz.z;
            if (vol < smallest_vol){ smallest_vol = vol; hit_id = o.id; }
        }
        else {
            if (tmin < closest_t){ closest_t = tmin; hit_id = o.id; }
        }

        next_object:;
    }

    return hit_id;
}