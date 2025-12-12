#include "noise.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace math {

PerlinNoise::PerlinNoise(unsigned int seed) {
    // Initialize permutation table with values 0-255
    int p[256];
    for (int i = 0; i < 256; i++) {
        p[i] = i;
    }

    // Shuffle using seed
    std::mt19937 rng(seed);
    std::shuffle(p, p + 256, rng);

    // Duplicate for easy wrapping
    for (int i = 0; i < 256; i++) {
        permutation[i] = p[i];
        permutation[256 + i] = p[i];
    }
}

float PerlinNoise::fade(float t) {
    // Smoothstep function: 6t^5 - 15t^4 + 10t^3
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float PerlinNoise::lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float PerlinNoise::grad(int hash, float x, float z) {
    // Convert hash to gradient direction
    int h = hash & 7;      // 8 gradient directions
    float u = h < 4 ? x : z;
    float v = h < 4 ? z : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float PerlinNoise::noise2D(float x, float z) const {
    // Find unit grid cell containing point
    int X = static_cast<int>(std::floor(x)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    // Relative position within cell [0, 1)
    x -= std::floor(x);
    z -= std::floor(z);

    // Fade curves for smooth interpolation
    float u = fade(x);
    float v = fade(z);

    // Hash coordinates of 4 corners
    int A  = permutation[X]     + Z;
    int AA = permutation[A];
    int AB = permutation[A + 1];
    int B  = permutation[X + 1] + Z;
    int BA = permutation[B];
    int BB = permutation[B + 1];

    // Blend results from 4 corners
    float res = lerp(v,
        lerp(u, grad(permutation[AA], x,     z    ),
                grad(permutation[BA], x - 1, z    )),
        lerp(u, grad(permutation[AB], x,     z - 1),
                grad(permutation[BB], x - 1, z - 1))
    );

    // Map from [-1, 1] to [0, 1]
    return (res + 1.0f) * 0.5f;
}

float PerlinNoise::octaveNoise(float x, float z, int octaves, float persistence) const {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;  // Normalization factor

    for (int i = 0; i < octaves; i++) {
        total += noise2D(x * frequency, z * frequency) * amplitude;

        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;  // Normalize to [0, 1]
}

} // namespace math
