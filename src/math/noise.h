#pragma once

namespace math {

/**
 * @brief 2D Perlin noise generator for terrain height maps
 * 
 * Generates smooth, continuous random values suitable for
 * natural-looking terrain generation.
 */
class PerlinNoise {
public:
    /**
     * @brief Initialize with a seed
     */
    explicit PerlinNoise(unsigned int seed = 0);

    /**
     * @brief Get 2D Perlin noise value at (x, z)
     * @return Value in range [0, 1]
     */
    float noise2D(float x, float z) const;

    /**
     * @brief Multi-octave Perlin noise for more detail
     * 
     * Combines multiple frequencies of noise for natural variation
     * 
     * @param x X coordinate
     * @param z Z coordinate
     * @param octaves Number of noise layers (default 4)
     * @param persistence Amplitude multiplier per octave (default 0.5)
     * @return Value in range [0, 1]
     */
    float octaveNoise(float x, float z, int octaves = 4, float persistence = 0.5f) const;

private:
    // Permutation table for gradient selection
    int permutation[512];

    // Fade function for smooth interpolation
    static float fade(float t);

    // Linear interpolation
    static float lerp(float t, float a, float b);

    // Gradient function
    static float grad(int hash, float x, float z);
};

} // namespace math
