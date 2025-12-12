#pragma once

#include <array>
#include <cmath>

namespace math {

/**
 * @brief Represents a 3D plane equation: ax + by + cz + d = 0
 */
struct Plane {
    float a, b, c, d;

    /**
     * @brief Normalize the plane equation
     */
    void normalize() {
        float mag = std::sqrt(a * a + b * b + c * c);
        if (mag > 0.0f) {
            a /= mag;
            b /= mag;
            c /= mag;
            d /= mag;
        }
    }

    /**
     * @brief Calculate signed distance from point to plane
     */
    float distanceToPoint(float x, float y, float z) const {
        return a * x + b * y + c * z + d;
    }
};

/**
 * @brief View frustum defined by 6 planes
 */
struct Frustum {
    enum PlaneIndex {
        LEFT = 0,
        RIGHT,
        BOTTOM,
        TOP,
        NEAR,
        FAR
    };

    std::array<Plane, 6> planes;
};

/**
 * @brief Axis-Aligned Bounding Box
 */
struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;

    AABB() : minX(0), minY(0), minZ(0), maxX(0), maxY(0), maxZ(0) {}
    
    AABB(float x0, float y0, float z0, float x1, float y1, float z1)
        : minX(x0), minY(y0), minZ(z0), maxX(x1), maxY(y1), maxZ(z1) {}
};

/**
 * @brief Extract frustum planes from view-projection matrix (column-major)
 * 
 * Uses the Gribb-Hartmann method to extract the 6 frustum planes
 * from a combined view-projection matrix.
 * 
 * @param viewProj 16-element column-major VP matrix
 * @return Extracted and normalized frustum
 */
Frustum extractFrustum(const float* viewProj);

/**
 * @brief Test if an AABB intersects or is inside a frustum
 * 
 * Uses the separating axis theorem: if the AABB is completely
 * outside any plane, it's not visible.
 * 
 * @param aabb The bounding box to test
 * @param frustum The view frustum
 * @return true if AABB is at least partially inside frustum
 */
bool testAABBFrustum(const AABB& aabb, const Frustum& frustum);

} // namespace math
