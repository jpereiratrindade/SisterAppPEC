#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV; // v3.6.1

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out float fragViewDist;
layout(location = 3) out vec2 fragUV; // Pass to frag

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    gl_PointSize = pc.pointSize;
    fragColor = inColor;
    fragNormal = inNormal;
    fragUV = inUV;
    
    // Pass View Depth (z-distance) roughly or just distance from camera
    // Since MVP is combined, we don't have View Space easily without separating matrices.
    // However, for simple fog, gl_Position.w is roughly the depth/distance.
    // Or we can pass World Pos if we had Model Matrix.
    // For now, let's use gl_Position.z / gl_Position.w as a depth approximation or just w.
    fragViewDist = gl_Position.w; 
}
