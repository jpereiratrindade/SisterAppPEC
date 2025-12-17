#version 450

// =========================================================================
// INPUTS (Vindos do Vertex Shader)
// =========================================================================
layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_WorldPos;

// =========================================================================
// OUTPUT
// =========================================================================
layout(location = 0) out vec4 outColor;

// =========================================================================
// DESCRIPTOR SET 1 — DADOS DA REPRESENTAÇÃO
// =========================================================================
// Gerado pelo SemanticTranslatorService (CPU)
//
// R = BaseGreenness (EI)
// G = StructuralRoughness (ES)
// B = VisualStress
// A = Metadata (reservado)
layout(set = 1, binding = 0) uniform sampler2D u_BioDataTexture;

// Texturas visuais (representação, não entidades ecológicas)
layout(set = 1, binding = 1) uniform sampler2D u_SoilTexture;
layout(set = 1, binding = 2) uniform sampler2D u_GrassTexture;
layout(set = 1, binding = 3) uniform sampler2D u_StrawTexture;
layout(set = 1, binding = 4) uniform sampler2D u_StructureTexture;

// =========================================================================
// DESCRIPTOR SET 0 — UNIFORMS DE CONTROLE
// =========================================================================
layout(set = 0, binding = 0) uniform RenderParams {
    int visualMode; // Value Object: VisualMode
} u_Render;

// =========================================================================
// FUNÇÕES DE REPRESENTAÇÃO
// =========================================================================

vec3 getRealisticLook(
    vec4 bioData,
    vec3 soilColor,
    vec3 grassColor,
    vec3 strawColor,
    vec3 structureColor
) {
    float baseGreenness      = bioData.r; // EI
    float structuralRoughness = bioData.g; // ES
    float stressFactor       = bioData.b; // Estresse visual

    // --- Estrato Inferior (EI): base fotossintética ---
    vec3 grassState = mix(grassColor, strawColor, stressFactor);

    vec3 baseLayer = mix(
        soilColor,
        grassState,
        smoothstep(0.1, 0.9, baseGreenness)
    );

    // --- Estrato Superior (ES): estrutura (modulação) ---
    vec3 structuralLayer = mix(
        baseLayer,
        baseLayer * structureColor * 0.8,
        smoothstep(0.05, 0.8, structuralRoughness)
    );

    // Estresse atua também na estrutura (amortecido)
    return mix(
        structuralLayer,
        structuralLayer * 0.7,
        stressFactor * 0.3
    );
}

// NDVI como observável
vec3 getNDVILook(vec4 bioData) {
    float ei = bioData.r;
    float es = bioData.g;
    float stress = bioData.b;

    float ndvi = clamp(
        ei * 0.7 +
        es * 0.3 -
        stress * 0.5,
        0.0, 1.0
    );

    vec3 low  = vec3(0.8, 0.2, 0.2);
    vec3 mid  = vec3(0.9, 0.9, 0.2);
    vec3 high = vec3(0.1, 0.8, 0.1);

    if (ndvi < 0.5)
        return mix(low, mid, ndvi * 2.0);

    return mix(mid, high, (ndvi - 0.5) * 2.0);
}

// =========================================================================
// MAIN
// =========================================================================
void main() {

    vec4 bioData = texture(u_BioDataTexture, v_TexCoord);

    vec2 uvTiled = v_TexCoord * 50.0;

    vec3 colSoil      = texture(u_SoilTexture, uvTiled).rgb;
    vec3 colGrass     = texture(u_GrassTexture, uvTiled).rgb;
    vec3 colStraw     = texture(u_StrawTexture, uvTiled).rgb;
    vec3 colStructure = texture(u_StructureTexture, uvTiled).rgb;

    vec3 finalColor;

    switch (u_Render.visualMode) {

        case 0: // Realista
            finalColor = getRealisticLook(
                bioData,
                colSoil,
                colGrass,
                colStraw,
                colStructure
            );
            break;

        case 1: // NDVI
            finalColor = getNDVILook(bioData);
            break;

        case 2: // Estrutural (ES)
            finalColor = vec3(bioData.g);
            break;

        case 3: // Estresse
            finalColor = mix(
                vec3(0.0, 0.0, 1.0),
                vec3(1.0, 0.0, 0.0),
                bioData.b
            );
            break;

        case 4: // Debug Raw
            finalColor = bioData.rgb;
            break;

        default:
            finalColor = vec3(1.0, 0.0, 1.0);
            break;
    }

    outColor = vec4(finalColor, 1.0);
}

