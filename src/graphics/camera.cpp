#include "camera.h"
#include "voxel_terrain.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstring>

namespace graphics {

// Math Helpers
// Local Math Helpers removed in favor of math_types.h
using math::Vec3;

Camera::Camera(float fov, float aspect, float nearZ, float farZ)
    : fov_(fov), aspect_(aspect), nearZ_(nearZ), farZ_(farZ) {
    theta_ = 45.0f * 3.14159f / 180.0f;
    phi_ = 45.0f * 3.14159f / 180.0f;
    
    // Identity
    std::fill(view_.begin(), view_.end(), 0.0f); view_[0]=view_[5]=view_[10]=view_[15]=1.0f;
    std::fill(proj_.begin(), proj_.end(), 0.0f); proj_[0]=proj_[5]=proj_[10]=proj_[15]=1.0f;
    
    updateView();
    updateProj();
}

void Camera::update(float dt) {
    (void)dt; // unused for now
    if (dirtyView_) updateView();
    if (dirtyProj_) updateProj();
}

void Camera::reset() {
    theta_ = 45.0f * 3.14159f / 180.0f;
    phi_ = 45.0f * 3.14159f / 180.0f;
    radius_ = 5.0f;
    target_ = {0.0f, 0.0f, 0.0f};
    dirtyView_ = true;
}

void Camera::processEvent(const SDL_Event& event) {
    if (mode_ == CameraMode::Orbital) {
        // Existing orbital camera logic
        if (event.type == SDL_MOUSEWHEEL) {
            float zoomSpeed = 0.5f;
            if (event.wheel.y > 0) radius_ -= zoomSpeed; // Zoom In
            if (event.wheel.y < 0) radius_ += zoomSpeed; // Zoom Out
            if (radius_ < 1.0f) radius_ = 1.0f;
            dirtyView_ = true;
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_MIDDLE || event.button.button == SDL_BUTTON_RIGHT) {
                isDragging_ = true;
                lastX_ = event.button.x;
                lastY_ = event.button.y;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            isDragging_ = false;
        }
        else if (event.type == SDL_MOUSEMOTION && isDragging_) {
            int dx = event.motion.x - lastX_;
            int dy = event.motion.y - lastY_;
            lastX_ = event.motion.x;
            lastY_ = event.motion.y;

            const Uint8* state = SDL_GetKeyboardState(nullptr);
            bool shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];

            if (shift) {
                // Pan
                float panSpeed = 0.01f * radius_ * 0.5f;
                
                Vec3 right = {view_[0], view_[4], view_[8]}; // Col 0
                Vec3 up = {view_[1], view_[5], view_[9]};    // Col 1
                
                Vec3 dX = right * (-(float)dx * panSpeed);
                Vec3 dY = up * ((float)dy * panSpeed); 
                
                target_[0] += dX.x + dY.x;
                target_[1] += dX.y + dY.y;
                target_[2] += dX.z + dY.z;
                
                dirtyView_ = true;
            } else {
                // Orbit (Left or Middle)
                if (event.motion.state & SDL_BUTTON_LMASK || event.motion.state & SDL_BUTTON_MMASK) {
                    float sensitivity = 0.005f;
                    theta_ -= static_cast<float>(dx) * sensitivity;
                    phi_ -= static_cast<float>(dy) * sensitivity;
                    
                    if (phi_ < 0.1f) phi_ = 0.1f;
                    if (phi_ > 3.0f) phi_ = 3.0f;
                    
                    dirtyView_ = true;
                }
            }
        }
    } else {
        // FreeFlight mode
        if (event.type == SDL_MOUSEWHEEL) {
             // Zoom (FOV)
             float fov = getFovDegrees();
             if (event.wheel.y > 0) fov -= 2.0f; // Zoom In
             if (event.wheel.y < 0) fov += 2.0f; // Zoom Out
             setFovDegrees(fov);
        }
        // Mouse look when right button held
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                isDragging_ = true;
                lastX_ = event.button.x;
                lastY_ = event.button.y;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_RIGHT) {
                isDragging_ = false;
            }
        }
        else if (event.type == SDL_MOUSEMOTION && isDragging_) {
            int dx = event.motion.x - lastX_;
            int dy = event.motion.y - lastY_;
            lastX_ = event.motion.x;
            lastY_ = event.motion.y;

            yaw_ += static_cast<float>(dx) * mouseSensitivity_;
            pitch_ -= static_cast<float>(dy) * mouseSensitivity_;

            // Clamp pitch to avoid gimbal lock
            if (pitch_ > 89.0f) pitch_ = 89.0f;
            if (pitch_ < -89.0f) pitch_ = -89.0f;

            dirtyView_ = true;
        }
    }
}

void Camera::processKeyboard(const Uint8* keyState, float dt) {
    if (mode_ != CameraMode::FreeFlight) return;

    // Update direction vectors from yaw/pitch
    float yawRad = yaw_ * 3.14159f / 180.0f;
    float pitchRad = pitch_ * 3.14159f / 180.0f;
    
    forward_.x = std::cos(yawRad) * std::cos(pitchRad);
    forward_.y = std::sin(pitchRad);
    forward_.z = std::sin(yawRad) * std::cos(pitchRad);
    forward_ = math::normalize(forward_);
    
    right_ = math::normalize(math::cross(forward_, {0.0f, 1.0f, 0.0f}));
    up_ = math::normalize(math::cross(right_, forward_));

    // Speed modifiers
    float speed = moveSpeed_;
    if (keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT]) {
        speed *= 3.0f; // Fast
    }
    if (keyState[SDL_SCANCODE_LALT] || keyState[SDL_SCANCODE_RALT]) {
        speed *= 0.3f; // Slow
    }

    float velocity = speed * dt;
    float rollSpeed = 60.0f; // degrees per second

    // WASD Movement
    if (keyState[SDL_SCANCODE_W]) {
        position_ = position_ + (forward_ * velocity);
        dirtyView_ = true;
    }
    if (keyState[SDL_SCANCODE_S]) {
        position_ = position_ - (forward_ * velocity);
        dirtyView_ = true;
    }
    if (keyState[SDL_SCANCODE_A]) {
        position_ = position_ - (right_ * velocity);
        dirtyView_ = true;
    }
    if (keyState[SDL_SCANCODE_D]) {
        position_ = position_ + (right_ * velocity);
        dirtyView_ = true;
    }

    // Flight vertical movement
    if (flying_) {
        // Debug
        // std::cout << "Flying: " << flying_ << " E:" << keyState[SDL_SCANCODE_E] << " Q:" << keyState[SDL_SCANCODE_Q] << " Vel:" << velocity << std::endl; 
        if (keyState[SDL_SCANCODE_E] || keyState[SDL_SCANCODE_Q]) {
             std::cout << "Flying Input detected! E:" << (int)keyState[SDL_SCANCODE_E] << " Q:" << (int)keyState[SDL_SCANCODE_Q] << " Vel:" << velocity << std::endl;
        }
        if (keyState[SDL_SCANCODE_E]) {
            position_.y += velocity;
            dirtyView_ = true;
        }
        if (keyState[SDL_SCANCODE_Q]) {
            position_.y -= velocity;
            dirtyView_ = true;
        }
    }

    // Camera roll (tilt) using Z / X keys
    if (keyState[SDL_SCANCODE_Z]) {
        addRoll(-rollSpeed * dt);
    }
    if (keyState[SDL_SCANCODE_X]) {
        addRoll(rollSpeed * dt);
    }
    
    // Jump (Space only if on ground - will be handled by jump() method)
    // Vertical movement removed - now handled by physics
}

void Camera::setCameraMode(CameraMode mode) {
    if (mode_ == mode) return;
    
    mode_ = mode;
    
    if (mode_ == CameraMode::FreeFlight) {
        // Transitioning to FreeFlight: start at current orbital position
        position_ = getPosition();
        yaw_ = -90.0f;
        pitch_ = 0.0f;
        roll_ = 0.0f;
    } else {
        // Transitioning to Orbital: keep current orbital params
        // (already have theta_, phi_, radius_, target_)
        roll_ = 0.0f;
    }
    
    dirtyView_ = true;
}

void Camera::teleportTo(const math::Vec3& pos) {
    if (mode_ == CameraMode::FreeFlight) {
        position_ = pos;
        dirtyView_ = true;
    } else {
        // In orbital mode, teleport by changing target
        target_[0] = pos.x;
        target_[1] = pos.y;
        target_[2] = pos.z;
        dirtyView_ = true;
    }
}

const float* Camera::viewMatrix() {
    if (dirtyView_) updateView();
    return view_.data();
}

const float* Camera::projectionMatrix() {
    if (dirtyProj_) updateProj();
    return proj_.data();
}

Vec3 Camera::getPosition() const {
    if (mode_ == CameraMode::Orbital) {
        float x = radius_ * std::sin(phi_) * std::cos(theta_);
        float y = radius_ * std::cos(phi_);
        float z = radius_ * std::sin(phi_) * std::sin(theta_);
        return {target_[0] + x, target_[1] + y, target_[2] + z};
    } else {
        return position_;
    }
}

void Camera::updateView() {
    if (mode_ == CameraMode::Orbital) {
        updateViewOrbital();
    } else {
        updateViewFreeFlight();
    }
}

void Camera::updateViewOrbital() {
    Vec3 eye = getPosition();
    Vec3 center = {target_[0], target_[1], target_[2]};
    Vec3 up = {0.0f, 1.0f, 0.0f};

    // LookAt Matrix
    Vec3 f = math::normalize(center - eye); // Forward
    Vec3 r = math::normalize(math::cross(f, up));     // Right
    Vec3 u = math::cross(r, f);                       // Up

    std::fill(view_.begin(), view_.end(), 0.0f);
    view_[0] = r.x;  view_[4] = r.y;  view_[8] = r.z;  view_[12] = -math::dot(r, eye);
    view_[1] = u.x;  view_[5] = u.y;  view_[9] = u.z;  view_[13] = -math::dot(u, eye);
    view_[2] = -f.x; view_[6] = -f.y; view_[10] = -f.z; view_[14] = math::dot(f, eye);
    view_[3] = 0.0f; view_[7] = 0.0f; view_[11] = 0.0f; view_[15] = 1.0f;

    dirtyView_ = false;
}

void Camera::updateViewFreeFlight() {
    // Build view matrix from position and direction vectors
    // forward_, right_, up_ are already world-space unit vectors
    float rollRad = roll_ * 3.14159f / 180.0f;
    float cosR = std::cos(rollRad);
    float sinR = std::sin(rollRad);

    math::Vec3 rolledRight = { right_.x * cosR + up_.x * sinR,
                               right_.y * cosR + up_.y * sinR,
                               right_.z * cosR + up_.z * sinR };
    math::Vec3 rolledUp = { up_.x * cosR - right_.x * sinR,
                            up_.y * cosR - right_.y * sinR,
                            up_.z * cosR - right_.z * sinR };
    
    std::fill(view_.begin(), view_.end(), 0.0f);
    view_[0] = rolledRight.x;  view_[4] = rolledRight.y;  view_[8] = rolledRight.z;   view_[12] = -math::dot(rolledRight, position_);
    view_[1] = rolledUp.x;     view_[5] = rolledUp.y;     view_[9] = rolledUp.z;      view_[13] = -math::dot(rolledUp, position_);
    view_[2] = -forward_.x; view_[6] = -forward_.y; view_[10] = -forward_.z; view_[14] = math::dot(forward_, position_);
    view_[3] = 0.0f;      view_[7] = 0.0f;      view_[11] = 0.0f;      view_[15] = 1.0f;

    dirtyView_ = false;
}

void Camera::updateProj() {
    float const tanHalfFovy = std::tan(fov_ / 2.0f);

    std::fill(proj_.begin(), proj_.end(), 0.0f);
    proj_[0] = 1.0f / (aspect_ * tanHalfFovy);
    proj_[5] = 1.0f / (tanHalfFovy);
    proj_[10] = farZ_ / (nearZ_ - farZ_);
    proj_[11] = -1.0f;
    proj_[14] = -(farZ_ * nearZ_) / (farZ_ - nearZ_);
    
    proj_[5] *= -1.0f; // Vulkan Y-flip
    
    dirtyProj_ = false;
}

Vec3 Camera::getRayDirection(float screenX, float screenY, float screenW, float screenH) {
    // 1. NDC
    float ndcX = (2.0f * screenX / screenW) - 1.0f;
    float ndcY = (2.0f * screenY / screenH) - 1.0f; 

    // 2. View Space
    // Unproject using TanHalfFov logic (Inverse Projection)
    // Proj[5] = -1/tanHalf.
    // So tanHalf = -1/Proj[5].
    // Note: dirtyProj_ might be true, so call updateProj if needed.
    if (dirtyProj_) updateProj();
    if (dirtyView_) updateView(); // Ensure matrices are up to date (specifically View for vectors)

    // Reconstruct basis directly from View matrix columns (which are Right, Up, Back)
    Vec3 right = {view_[0], view_[4], view_[8]};
    Vec3 up    = {view_[1], view_[5], view_[9]};
    Vec3 back  = {view_[2], view_[6], view_[10]}; // Back = -Forward
    Vec3 forward = back * -1.0f;

    // View Space Coordinates
    // Px = ndcX / P[0]
    // Py = ndcY / P[5]
    // Pz = -1.0
    float P0 = proj_[0];
    float P5 = proj_[5];
    
    float vx = ndcX / P0;
    float vy = ndcY / P5;

    // Transform View -> World
    // Ray = vx * Right + vy * Up + vz * Back (Wait, View matrix transforms World->View)
    // Inverse View transforms View->World.
    // Inverse View Rotation = Transpose(View Rotation).
    // Transpose Rows are:
    // Right (r.x, r.y, r.z)
    // Up    (u.x, u.y, u.z)
    // Back  (-f.x, -f.y, -f.z) (Assuming View matrix Col 2 is -Forward)
    
    // In updateView:
    // view_[0] = r.x...
    // view_[1] = u.x...
    // view_[2] = -f.x...
    // So Col 0 is Right. Col 1 is Up. Col 2 is -Forward (Back).
    
    // World Dir = vx * Right + vy * Up + (-1)*Back = vx*Right + vy*Up + Forward.
    
    Vec3 rayDir = (right * vx) + (up * vy) + forward; 
    return math::normalize(rayDir);
}

void Camera::addRoll(float degrees) {
    roll_ = std::clamp(roll_ + degrees, -75.0f, 75.0f);
    dirtyView_ = true;
}

void Camera::setFovDegrees(float degrees) {
    float clamped = std::clamp(degrees, 45.0f, 110.0f);
    fov_ = clamped * 3.14159f / 180.0f;
    dirtyProj_ = true;
}

float Camera::getFovDegrees() const {
    return fov_ * 180.0f / 3.14159f;
}

void Camera::applyGravity(float dt) {
    if (mode_ != CameraMode::FreeFlight) return;
    if (flying_) return; // No gravity when flying

    // Apply gravity to vertical velocity
    velocity_.y -= gravity_ * dt;

    // Apply velocity to position
    position_ = position_ + (velocity_ * dt);
    
    // Clamp falling speed
    if (velocity_.y < -50.0f) {
        velocity_.y = -50.0f;
    }

    dirtyView_ = true;
}

void Camera::checkTerrainCollision(VoxelTerrain& terrain) {
    if (mode_ != CameraMode::FreeFlight) return;
    if (flying_) return; // Noclip when flying

    // Player collision box (simple AABB around camera position)
    float feetY = position_.y - playerHeight_;
    float headY = position_.y;

    // Check ground collision
    onGround_ = false;
    
    // Sample blocks around player feet
    int checkX = static_cast<int>(std::floor(position_.x));
    int checkZ = static_cast<int>(std::floor(position_.z));
    int checkY = static_cast<int>(std::floor(feetY));

    // Check blocks in a small radius for ground contact
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            Block* block = terrain.getBlock(checkX + dx, checkY, checkZ + dz);
            if (block && block->isSolid()) {
                // Block at feet level - check if we're overlapping
                float blockTop = static_cast<float>(checkY) + 1.0f;
                if (feetY < blockTop && feetY > blockTop - 0.1f) {
                    // Landing on top of block
                    position_.y = blockTop + playerHeight_;
                    velocity_.y = 0.0f;
                    onGround_ = true;
                    dirtyView_ = true;
                }
            }
        }
    }

    // Head collision (bumping ceiling)
    int headCheckY = static_cast<int>(std::floor(headY));
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            Block* block = terrain.getBlock(checkX + dx, headCheckY + 1, checkZ + dz);
            if (block && block->isSolid()) {
                if (velocity_.y > 0) {
                    velocity_.y = 0.0f;
                }
            }
        }
    }

    // Horizontal collision (walls)
    // Sample ahead in movement direction
    math::Vec3 futurePos = position_ + (velocity_ * 0.1f);
    int futureX = static_cast<int>(std::floor(futurePos.x));
    int futureZ = static_cast<int>(std::floor(futurePos.z));
    
    for (int dy = 0; dy < 2; dy++) {  // Check at feet and chest height
        float dyOffset = static_cast<float>(dy) * 0.9f;
        int yCheck = static_cast<int>(std::floor(position_.y - playerHeight_ + dyOffset));
        Block* block = terrain.getBlock(futureX, yCheck, futureZ);
        if (block && block->isSolid()) {
            // Blocked - prevent horizontal movement
            position_.x -= velocity_.x * 0.1f;
            position_.z -= velocity_.z * 0.1f;
            velocity_.x *= 0.5f;  // Dampen
            velocity_.z *= 0.5f;
            dirtyView_ = true;
        }
    }

    // Prevent falling through world
    if (position_.y < 0) {
        position_.y = playerHeight_;
        velocity_.y = 0.0f;
        onGround_ = true;
        dirtyView_ = true;
    }
}

void Camera::jump() {
    if (mode_ != CameraMode::FreeFlight) return;
    if (flying_) return; // No jumping when flying (use E/Q)
    if (!onGround_) return;  // Can only jump when on ground

    velocity_.y = jumpSpeed_;
    onGround_ = false;
}

} // namespace graphics
