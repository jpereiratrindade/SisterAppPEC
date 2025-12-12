#pragma once

#include <cmath>

namespace math {

struct Vec3 { float x,y,z; };

inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator*(const Vec3& v, float s) { return {v.x*s, v.y*s, v.z*s}; }
inline Vec3 operator*(float s, const Vec3& v) { return {v.x*s, v.y*s, v.z*s}; }

inline float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float lengthSq(const Vec3& v) { return v.x*v.x + v.y*v.y + v.z*v.z; }
inline float length(const Vec3& v) { return std::sqrt(lengthSq(v)); }

inline Vec3 cross(const Vec3& a, const Vec3& b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    return (len > 0) ? Vec3{v.x/len, v.y/len, v.z/len} : Vec3{0,0,0};
}

} // namespace math
