#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragViewDist;
layout(location = 3) in vec2 fragUV; // v3.6.1 Flux

layout(location = 0) out vec4 outColor;

// Unified PushConstants (Matches src/renderer.cpp + custom fields)
layout(push_constant) uniform PushConstants {
    mat4 mvp;               // Offset 0
    float pointSize;        // Offset 64
    float useLighting;      // Offset 68
    float useFixedColor;    // Offset 72
    float opacity;          // Offset 76
    vec3  fixedColor;       // Offset 80
    float useSlopeVis;      // Offset 92 (Replaces padding)
    vec3  cameraPos;        // Offset 96
    float useDrainageVis;   // Offset 108 (New in v3.6.1)
} pc;

// Pseudo-random noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 color = fragColor;
    
    // 0. Base Color (Albedo)
    if (pc.useFixedColor > 0.5) {
        color = pc.fixedColor;
    } 
    else if (pc.useSlopeVis > 0.5) {
        // --- Slope Visualization Analysis (Standardized Classes) ---
        vec3 N = normalize(fragNormal);
        float dotUp = clamp(N.y, 0.0, 1.0);
        float angle = acos(dotUp);
        float slopePercent = tan(angle) * 100.0;
        
        // Classes (Keyframes):
        // 0%  : Blue   (0.1, 0.3, 0.8)
        // 3%  : Cyan   (0.0, 0.8, 1.0)
        // 8%  : Green  (0.4, 0.8, 0.2)
        // 20% : Steep  (1.0, 0.9, 0.1) // Yellowish
        // 45% : V.Steep(1.0, 0.5, 0.0) // Orange
        // 75% : Extreme(0.8, 0.0, 0.0) // Red

        vec3 cBlue   = vec3(0.1, 0.3, 0.8);
        vec3 cCyan   = vec3(0.0, 0.8, 1.0);
        vec3 cGreen  = vec3(0.4, 0.8, 0.2);
        vec3 cYellow = vec3(1.0, 0.9, 0.1);
        vec3 cOrange = vec3(1.0, 0.5, 0.0);
        vec3 cRed    = vec3(0.8, 0.0, 0.0);

        if (slopePercent < 3.0) {
            float t = slopePercent / 3.0;
            color = mix(cBlue, cCyan, t);
        } else if (slopePercent < 8.0) {
            float t = (slopePercent - 3.0) / (8.0 - 3.0);
            color = mix(cCyan, cGreen, t);
        } else if (slopePercent < 20.0) {
            float t = (slopePercent - 8.0) / (20.0 - 8.0);
            color = mix(cGreen, cYellow, t);
        } else if (slopePercent < 45.0) {
            float t = (slopePercent - 20.0) / (45.0 - 20.0);
            color = mix(cYellow, cOrange, t);
        } else if (slopePercent < 75.0) {
            float t = (slopePercent - 45.0) / (75.0 - 45.0);
            color = mix(cOrange, cRed, t);
        } else {
            color = cRed;
        }
    }
    else {
        // Natural Terrain Color + Noise
        float noise = hash(fragNormal.xy * 10.0 + gl_FragCoord.xy * 0.1); 
        color += (noise - 0.5) * 0.05; 
    }
    
    // 2. Drainage / Flux Overlay (v3.6.1)
    if (pc.useDrainageVis > 0.5) {
        float flux = fragUV.x;
        if (flux > 5.0) { // Threshold
            float flow = log(flux) * 0.15;
            flow = min(flow, 1.0);
            vec3 waterColor = vec3(0.0, 0.2, 0.8); // Deep Blue
            // Mix water on top of terrain (or slope analysis)
            color = mix(color, waterColor, flow);
        }
    }

    // 1. Lighting (Applied to Albedo)
    if (pc.useLighting > 0.5) {
        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // Sun from side
        
        // Diffuse (Sun)
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 sunColor = vec3(1.0, 0.95, 0.8);
        vec3 diffuse = diff * sunColor * 0.8;
        
        // Hemispheric Ambient (Sky Blue -> Ground Brown)
        float hemi = (norm.y * 0.5 + 0.5); 
        vec3 skyAmbient = vec3(0.53, 0.81, 0.92) * 0.5;
        vec3 gndAmbient = vec3(0.2, 0.15, 0.1) * 0.3;
        vec3 ambient = mix(gndAmbient, skyAmbient, hemi);
        
        // Fake AO based on slope
        float ao = clamp(norm.y, 0.2, 1.0);
        
        color = color * (ambient + diffuse) * ao;
    }

    // 2. Distance Fog
    float fogDensity = 0.0015; 
    float fogFactor = 1.0 - exp(-fragViewDist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    vec3 skyColor = vec3(0.53, 0.81, 0.92); // Matches Clear Color
    vec3 finalColor = mix(color, skyColor, fogFactor);

    outColor = vec4(finalColor, pc.opacity);
}
