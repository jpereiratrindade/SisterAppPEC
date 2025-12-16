#pragma once

#include <SDL2/SDL.h>
#include <array>
#include <cmath>
#include <algorithm> // v3.8.0 Fix: for std::clamp
#include "../math/math_types.h"

namespace graphics {

// Forward declaration
class VoxelTerrain;

enum class CameraMode {
    Orbital,     // Orbit around a target (existing mode)
    FreeFlight   // Free movement like FPS game
};

class Camera {
public:
    Camera(float fov, float aspect, float nearZ, float farZ);

    void update(float dt);
    void processEvent(const SDL_Event& event);
    void processKeyboard(const Uint8* keyState, float dt); // NEW: WASD movement

    void setAspect(float aspect) { aspect_ = aspect; dirtyProj_ = true; }

    // Returns pointer to 16 floats (column-major)
    const float* viewMatrix();
    const float* projectionMatrix();

    // Helpers for copy
    void getViewMatrix(float* out) {
        const float* m = viewMatrix();
        for(int i=0; i<16; ++i) out[i] = m[i];
    }
    void getProjectionMatrix(float* out) {
        const float* m = projectionMatrix();
        for(int i=0; i<16; ++i) out[i] = m[i];
    }
    
    // Core transforms
    void setDistance(float d) { radius_ = d; dirtyView_ = true; }
    void reset(); // Resets to initial view

    // Camera mode control
    void setCameraMode(CameraMode mode);
    CameraMode getCameraMode() const { return mode_; }
    
    // Flight control
    void setFlying(bool flying) { flying_ = flying; velocity_ = {0,0,0}; }
    bool isFlying() const { return flying_; }

    // Roll / tilt control (free flight)
    void addRoll(float degrees);
    float getRollDegrees() const { return roll_; }
    void resetRoll() { roll_ = 0.0f; dirtyView_ = true; }
    
    void setTarget(const math::Vec3& target) { target_ = {target.x, target.y, target.z}; dirtyView_ = true; }
    void setMoveSpeed(float speed) { moveSpeed_ = speed; }
    float getMoveSpeed() const { return moveSpeed_; }

    // FOV helpers
    void setFovDegrees(float degrees);
    float getFovDegrees() const;
    void setFarClip(float farZ) { farZ_ = farZ; dirtyProj_ = true; } // v3.8.1 Dynamic Draw Distance
    
    // Teleport (instant position change)
    void teleportTo(const math::Vec3& pos);

    math::Vec3 getPosition() const;
    math::Vec3 getRayDirection(float screenX, float screenY, float screenW, float screenH);
    
    // v3.8.0 Minimap support
    float getYaw() const { return yaw_; }
    void setYaw(float yaw) { yaw_ = yaw; dirtyView_ = true; }
    void setPitch(float pitch) { pitch_ = std::clamp(pitch, -89.0f, 89.0f); dirtyView_ = true; }

    // Player physics (for voxel terrain)
    void applyGravity(float dt);
    void checkTerrainCollision(VoxelTerrain& terrain);
    void jump();
    bool isOnGround() const { return onGround_; }

private:
    void updateView();
    void updateProj();
    void updateViewOrbital();    // Orbital camera view
    void updateViewFreeFlight(); // Free flight camera view

    // Current mode
    CameraMode mode_ = CameraMode::Orbital;
    bool flying_ = false; // Creative/Spectator flight (no gravity)

    // Orbit parameters (for Orbital mode)
    float theta_ = 0.0f; // Horizontal angle
    float phi_ = 0.0f;   // Vertical angle
    float radius_ = 5.0f; 
    std::array<float, 3> target_{0.0f, 0.0f, 0.0f};

    // Free Flight parameters
    math::Vec3 position_{0.0f, 2.0f, 5.0f}; // Eye position
    float yaw_ = -90.0f;    // Horizontal rotation (degrees)
    float pitch_ = 0.0f;    // Vertical rotation (degrees)
    math::Vec3 forward_{0.0f, 0.0f, -1.0f};
    math::Vec3 right_{1.0f, 0.0f, 0.0f};
    math::Vec3 up_{0.0f, 1.0f, 0.0f};
    float moveSpeed_ = 10.0f;      // units per second (faster for voxel world)
    float mouseSensitivity_ = 0.1f; // degrees per pixel
    float roll_ = 0.0f;             // Camera roll (degrees)

    // Player physics
    math::Vec3 velocity_{0.0f, 0.0f, 0.0f};  // Current velocity
    bool onGround_ = false;                   // Is player standing on ground?
    float gravity_ = 20.0f;                   // Gravity acceleration (m/s²)
    float jumpSpeed_ = 8.0f;                  // Initial jump velocity
    float playerHeight_ = 1.8f;               // Player eye height

    // Projection params
    float fov_;
    float aspect_;
    float nearZ_;
    float farZ_;

    // Matrices (16 floats)
    std::array<float, 16> view_;
    std::array<float, 16> proj_;
    bool dirtyView_ = true;
    bool dirtyProj_ = true;

    // Input state
    bool isDragging_ = false;
    int lastX_ = 0;
    int lastY_ = 0;
};

} // namespace graphics
