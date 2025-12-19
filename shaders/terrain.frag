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

layout(binding = 0) uniform sampler2D vegetationMap;

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

// --- DDD Vegetation Logic ---
vec3 getRealisticLook(vec4 bioData) {
    float baseGreenness = bioData.r;       // EI Coverage
    float structuralRoughness = bioData.g; // ES Coverage
    float vitality = bioData.b;            // Vigor (Simplification: using B for both EI/ES vigor mix or Primary Vigor)
    // Note: C++ sends: R=EI, G=ES, B=VigorEI, A=VigorES. 
    // DDD expects B=Stress. We will map Vigor -> Stress (1.0 - Vigor) or use Vigor directly.
    
    // Using Vigor directly as "Vitality"
    
    // Paleta Campestre
    vec3 cSoil  = vec3(0.55, 0.45, 0.35); // Solo exposto
    vec3 cDry   = vec3(0.70, 0.60, 0.40); // Palha (low vigor)
    vec3 cGreen = vec3(0.25, 0.55, 0.15); // Grama viva (high vigor)
    vec3 cShrub = vec3(0.10, 0.30, 0.10); // Arbusto

    // 1. Estado da Grama (Vitalidade)
    vec3 grassState = mix(cDry, cGreen, vitality);

    // 2. Base (Solo -> Grama)
    vec3 baseLayer = mix(cSoil, grassState, smoothstep(0.1, 0.9, baseGreenness));

    // 3. Estrutura (Arbustos)
    vec3 structuralLayer = mix(baseLayer, cShrub, smoothstep(0.1, 0.8, structuralRoughness));

    return structuralLayer;
}

// Scientific NDVI Palette (Standardized)
vec3 getNDVILook(vec4 bioData) {
    float ei = bioData.r;
    float es = bioData.g;
    float vigor = bioData.b;

    // 1. Calculate Synthetic NDVI (-1.0 to 1.0)
    // We start from 0.0 (Soil) to 1.0 (Dense Healthy Veg)
    float biomass = clamp(ei * 0.7 + es * 0.3, 0.0, 1.0);
    float ndvi = biomass * vigor; 
    
    // Remap to Scientific Range
    // Soil Base: 0.1
    // Full Veg: 0.9
    ndvi = 0.1 + (ndvi * 0.8); 

    // Palette Ramps
    vec3 cWater = vec3(0.0, 0.0, 1.0);  // -1.0 to 0.0
    vec3 cSoil  = vec3(0.6, 0.5, 0.4);  // 0.0 to 0.2
    vec3 cSparse= vec3(0.9, 0.9, 0.5);  // 0.2 to 0.4
    vec3 cDense = vec3(0.0, 0.6, 0.0);  // 0.4 to 0.8
    vec3 cRich  = vec3(0.0, 0.3, 0.0);  // 0.8 to 1.0

    // Gradient Logic
    if (ndvi < 0.2) {
       float t = ndvi / 0.2;
       return mix(cSoil, cSparse, t);
    } 
    else if (ndvi < 0.4) {
       float t = (ndvi - 0.2) / 0.2;
       return mix(cSparse, cDense, t);
    }
    else {
       float t = clamp((ndvi - 0.4) / 0.5, 0.0, 1.0); 
       return mix(cDense, cRich, t);
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
        // ... (Existing Watershed Logic) ...
        int bid = int(round(fragAux));
        if (bid > 0) {
            float r = hash(vec2(float(bid), 12.34));
            float g = hash(vec2(float(bid), 56.78));
            float b = hash(vec2(float(bid), 90.12));
            color = mix(color, vec3(r,g,b), 0.6);
        }
    }
    
    // v3.6.4: Basin Outlines (Uses Smooth interpolated ID to detect edges)
    if (useBasinOutlines) {
        float edge = fwidth(fragAuxSmooth);
        // If derivative is high, we are on a transition between basins
        if (edge > 0.1) {
            color = vec3(1.0); // White contours
        }
    }

    // 5. Vegetation Visualization (v3.9.0 DDD Implementation)
    // Decode Mode from high bits (16-19)
    uint vegMode = (pc.flags >> 16) & 0xFu; 
    
    // uvScale passed in sunDir.w
    float uvScale = pc.sunDir.w; 
    
    if (vegMode > 0u && uvScale > 0.0) {
        vec2 texUV = fragWorldPos.xz * uvScale;
        
        // Sample Vegetation
        // Engine C++ Data: R=EI_Cov, G=ES_Cov, B=EI_Vigor, A=ES_Vigor
        vec4 rawData = texture(vegetationMap, texUV);
        
        // Adapter for DDD Functions
        vec4 bioData;
        bioData.r = rawData.r; // Base Greenness
        bioData.g = rawData.g; // Structural Roughness
        bioData.b = rawData.b; // Vitality (Using EI Vigor as primary)
        bioData.a = rawData.a; // Info
        
        if (vegMode == 1u) { // Realistic (DDD)
             color = getRealisticLook(bioData);
        } 
        else if (vegMode == 2u) { // Heatmap EI (Cover)
             color = mix(vec3(0.0), vec3(0.0, 1.0, 0.0), rawData.r);
        } 
        else if (vegMode == 3u) { // Heatmap ES (Cover)
             color = mix(vec3(0.0), vec3(1.0, 0.0, 0.0), rawData.g);
        } 
        else if (vegMode == 4u) { // Vigor Map / NDVI Style
             color = getNDVILook(bioData);
        }
    }

    // 1. Lighting (Applied to Albedo)
    vec3 ambient = vec3(0.0);
    vec3 diffuse = vec3(0.0);

    float lightMultiplier = pc.params.w; // v3.8.1 Light Intensity Control

    if (useLighting) {
        vec3 norm = normalize(fragNormal);
        // Use user-defined sun direction (Safe Normalize, but pre-normalized in CPU)
        vec3 lightDir = normalize(pc.sunDir.xyz); 
        
        // Diffuse (Sun)
        float diff = max(dot(norm, lightDir), 0.0);
       
        vec3 sunColor = vec3(1.0, 0.95, 0.8);
        diffuse = diff * sunColor * 0.8 * lightMultiplier; // Scaled by user intensity
         
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
    
    // V3.8.1 Fix: Revert to Medium Blue (User Preference)
    vec3 skyColor = vec3(0.53, 0.81, 0.92); 
    // vec3 skyColor = vec3(0.75, 0.85, 1.0); // Matched Horizon (Disabled on user request)
    vec3 finalColor = mix(color, skyColor, fogFactor);

    // Final Clamp (Defensive)
    outColor = vec4(clamp(finalColor, 0.0, 1.0), opacity);
}
