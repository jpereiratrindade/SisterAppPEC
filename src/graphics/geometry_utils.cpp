#include "geometry_utils.h"
#include "../math/math_types.h"

namespace graphics {

using namespace math;

// Helper to add a quad (2 triangles)
static void addQuad(std::vector<Vertex>& verts, std::vector<uint16_t>& indices, 
             const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, 
             const Vec3& color, const Vec3& normal) {
    uint16_t base = static_cast<uint16_t>(verts.size());
    verts.push_back({{p0.x, p0.y, p0.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    verts.push_back({{p1.x, p1.y, p1.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    verts.push_back({{p2.x, p2.y, p2.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    verts.push_back({{p3.x, p3.y, p3.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    
    // Triangle 1
    indices.push_back(base); indices.push_back(base+1); indices.push_back(base+2);
    // Triangle 2
    indices.push_back(base); indices.push_back(base+2); indices.push_back(base+3);
}

// Helper to add a triangle
static void addTri(std::vector<Vertex>& verts, std::vector<uint16_t>& indices, 
            const Vec3& p0, const Vec3& p1, const Vec3& p2, 
            const Vec3& color, const Vec3& normal) {
    uint16_t base = static_cast<uint16_t>(verts.size());
    verts.push_back({{p0.x, p0.y, p0.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    verts.push_back({{p1.x, p1.y, p1.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    verts.push_back({{p2.x, p2.y, p2.z}, {color.x, color.y, color.z}, {normal.x, normal.y, normal.z}});
    
    indices.push_back(base); indices.push_back(base+1); indices.push_back(base+2);
}

void createGrid(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, int size) {
    vertices.clear();
    indices.clear();
    
    Vec3 normal = {0.0f, 1.0f, 0.0f}; // Normal for the grid plane
    
    // Create a much larger grid with subdivisions
    int halfSize = size / 2; // Will be 25 for size=50
    
    for (int i = -halfSize; i <= halfSize; ++i) {
        float pos = static_cast<float>(i);
        
        // Determine color based on position
        float cr = 0.4f, cg = 0.4f, cb = 0.4f; // Default grey
        
        // Main axes (every 10 units) are brighter
        if (i % 10 == 0) {
            cr = cg = cb = 0.7f;
        }
        
        // Center lines (X=0 or Z=0) have color
        if (i == 0) {
            // Will be colored by individual lines below
        }
        
        // Line along Z (fixed X)
        float zColor[3] = {cr, cg, cb};
        if (i == 0) {
            // X=0 line is BLUE (along Z axis)
            zColor[0] = 0.3f; zColor[1] = 0.5f; zColor[2] = 1.0f;
        }
        
        vertices.push_back({{ pos, 0.0f, -static_cast<float>(halfSize) }, {zColor[0], zColor[1], zColor[2]}, {normal.x, normal.y, normal.z}});
        vertices.push_back({{ pos, 0.0f, static_cast<float>(halfSize) },    {zColor[0], zColor[1], zColor[2]}, {normal.x, normal.y, normal.z}});
        uint16_t base = static_cast<uint16_t>(vertices.size() - 2);
        indices.push_back(base);
        indices.push_back(base + 1);

        // Line along X (fixed Z)
        float xColor[3] = {cr, cg, cb};
        if (i == 0) {
            // Z=0 line is RED (along X axis)
            xColor[0] = 1.0f; xColor[1] = 0.3f; xColor[2] = 0.3f;
        }
        
        vertices.push_back({{-static_cast<float>(halfSize), 0.0f, pos }, {xColor[0], xColor[1], xColor[2]}, {normal.x, normal.y, normal.z}});
        vertices.push_back({{ static_cast<float>(halfSize), 0.0f, pos },    {xColor[0], xColor[1], xColor[2]}, {normal.x, normal.y, normal.z}});
        base = static_cast<uint16_t>(vertices.size() - 2);
        indices.push_back(base);
        indices.push_back(base + 1);
    }
}

void createAxes(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    vertices.clear();
    indices.clear();
    
    float len = 2.0f;
    Vec3 dummyNormal = {0.0f, 1.0f, 0.0f};

    // X Axis (Red)
    vertices.push_back({{0.0f, 0.001f, 0.0f}, {1.0f, 0.0f, 0.0f}, {dummyNormal.x, dummyNormal.y, dummyNormal.z}}); 
    vertices.push_back({{len, 0.001f, 0.0f}, {1.0f, 0.0f, 0.0f}, {dummyNormal.x, dummyNormal.y, dummyNormal.z}});
    indices.push_back(0); indices.push_back(1);
    
    // Y Axis (Green)
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {dummyNormal.x, dummyNormal.y, dummyNormal.z}});
    vertices.push_back({{0.0f, len, 0.0f}, {0.0f, 1.0f, 0.0f}, {dummyNormal.x, dummyNormal.y, dummyNormal.z}});
    indices.push_back(2); indices.push_back(3);

    // Z Axis (Blue)
    vertices.push_back({{0.0f, 0.001f, 0.0f}, {0.0f, 0.0f, 1.0f}, {dummyNormal.x, dummyNormal.y, dummyNormal.z}});
    vertices.push_back({{0.0f, 0.001f, len}, {0.0f, 0.0f, 1.0f}, {dummyNormal.x, dummyNormal.y, dummyNormal.z}});
    indices.push_back(4); indices.push_back(5);
}

void generateCubeMesh(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices, const std::vector<Vertex>& points, float size) {
    outVerts.clear();
    outIndices.clear();
    float h = size * 0.5f;

    for (const auto& p : points) {
        float cx = p.pos[0], cy = p.pos[1], cz = p.pos[2];
        Vec3 c = {p.color[0], p.color[1], p.color[2]};
        
        Vec3 p0 = {cx-h, cy-h, cz+h};
        Vec3 p1 = {cx+h, cy-h, cz+h};
        Vec3 p2 = {cx+h, cy+h, cz+h};
        Vec3 p3 = {cx-h, cy+h, cz+h};
        Vec3 p4 = {cx-h, cy-h, cz-h};
        Vec3 p5 = {cx+h, cy-h, cz-h};
        Vec3 p6 = {cx+h, cy+h, cz-h};
        Vec3 p7 = {cx-h, cy+h, cz-h};
        
        // Front (+Z)
        addQuad(outVerts, outIndices, p0, p1, p2, p3, c, {0, 0, 1});
        // Back (-Z)
        addQuad(outVerts, outIndices, p5, p4, p7, p6, c, {0, 0, -1});
        // Left (-X)
        addQuad(outVerts, outIndices, p4, p0, p3, p7, c, {-1, 0, 0});
        // Right (+X)
        addQuad(outVerts, outIndices, p1, p5, p6, p2, c, {1, 0, 0});
        // Top (+Y)
        addQuad(outVerts, outIndices, p3, p2, p6, p7, c, {0, 1, 0});
        // Bottom (-Y)
        addQuad(outVerts, outIndices, p4, p5, p1, p0, c, {0, -1, 0});
    }
}

void generatePyramidMesh(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices, const std::vector<Vertex>& points, float size) {
    outVerts.clear();
    outIndices.clear();
    float h = size * 0.5f;

    for (const auto& p : points) {
        float cx = p.pos[0], cy = p.pos[1], cz = p.pos[2];
        Vec3 c = {p.color[0], p.color[1], p.color[2]};
        
        Vec3 top = {cx, cy+h, cz};
        Vec3 b0 = {cx-h, cy-h, cz+h};
        Vec3 b1 = {cx+h, cy-h, cz+h};
        Vec3 b2 = {cx+h, cy-h, cz-h};
        Vec3 b3 = {cx-h, cy-h, cz-h};
        
        // Base (Down)
        addQuad(outVerts, outIndices, b3, b2, b1, b0, c, {0, -1, 0});
        
        // Sides
        // Side 1 (Front)
        Vec3 n1 = normalize(cross(b1-b0, top-b0));
        addTri(outVerts, outIndices, b0, b1, top, c, n1);

        // Side 2 (Right)
        Vec3 n2 = normalize(cross(b2-b1, top-b1));
        addTri(outVerts, outIndices, b1, b2, top, c, n2);
        
        // Side 3 (Back)
        Vec3 n3 = normalize(cross(b3-b2, top-b2));
        addTri(outVerts, outIndices, b2, b3, top, c, n3);
        
        // Side 4 (Left)
        Vec3 n4 = normalize(cross(b0-b3, top-b3));
        addTri(outVerts, outIndices, b3, b0, top, c, n4);
    }
}

void createSkyDome(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, float radius, int segments) {
    vertices.clear();
    indices.clear();
    
    // Create a dome (half sphere) with gradient colors
    // Sky dome doesn't need precise normals as it's distant background
    
    int rings = segments / 2; // Half sphere
    
    for (int lat = 0; lat <= rings; ++lat) {
        float theta = (float)lat / (float)rings * (3.14159f / 2.0f); // 0 to PI/2
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);
        
        for (int lon = 0; lon <= segments; ++lon) {
            float phi = (float)lon / (float)segments * 2.0f * 3.14159f; // 0 to 2PI
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            
            // Position
            float x = radius * sinTheta * cosPhi;
            float y = radius * cosTheta;
            float z = radius * sinTheta * sinPhi;
            
            // Gradient color: bottom (horizon) is light blue/cyan, top (zenith) is deep blue
            float t = (float)lat / (float)rings; // 0 at horizon, 1 at zenith
            float r = 0.4f + t * 0.1f; // Slight reddish tint at horizon
            float g = 0.6f + t * 0.2f;
            float b = 0.8f + t * 0.2f;
            
            Vec3 normal = {0, 1, 0}; // Dummy normal
            
            vertices.push_back({{x, y, z}, {r, g, b}, {normal.x, normal.y, normal.z}});
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < rings; ++lat) {
        for (int lon = 0; lon < segments; ++lon) {
            int current = lat * (segments + 1) + lon;
            int next = current + segments + 1;
            
            indices.push_back(static_cast<uint16_t>(current));
            indices.push_back(static_cast<uint16_t>(next));
            indices.push_back(static_cast<uint16_t>(current + 1));
            
            indices.push_back(static_cast<uint16_t>(current + 1));
            indices.push_back(static_cast<uint16_t>(next));
            indices.push_back(static_cast<uint16_t>(next + 1));
        }
    }
}

void createDistanceMarkers(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, int gridSize, int interval) {
    vertices.clear();
    indices.clear();
    
    int halfSize = gridSize / 2;
    
    // Create simple vertical line markers at intervals
    for (int x = -halfSize; x <= halfSize; x += interval) {
        for (int z = -halfSize; z <= halfSize; z += interval) {
            if (x == 0 && z == 0) continue; // Skip origin
            
            float fx = static_cast<float>(x);
            float fz = static_cast<float>(z);
            
            // Distance from origin for color coding
            float dist = std::sqrt(fx*fx + fz*fz);
            float maxDist = std::sqrt(2.0f * static_cast<float>(halfSize) * static_cast<float>(halfSize));
            float t = dist / maxDist;
            
            // Color: green (near) to yellow (medium) to red (far)
            float r = t;
            float g = 1.0f - (t * 0.5f);
            float b = 0.2f;
            
            // Height based on distance (taller = farther)
            float height = 1.0f + t * 3.0f;
            
            Vec3 normal{0, 1, 0};
            
            // Simple vertical line
            vertices.push_back({{fx, 0.0f, fz}, {r, g, b}, {normal.x, normal.y, normal.z}});
            vertices.push_back({{fx, height, fz}, {r, g, b}, {normal.x, normal.y, normal.z}});
            
            uint16_t base = static_cast<uint16_t>(vertices.size() - 2);
            indices.push_back(base);
            indices.push_back(base + 1);
        }
    }
}

} // namespace graphics
