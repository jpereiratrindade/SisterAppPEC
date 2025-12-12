#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
    float useLighting;   // 0.0 or 1.0
    float useFixedColor; // 0.0 or 1.0
    float opacity;       // Alpha
    vec3  fixedColor;    // Color Override
} pc;

void main() {
    vec3 color = fragColor;
    if (pc.useFixedColor > 0.5) {
        color = pc.fixedColor;
    }

    if (pc.useLighting > 0.5) {
        vec3 norm = normalize(fragNormal);
        // Fixed light direction (from top-front-right)
        vec3 lightDir = normalize(vec3(0.5, 1.0, 1.0)); 
        // Diffuse factor (Lambert)
        float diff = max(dot(norm, lightDir), 0.0);
        
        // Ambient + Diffuse
        vec3 ambient = vec3(0.3);
        vec3 diffuse = diff * vec3(0.7);
        color = color * (ambient + diffuse);
    }

    outColor = vec4(color, pc.opacity);
}
