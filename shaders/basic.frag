#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragViewDist;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
    float useLighting;   // 0.0 or 1.0
    float useFixedColor; // 0.0 or 1.0
    float opacity;       // Alpha
    vec3  fixedColor;    // Color Override
} pc;

// Pseudo-random noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 color = fragColor;
    
    // 0. Base Color & Noise
    if (pc.useFixedColor > 0.5) {
        color = pc.fixedColor;
    } else {
        // Add minimal noise texture for "grass" detail
        float noise = hash(fragNormal.xy * 10.0 + gl_FragCoord.xy * 0.1); 
        color += (noise - 0.5) * 0.05; // Subtle variation
    }

    // 1. Lighting
    if (pc.useLighting > 0.5) {
        vec3 norm = normalize(fragNormal);
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5)); // Sun from side
        
        // Diffuse (Sun)
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 sunColor = vec3(1.0, 0.95, 0.8);
        vec3 diffuse = diff * sunColor * 0.8;
        
        // Hemispheric Ambient (Sky Blue -> Ground Brown)
        // Up (0,1,0) = Sky, Down (0,-1,0) = Ground
        float hemi = (norm.y * 0.5 + 0.5); // 0..1
        vec3 skyAmbient = vec3(0.53, 0.81, 0.92) * 0.5;
        vec3 gndAmbient = vec3(0.2, 0.15, 0.1) * 0.3;
        vec3 ambient = mix(gndAmbient, skyAmbient, hemi);
        
        // Fake AO based on slope
        float ao = clamp(norm.y, 0.2, 1.0);
        
        color = color * (ambient + diffuse) * ao;
    }

    // 2. Distance Fog
    // Density 0.0015 -> visibility ~700m
    float fogDensity = 0.0015; 
    float fogFactor = 1.0 - exp(-fragViewDist * fogDensity);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    vec3 skyColor = vec3(0.53, 0.81, 0.92); // Matches Clear Color
    vec3 finalColor = mix(color, skyColor, fogFactor);

    outColor = vec4(finalColor, pc.opacity);
}
