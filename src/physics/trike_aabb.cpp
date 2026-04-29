#include "trike_aabb.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

void aabb_update(Aabb& box, const glm::vec3& position, float heading){
    // we have a local box of half-extents (HEXT_X, HEXT_Y, HEXT_Z)
    // we want its world-space AABB after rotating by heading around Y
    //
    // for a rotated box, the world-space half-extent on each axis is:
    //   hx_world = |cos(h)| * HEXT_X + |sin(h)| * HEXT_Z
    //   hz_world = |sin(h)| * HEXT_X + |cos(h)| * HEXT_Z
    // Y is unaffected by a yaw rotation
    //
    // this gives a tight AABB envelope

    float ch= std::abs(std::cos(heading));
    float sh= std::abs(std::sin(heading));

    float hx= ch * TRIKE_HEXT_X + sh * TRIKE_HEXT_Z;
    float hy= TRIKE_HEXT_Y;
    float hz= sh * TRIKE_HEXT_X + ch * TRIKE_HEXT_Z;

    // position is the rear-axle ground point, so lift center up by half-height
    glm::vec3 center= position + glm::vec3(0.0f, hy, 0.0f);

    box.min= center - glm::vec3(hx, hy, hz);
    box.max= center + glm::vec3(hx, hy, hz);
}

bool aabb_overlap(const Aabb& a, const Aabb& b){
    // separated on ANY axis means no collision
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;
    return true;
}

glm::vec3 aabb_mtv(const Aabb& a, const Aabb& b){
    // penetration depth on each axis
    float dx1= b.max.x - a.min.x;  // push a in +x
    float dx2= a.max.x - b.min.x;  // push a in -x
    float dy1= b.max.y - a.min.y;
    float dy2= a.max.y - b.min.y;
    float dz1= b.max.z - a.min.z;
    float dz2= a.max.z - b.min.z;

    // pick shallowest penetration per axis, preserve sign
    float px= (dx1 < dx2) ?  dx1 : -dx2;
    float py= (dy1 < dy2) ?  dy1 : -dy2;
    float pz= (dz1 < dz2) ?  dz1 : -dz2;

    // resolve along the axis with the smallest absolute penetration
    float ax= std::abs(px), ay = std::abs(py), az = std::abs(pz);

    if (ax <= ay && ax <= az) return glm::vec3(px, 0.0f, 0.0f);
    if (az <= ay && az <= ax) return glm::vec3(0.0f, 0.0f, pz);
    return glm::vec3(0.0f, py, 0.0f);
}