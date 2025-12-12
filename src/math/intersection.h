#pragma once

#include "math_types.h"
#include <cmath>

namespace math {

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

// Returns distance to intersection, or -1.0f if no intersection
inline float intersectRaySphere(const Ray& ray, const Vec3& center, float radius) {
    Vec3 L = center - ray.origin;
    float tca = dot(L, ray.direction);
    if (tca < 0) return -1.0f; // Behind ray (simple check, though technically sphere could envelop origin)
    
    float d2 = dot(L, L) - tca * tca;
    float r2 = radius * radius;
    
    if (d2 > r2) return -1.0f;
    
    float thc = std::sqrt(r2 - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;
    
    if (t0 > 0) return t0;
    if (t1 > 0) return t1;
    
    return -1.0f;
}

} // namespace math
