#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragViewDist;
layout(location = 3) in vec2 fragUV; // v3.6.1 Flux (x), Sediment (y)
layout(location = 4) flat in float fragAux; // v3.6.3 Basin ID
layout(location = 5) in float fragAuxSmooth; // v3.6.4 Smooth ID
layout(location = 6) in vec3 fragWorldPos;   // v3.6.4 World Pos
layout(location = 7) flat in float fragSoilId; // v3.7.3

layout(location = 0) out vec4 outColor;

// Unified PushConstants (Packed for <128 bytes)
layout(push_constant) uniform PushConstants {
    layout(offset = 0) mat4 mvp;         // 64 bytes
    layout(offset = 64) vec4 sunDir;     // 16 bytes (Normalized)
    layout(offset = 80) vec4 fixedColor; // 16 bytes
    layout(offset = 96) vec4 params;     // 16 bytes [opacity, drainageInt, fogDens, pointSize]
    layout(offset = 112) uint flags;     // 4 bytes (Bitmask)
} pc;

// Flags Decode Helper
bool hasFlag(uint bit) {
    return (pc.flags & bit) != 0;
}

// Pseudo-random noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Value Noise for Coherent Patches (v3.7.2)
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Soil Color Lookup (v3.7.3 Semantic)
vec3 getSoilColor(int id) {
    // Match SoilType enum
    // 0=None, 1=Hidro, 2=BText, 3=Argila, 4=BemDes, 5=Raso, 6=Rocha
    switch(id) {
        case 1: return vec3(0.0, 0.3, 0.3); // Hidromorfico
        case 2: return vec3(0.7, 0.35, 0.05); // BTextural
        case 3: return vec3(0.4, 0.0, 0.5); // Argila Expansiva
        case 4: return vec3(0.5, 0.15, 0.1); // Bem Desenvolvido
        case 5: return vec3(0.7, 0.7, 0.2); // Solo Raso
        case 6: return vec3(0.2, 0.2, 0.2); // Rocha
        default: return vec3(0.1); // Error/None
    }
}

void main() {
    vec3 color = fragColor;
    bool isNatural = true;
    
    // Decode Params
    float opacity = pc.params.x;
    float drainageIntensity = pc.params.y;
    float fogDensity = pc.params.z;
    // float pointSize = pc.params.w;
    
    // Decode Flags
    bool useLighting = hasFlag(1u);
    bool useFixedColor = hasFlag(2u);
    bool useSlopeVis = hasFlag(4u);
    bool useDrainageVis = hasFlag(8u);
    bool useErosionVis = hasFlag(16u);
    bool useWatershedVis = hasFlag(32u);
    bool useSoilVis = hasFlag(64u);
    bool useBasinOutlines = hasFlag(128u);

    // 0. Base Color (Albedo)
    if (useFixedColor) {
        color = pc.fixedColor.rgb;
    } 
    else if (useSoilVis) {
        isNatural = false;
        
        int soilType = int(round(fragSoilId));
        
        // Soil Whitelist Flags
        bool allowHidro   = hasFlag(256u);
        bool allowBText   = hasFlag(512u);
        bool allowArgila  = hasFlag(1024u);
        bool allowBemDes  = hasFlag(2048u);
        bool allowRaso    = hasFlag(4096u);
        bool allowRocha   = hasFlag(8192u);

        // Check if allowed
        bool visible = false;
        if (soilType == 1 && allowHidro) visible = true;
        else if (soilType == 2 && allowBText) visible = true;
        else if (soilType == 3 && allowArgila) visible = true;
        else if (soilType == 4 && allowBemDes) visible = true;
        else if (soilType == 5 && allowRaso) visible = true;
        else if (soilType == 6 && allowRocha) visible = true;
        
        if (visible) {
            color = getSoilColor(soilType);
        } else {
             color = vec3(0.1); // Gray/Hidden
        }
    }
    else if (useSlopeVis) {
        isNatural = false;
        // --- Slope Visualization Analysis (Standardized Classes) ---
        vec3 N = normalize(fragNormal);
        float dotUp = clamp(N.y, 0.0, 1.0);
        float angle = acos(dotUp);
        float slopePercent = tan(angle) * 100.0;
        
        // Classes (Keyframes):
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
        // Natural Terrain Color (No Noise) -> NOW WITH NOISE
        // Add subtle high-freq noise to simulate sand/soil texture
        float n = noise(fragWorldPos.xz * 1.0); // High freq
        color += (n - 0.5) * 0.1; // +/- 0.05 intensity
    }
    
    // 2. Drainage / Flux Overlay
    if (useDrainageVis) {
        float flux = fragUV.x;
        if (flux > 1.0) { 
            float flow = log(flux) * drainageIntensity;
            flow = min(flow, 1.0);
            vec3 waterColor = vec3(0.0, 0.8, 1.0);
            color = mix(color, waterColor, flow * 0.8);
        }
    }
    
    // 3. Erosion / Sediment Overlay
    if (useErosionVis) {
        float sediment = fragUV.y;
        if (sediment > 0.05) { // Sensitivity
            float intensity = min(sediment * 2.0, 1.0);
            vec3 erosionColor = vec3(0.6, 0.2, 0.1); // Brownish-Red
            color = mix(color, erosionColor, intensity * 0.7);
        }
    }

    // 4. Watershed Visualization
    if (useWatershedVis) {
        int bid = int(round(fragAux));
        if (bid > 0) {
            // Generate distinctive color for each basin
            float r = hash(vec2(float(bid), 12.34));
            float g = hash(vec2(float(bid), 56.78));
            float b = hash(vec2(float(bid), 90.12));
            vec3 basinColor = vec3(r, g, b);
            // Ensure some brightness
            if (length(basinColor) < 0.5) basinColor += 0.5;
            color = mix(color, basinColor, 0.6); // Tint
            
            // Outline (v3.6.4)
            if (useBasinOutlines) {
                // Check if we are in a transition zone (interpolated ID varies)
                float delta = fwidth(fragAuxSmooth);
                if (delta > 0.0001) { // Lower threshold to catch shallow angles
                    // We are in a triangle connecting different IDs.
                    // To narrow the line, we only draw near the grid cell edge (integer + 0.5)
                    // Calculate distance to nearest 0.5 coordinate offset
                    vec2 gridDist = abs(fract(fragWorldPos.xz) - 0.5);
                    // Use gradients to get screen-space/pixel-width aa
                    vec2 width = fwidth(fragWorldPos.xz);
                    // Standard AA grid line logic - Softer falloff (Safe smoothstep)
                    // We want 1.0 at dist=0, and 0.0 at dist=width*2.5
                    vec2 gridAA = 1.0 - smoothstep(vec2(0.0), width * 2.5, gridDist);
                    float lineStr = max(gridAA.x, gridAA.y);
                    
                    if (lineStr > 0.0) {
                        // Apply Dark Line (Softer)
                         color = mix(color, vec3(0.0), lineStr * 0.5);
                    }
                }
            }
        } else {
             // Gray out outside areas
             float lum = dot(color, vec3(0.299, 0.587, 0.114));
             color = vec3(lum) * 0.5;
        }
    }

    // 1. Lighting (Applied to Albedo)
    vec3 ambient = vec3(0.0);
    vec3 diffuse = vec3(0.0);

    if (useLighting) {
        vec3 norm = normalize(fragNormal);
        // Use user-defined sun direction (Safe Normalize, but pre-normalized in CPU)
        vec3 lightDir = normalize(pc.sunDir.xyz); 
        
        // Diffuse (Sun)
        float diff = max(dot(norm, lightDir), 0.0);
       
        vec3 sunColor = vec3(1.0, 0.95, 0.8);
        diffuse = diff * sunColor * 0.8;
        
        // Hemispheric Ambient (Sky Blue -> Ground Brown)
        float hemi = (norm.y * 0.5 + 0.5); 
        vec3 skyAmbient = vec3(0.53, 0.81, 0.92) * 0.6; // Improved ambient
        vec3 gndAmbient = vec3(0.2, 0.15, 0.1) * 0.4;
        ambient = mix(gndAmbient, skyAmbient, hemi);
        
        // Fake AO based on slope
        float ao = clamp(norm.y, 0.2, 1.0);
        
        // Optimization: Reduce Lighting intensity for data maps to keep colors true?
        // User requested: "Controle de luz" available.
        // We can slightly desaturate lighting for data maps vs "Natural".
        if (!isNatural) {
          diffuse *= 0.7; // Softer shadows on data
          ambient = mix(ambient, vec3(1.0), 0.3); // More uniform ambient
        }

        color = color * (ambient + diffuse) * ao;
    }

    // 2. Distance Fog
    // Improved Fog Curve: 1.0 - exp(-pow(dist * density, 1.2)) (Gives clearer near field, sharper falloff)
    float fogFactor = 1.0 - exp(-pow(fragViewDist * fogDensity, 1.2)); 
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    vec3 skyColor = vec3(0.53, 0.81, 0.92); // Matches Clear Color
    vec3 finalColor = mix(color, skyColor, fogFactor);

    // Final Clamp (Defensive)
    outColor = vec4(clamp(finalColor, 0.0, 1.0), opacity);
}
