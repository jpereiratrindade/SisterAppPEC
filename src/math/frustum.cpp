#include "frustum.h"

namespace math {

Frustum extractFrustum(const float* m) {
    // m is column-major 4x4 matrix
    // Access: m[col*4 + row]
    
    Frustum frustum;
    
    // Left plane: row4 + row1
    frustum.planes[Frustum::LEFT].a = m[3]  + m[0];
    frustum.planes[Frustum::LEFT].b = m[7]  + m[4];
    frustum.planes[Frustum::LEFT].c = m[11] + m[8];
    frustum.planes[Frustum::LEFT].d = m[15] + m[12];
    
    // Right plane: row4 - row1
    frustum.planes[Frustum::RIGHT].a = m[3]  - m[0];
    frustum.planes[Frustum::RIGHT].b = m[7]  - m[4];
    frustum.planes[Frustum::RIGHT].c = m[11] - m[8];
    frustum.planes[Frustum::RIGHT].d = m[15] - m[12];
    
    // Bottom plane: row4 + row2
    frustum.planes[Frustum::BOTTOM].a = m[3]  + m[1];
    frustum.planes[Frustum::BOTTOM].b = m[7]  + m[5];
    frustum.planes[Frustum::BOTTOM].c = m[11] + m[9];
    frustum.planes[Frustum::BOTTOM].d = m[15] + m[13];
    
    // Top plane: row4 - row2
    frustum.planes[Frustum::TOP].a = m[3]  - m[1];
    frustum.planes[Frustum::TOP].b = m[7]  - m[5];
    frustum.planes[Frustum::TOP].c = m[11] - m[9];
    frustum.planes[Frustum::TOP].d = m[15] - m[13];
    
    // Near plane: row4 + row3
    frustum.planes[Frustum::NEAR].a = m[3]  + m[2];
    frustum.planes[Frustum::NEAR].b = m[7]  + m[6];
    frustum.planes[Frustum::NEAR].c = m[11] + m[10];
    frustum.planes[Frustum::NEAR].d = m[15] + m[14];
    
    // Far plane: row4 - row3
    frustum.planes[Frustum::FAR].a = m[3]  - m[2];
    frustum.planes[Frustum::FAR].b = m[7]  - m[6];
    frustum.planes[Frustum::FAR].c = m[11] - m[10];
    frustum.planes[Frustum::FAR].d = m[15] - m[14];
    
    // Normalize all planes
    for (auto& plane : frustum.planes) {
        plane.normalize();
    }
    
    return frustum;
}

bool testAABBFrustum(const AABB& aabb, const Frustum& frustum) {
    // Test AABB against each frustum plane
    // AABB is outside if all 8 corners are on the negative side of any plane
    
    for (const auto& plane : frustum.planes) {
        // Find the "positive vertex" (furthest in plane normal direction)
        float px = (plane.a >= 0) ? aabb.maxX : aabb.minX;
        float py = (plane.b >= 0) ? aabb.maxY : aabb.minY;
        float pz = (plane.c >= 0) ? aabb.maxZ : aabb.minZ;
        
        // If positive vertex is outside (negative side), AABB is completely outside
        if (plane.distanceToPoint(px, py, pz) < 0) {
            return false;
        }
    }
    
    // AABB is at least partially inside frustum
    return true;
}

} // namespace math
