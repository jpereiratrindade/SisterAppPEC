#version 450

//---------------------------------------------
// Inputs
//---------------------------------------------
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

//---------------------------------------------
// Push Constants (deve ser igual ao vertex shader)
//---------------------------------------------
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
    float useLighting;
    float useFixedColor;
    float opacity;
    vec3 fixedColor;
    vec3 cameraPos;
} pc;

//---------------------------------------------
// Main
//---------------------------------------------
void main() {

    //----------------------------------------------------
    // 1. Base color
    //----------------------------------------------------
    vec3 baseColor = pc.useFixedColor > 0.5 ? pc.fixedColor : fragColor;
    vec3 color = baseColor;

    //----------------------------------------------------
    // 2. Optional directional lighting
    //----------------------------------------------------
    if (pc.useLighting > 0.5) {
        vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));
        vec3 normal = normalize(fragNormal);

        float diff = max(dot(normal, lightDir), 0.0);
        vec3 sunColor = vec3(1.0, 0.95, 0.85);

        float ambient = 0.45;
        float diffuse = 0.65;

        vec3 lighting = (ambient + diffuse * diff) * sunColor;
        color *= lighting;
    }

    //----------------------------------------------------
    // 3. Fog (based on real camera position)
    //----------------------------------------------------
    // Push horizon further
    float fogStart = 150.0;
    float fogEnd   = 450.0;

    float dist = length(fragWorldPos - pc.cameraPos);
    float fogFactor = smoothstep(fogStart, fogEnd, dist);

    //----------------------------------------------------
    // 4. Sky Gradient
    //----------------------------------------------------
    vec3 skyBottom = vec3(0.75, 0.85, 1.0);
    vec3 skyTop    = vec3(0.25, 0.45, 0.85);
    float skyMix = clamp((fragWorldPos.y + 30.0) / 120.0, 0.0, 1.0);

    vec3 skyColor = mix(skyBottom, skyTop, skyMix);

    //----------------------------------------------------
    // 5. Horizon + procedural "mountain layers"
    //----------------------------------------------------
    float h = fragWorldPos.z * 0.02 + sin(fragWorldPos.x * 0.05) * 1.5;

    float layer1 = smoothstep(h - 1.0, h + 1.0, fragWorldPos.y);
    vec3 layer1Color = vec3(0.5, 0.7, 0.9);

    float layer2 = smoothstep(h - 0.5, h + 0.5, fragWorldPos.y);
    vec3 layer2Color = vec3(0.4, 0.6, 0.85);

    skyColor = mix(skyColor, layer1Color, layer1 * 0.4);
    skyColor = mix(skyColor, layer2Color, layer2 * 0.3);

    //----------------------------------------------------
    // 6. Reference Pillars (navigation depth cues)
    //----------------------------------------------------
    float spacing = 50.0;

    float px = abs(fract(fragWorldPos.x / spacing) - 0.5);
    float pz = abs(fract(fragWorldPos.z / spacing) - 0.5);

    float pillar = smoothstep(0.02, 0.005, min(px, pz));

    // fade pillars with fog
    pillar *= (1.0 - fogFactor);

    vec3 pillarColor = vec3(0.8, 0.9, 1.0);
    color = mix(color, pillarColor, pillar * 0.5);

    //----------------------------------------------------
    // 8. Apply fog mixing with sky
    //----------------------------------------------------
    color = mix(color, skyColor, fogFactor);

    //----------------------------------------------------
    // 9. Final output
    //----------------------------------------------------
    outColor = vec4(color, pc.opacity);
}
