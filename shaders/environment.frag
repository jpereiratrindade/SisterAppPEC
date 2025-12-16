#version 450

//----------------------------------------------------
// Inputs do vertex shader
//----------------------------------------------------
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragNormal;

//----------------------------------------------------
// Saída
//----------------------------------------------------
layout(location = 0) out vec4 outColor;

//----------------------------------------------------
// Push constants (APENAS mvp, mantendo compatibilidade)
//----------------------------------------------------
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

//----------------------------------------------------
// Funções auxiliares
//----------------------------------------------------
vec3 skyGradient(float y) {
    // Céu gradiente: inferior → azul claro | superior → azul profundo
    vec3 bottom = vec3(0.75, 0.85, 1.0);
    vec3 top    = vec3(0.25, 0.45, 0.85);

    // V3.8.1 Fix: Gradient must span the large sphere radius (4000.0)
    // Previous (y+40)/120 was for a tiny box. Now we use a broader range.
    // Map Y from -1000 (Horizon/Bottom) to +3000 (Zenith)
    float t = clamp((y + 500.0) / 3500.0, 0.0, 1.0);
    return mix(bottom, top, t);
}

vec3 horizonBand(vec3 baseSky, float y, float z) {
    // Linha de horizonte suave, levemente ondulada
    float h = z * 0.02 + sin(z * 0.01) * 1.5;
    float d = smoothstep(h - 1.0, h + 1.0, y);
    vec3 hazeColor = vec3(0.85, 0.9, 0.95);
    return mix(baseSky, hazeColor, d * 0.35);
}

//----------------------------------------------------
// MAIN
//----------------------------------------------------
void main() {

    //------------------------------------------------
    // 1) Céu básico
    //------------------------------------------------
    vec3 color = skyGradient(fragWorldPos.y);

    //------------------------------------------------
    // 2) Horizonte procedural leve
    //------------------------------------------------
    color = horizonBand(color, fragWorldPos.y, fragWorldPos.z);

    //------------------------------------------------
    // 3) Iluminação simples (do topo)
    //------------------------------------------------
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(0.6, 1.0, 0.4));

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 ambient = vec3(0.35);
    vec3 lightColor = vec3(0.9, 0.95, 1.0);

    vec3 lighting = ambient + diff * lightColor * 0.6;

    // Aplique iluminação somente se os elementos tiverem cor
    vec3 surface = fragColor * lighting;

    //------------------------------------------------
    // 4) Mistura do ambiente com a geometria
    //------------------------------------------------
    // Geometria deve ser visível; se não houver geometria, vira céu completo.
    float hasSurface = length(fragColor);
    hasSurface = clamp(hasSurface, 0.0, 1.0);

    vec3 finalColor = mix(color, surface, hasSurface);

    //------------------------------------------------
    // 5) Fog atmosférico simples
    //------------------------------------------------
    float dist = length(fragWorldPos);
    float fogDensity = 0.007;
    float fogFactor = exp(-fogDensity * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 fogColor = color; // fog usa o próprio céu
    finalColor = mix(fogColor, finalColor, fogFactor);

    outColor = vec4(finalColor, 1.0);
}
