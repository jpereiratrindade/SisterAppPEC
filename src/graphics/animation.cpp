#include "animation.h"
#include <cstring>
#include <algorithm>

namespace graphics {

void Transform::toMatrix(float mat[16]) const {
    // Convert Euler angles to radians
    const float PI = 3.14159265358979323846f;
    float rx = rotation.x * PI / 180.0f;
    float ry = rotation.y * PI / 180.0f;
    float rz = rotation.z * PI / 180.0f;
    
    // Compute rotation matrices
    float cx = std::cos(rx), sx = std::sin(rx);
    float cy = std::cos(ry), sy = std::sin(ry);
    float cz = std::cos(rz), sz = std::sin(rz);
    
    // Combined rotation matrix (ZYX order)
    float r00 = cy * cz;
    float r01 = cy * sz;
    float r02 = -sy;
    
    float r10 = sx * sy * cz - cx * sz;
    float r11 = sx * sy * sz + cx * cz;
    float r12 = sx * cy;
    
    float r20 = cx * sy * cz + sx * sz;
    float r21 = cx * sy * sz - sx * cz;
    float r22 = cx * cy;
    
    // Apply scale and build final matrix (column-major)
    mat[0]  = r00 * scale.x;
    mat[1]  = r10 * scale.x;
    mat[2]  = r20 * scale.x;
    mat[3]  = 0.0f;
    
    mat[4]  = r01 * scale.y;
    mat[5]  = r11 * scale.y;
    mat[6]  = r21 * scale.y;
    mat[7]  = 0.0f;
    
    mat[8]  = r02 * scale.z;
    mat[9]  = r12 * scale.z;
    mat[10] = r22 * scale.z;
    mat[11] = 0.0f;
    
    mat[12] = position.x;
    mat[13] = position.y;
    mat[14] = position.z;
    mat[15] = 1.0f;
}

void Animator::update(float dt) {
    // Auto-rotation
    if (autoRotate_) {
        float angle = rotationSpeed_ * dt;
        
        // Apply rotation around the specified axis
        // For simplicity, we'll just rotate around Y if axis is (0,1,0)
        if (std::abs(rotationAxis_.y - 1.0f) < 0.01f) {
            current_.rotation.y += angle;
            // Keep angle in [0, 360)
            if (current_.rotation.y >= 360.0f) {
                current_.rotation.y -= 360.0f;
            }
        } else if (std::abs(rotationAxis_.x - 1.0f) < 0.01f) {
            current_.rotation.x += angle;
            if (current_.rotation.x >= 360.0f) {
                current_.rotation.x -= 360.0f;
            }
        } else if (std::abs(rotationAxis_.z - 1.0f) < 0.01f) {
            current_.rotation.z += angle;
            if (current_.rotation.z >= 360.0f) {
                current_.rotation.z -= 360.0f;
            }
        }
    }
    
    // Interpolation
    if (isInterpolating_) {
        interpTime_ += dt;
        
        if (interpTime_ >= interpDuration_) {
            // Finished interpolating
            current_ = target_;
            isInterpolating_ = false;
            interpTime_ = 0.0f;
        } else {
            // Linear interpolation (lerp)
            float t = interpTime_ / interpDuration_;
            
            // Smooth step for nicer interpolation
            t = t * t * (3.0f - 2.0f * t);
            
            current_.position.x = interpolationStart_.position.x + (target_.position.x - interpolationStart_.position.x) * t;
            current_.position.y = interpolationStart_.position.y + (target_.position.y - interpolationStart_.position.y) * t;
            current_.position.z = interpolationStart_.position.z + (target_.position.z - interpolationStart_.position.z) * t;
            
            current_.rotation.x = interpolationStart_.rotation.x + (target_.rotation.x - interpolationStart_.rotation.x) * t;
            current_.rotation.y = interpolationStart_.rotation.y + (target_.rotation.y - interpolationStart_.rotation.y) * t;
            current_.rotation.z = interpolationStart_.rotation.z + (target_.rotation.z - interpolationStart_.rotation.z) * t;
            
            current_.scale.x = interpolationStart_.scale.x + (target_.scale.x - interpolationStart_.scale.x) * t;
            current_.scale.y = interpolationStart_.scale.y + (target_.scale.y - interpolationStart_.scale.y) * t;
            current_.scale.z = interpolationStart_.scale.z + (target_.scale.z - interpolationStart_.scale.z) * t;
        }
    }
}

void Animator::setAutoRotate(bool enabled, const math::Vec3& axis, float speed) {
    autoRotate_ = enabled;
    rotationAxis_ = axis;
    rotationSpeed_ = speed;
}

void Animator::interpolateTo(const Transform& target, float duration) {
    interpolationStart_ = current_;
    target_ = target;
    interpDuration_ = (duration > 0.0f) ? duration : 0.01f;
    interpTime_ = 0.0f;
    isInterpolating_ = true;
}

} // namespace graphics
