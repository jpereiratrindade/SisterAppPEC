#pragma once

#include "mesh.h"
#include <vector>

namespace graphics {

/**
 * @brief Generates a wireframe grid on the XZ plane centered at origin.
 * @param vertices Output vertex buffer.
 * @param indices Output index buffer.
 * @param size How many cells in each direction (default 10).
 */
void createGrid(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, int size = 10);

/**
 * @brief Generates XYZ axis lines (Red=X, Green=Y, Blue=Z).
 * @param vertices Output vertex buffer.
 * @param indices Output index buffer.
 */
void createAxes(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices);

/**
 * @brief Generates cube geometry instances for a set of points.
 * 
 * For each point in 'points', a cube is generated centered at that point's position.
 * The cube inherits the point's color (or slightly modified lighting normals).
 * 
 * @param outVerts Output vertex buffer.
 * @param outIndices Output index buffer.
 * @param points Source points (centers of cubes).
 * @param size Size of the cube edges.
 */
void generateCubeMesh(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices, const std::vector<Vertex>& points, float size);

/**
 * @brief Generates pyramid geometry instances for a set of points.
 * 
 * Similar to generateCubeMesh but creates square-based pyramids.
 * 
 * @param outVerts Output vertex buffer.
 * @param outIndices Output index buffer.
 * @param points Source points (bases of pyramids).
 * @param size Size of the base edges and height.
 */
void generatePyramidMesh(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices, const std::vector<Vertex>& points, float size);

/**
 * @brief Generates a sky dome (half-sphere) with gradient colors.
 * @param vertices Output vertex buffer.
 * @param indices Output index buffer.
 * @param radius Radius of the dome (default 100).
 * @param segments Number of subdivisions (default 32).
 */
void createSkyDome(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, float radius = 100.0f, int segments = 32);

/**
 * @brief Generates distance marker cylinders at regular intervals.
 * @param vertices Output vertex buffer.
 * @param indices Output index buffer.
 * @param gridSize Size of the grid to place markers.
 * @param interval Spacing between markers (default 10).
 */
void createDistanceMarkers(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, int gridSize, int interval = 10);

} // namespace graphics
