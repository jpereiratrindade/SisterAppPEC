#version 450

//----------------------------------------------------
// Vertex Inputs
//----------------------------------------------------
layout(location = 0) in vec3 inPosition;  // posição em world-space
layout(location = 1) in vec3 inColor;     // cor opcional (ex.: para marcadores)
layout(location = 2) in vec3 inNormal;    // normal da geometria

//----------------------------------------------------
// Outputs para o fragment shader
//----------------------------------------------------
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragNormal;

//----------------------------------------------------
// Push constant APENAS com MVP
//----------------------------------------------------
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

//----------------------------------------------------
// Main
//----------------------------------------------------
void main() {

    // Transformação para clip-space
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    // Dados enviados ao fragment shader
    fragWorldPos = inPosition;          // usado para fog, sky, horizon, etc.
    fragColor    = inColor;             // usada caso haja geometria colorida
    fragNormal   = normalize(inNormal); // iluminação simples no frag
}
