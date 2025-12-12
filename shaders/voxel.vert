#version 450

// ----------------------------------------
// Inputs
// ----------------------------------------
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

// ----------------------------------------
// Outputs to Fragment Shader
// ----------------------------------------
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

// ----------------------------------------
// Push Constants (must match frag shader!)
// ----------------------------------------
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
    float useLighting;
    float useFixedColor;
    float opacity;
    vec3 fixedColor;
    vec3 cameraPos;
} pc;

// ----------------------------------------
// Main
// ----------------------------------------
void main() {

    // Clip-space transform
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    // Point size for point clouds / voxels
    gl_PointSize = pc.pointSize;

    // Pass-through attributes
    fragColor    = inColor;
    fragNormal   = normalize(inNormal);
    fragWorldPos = inPosition;  // world-space position (as required)
}
