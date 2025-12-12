#pragma once
#include "../math/math_types.h"
#include <cmath>

namespace graphics {

/**
 * @brief Represents a 3D transformation (position, rotation, scale)
 */
struct Transform {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Vec3 rotation{0.0f, 0.0f, 0.0f}; // Euler angles in degrees (X, Y, Z)
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
    
    /**
     * @brief Convert transform to a 4x4 matrix (column-major)
     * @param mat Output matrix (16 floats)
     */
    void toMatrix(float mat[16]) const;
};

/**
 * @brief Animates transformations over time
 */
class Animator {
public:
    Animator() = default;
    
    /**
     * @brief Update animation state
     * @param dt Delta time in seconds
     */
    void update(float dt);
    
    /**
     * @brief Enable/disable auto-rotation around an axis
     * @param enabled Whether auto-rotation is enabled
     * @param axis Rotation axis (should be normalized)
     * @param speed Rotation speed in degrees per second
     */
    void setAutoRotate(bool enabled, const math::Vec3& axis = {0.0f, 1.0f, 0.0f}, float speed = 45.0f);
    
    /**
     * @brief Smoothly interpolate to a target transform
     * @param target Target transformation
     * @param duration Duration of interpolation in seconds
     */
    void interpolateTo(const Transform& target, float duration);
    
    /**
     * @brief Get current transform (mutable)
     */
    Transform& transform() { return current_; }
    
    /**
     * @brief Get current transform (const)
     */
    const Transform& transform() const { return current_; }
    
    /**
     * @brief Check if currently interpolating
     */
    bool isInterpolating() const { return isInterpolating_; }
    
private:
    Transform current_;
    Transform target_;
    Transform interpolationStart_;
    
    bool isInterpolating_ = false;
    float interpTime_ = 0.0f;
    float interpDuration_ = 1.0f;
    
    bool autoRotate_ = false;
    math::Vec3 rotationAxis_{0.0f, 1.0f, 0.0f};
    float rotationSpeed_ = 45.0f; // degrees/sec
};

} // namespace graphics
