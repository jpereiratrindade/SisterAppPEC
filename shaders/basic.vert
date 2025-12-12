#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    gl_PointSize = pc.pointSize;
    fragColor = inColor;
    fragNormal = inNormal;
}
