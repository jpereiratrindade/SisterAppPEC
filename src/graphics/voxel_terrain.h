#pragma once

#include "../core/graphics_context.h"
#include "mesh.h"
#include "../math/frustum.h"
#include "mesh.h"
#include "../math/intersection.h"
#include "../math/noise.h"
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <queue>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace graphics {

/**
 * @brief Types of blocks in the voxel world
 */
enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Water,
    Wood,      // Tree trunks
    Leaves     // Tree foliage
};

enum class TerrainClass : uint8_t {
    Flat,
    GentleSlope,
    Rolling,      // Ondulado
    SteepSlope,
    Mountain
};

enum class VegetationClass : uint8_t {
    None,
    Sparse,
    Rich
};

enum class TerrainModel : uint8_t {
    RippledFlat = 0,   // Plano com poucas ondulações
    SmoothHills = 1,   // Suave ondulado (default)
    RollingHills = 2,  // Ondulado
    Mountainous = 3    // Montanhoso
};

/**
 * @brief Configuration for slope classification ranges (in Percentage %)
 */
struct SlopeConfig {
    float flatMaxPct = 3.0f;       // 0 to 3% = Flat (User Request: 3% default)
    float gentleMaxPct = 8.0f;    // 3% to 8% = Gentle Slope
    float onduladoMaxPct = 20.0f;    // 8% to 20% = Ondulado
    float steepMaxPct = 45.0f;     // 20% to 45% = Steep Slope
    // > 45% = Mountain
};

struct SurfaceHit {
    int worldX = 0;
    int worldY = 0;
    int worldZ = 0;
    TerrainClass terrainClass = TerrainClass::Flat;
    BlockType surfaceBlock = BlockType::Air;
    float slopePct = 0.0f; // Slope in Percentage
    float moisture = 0.0f;
    VegetationClass vegetation = VegetationClass::None;
    bool valid = false;
};

/**
 * @brief Individual block data
 */
struct Block {
    BlockType type = BlockType::Air;
    
    bool isSolid() const {
        return type != BlockType::Air && type != BlockType::Water;
    }
};

/**
 * @brief A 16x16x64 section of the world
 */
class Chunk {
public:
    static constexpr int CHUNK_SIZE = 16;
    static constexpr int CHUNK_HEIGHT = 64;

    Chunk(int worldX, int worldZ);

    /**
     * @brief Get block at local coordinates
     */
    Block* getBlock(int x, int y, int z);
    const Block* getBlock(int x, int y, int z) const;

    /**
     * @brief Set block at local coordinates
     */
    void setBlock(int x, int y, int z, BlockType type);

    /**
     * @brief Check if chunk needs mesh rebuild
     */
    /**
     * @brief Check if chunk needs mesh rebuild
     */
    bool isDirty() const { return dirty_.load(std::memory_order_relaxed); }
    void markDirty() { dirty_.store(true, std::memory_order_relaxed); }
    void markClean() { dirty_.store(false, std::memory_order_relaxed); }

    /**
     * @brief Get world-space bounding box for frustum culling
     */
    const math::AABB& getAABB() const { return aabb_; }

    /**
     * @brief Get chunk world position
     */
    int getWorldX() const { return worldX_; }
    int getWorldZ() const { return worldZ_; }

    /**
     * @brief Access to mesh (may be null if not generated)
     */
    Mesh* getMesh() const { auto sp = std::atomic_load(&mesh_); return sp.get(); }
    std::shared_ptr<Mesh> getMeshShared() const { return std::atomic_load(&mesh_); }
    void setMesh(std::shared_ptr<Mesh> mesh) { std::atomic_store(&mesh_, std::move(mesh)); }
    std::shared_ptr<Mesh> getWaterMesh() const { return std::atomic_load(&waterMesh_); }
    void setWaterMesh(std::shared_ptr<Mesh> mesh) { std::atomic_store(&waterMesh_, std::move(mesh)); }
    void setTerrainClass(int x, int z, TerrainClass c) { terrainClass_[x][z] = c; }
    TerrainClass terrainClass(int x, int z) const { return terrainClass_[x][z]; }
    void setVegetationVersion(uint32_t v) { vegetationVersion_ = v; }
    uint32_t vegetationVersion() const { return vegetationVersion_; }

private:
    int worldX_, worldZ_;  // Chunk coordinates in world space
    Block blocks_[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    std::atomic<bool> dirty_{true};
    math::AABB aabb_;
    std::shared_ptr<Mesh> mesh_;
    std::shared_ptr<Mesh> waterMesh_;
    std::atomic<bool> isGenerated_{false}; // True when blocks are filled
    TerrainClass terrainClass_[CHUNK_SIZE][CHUNK_SIZE] = {}; // per-column classification
    uint32_t vegetationVersion_ = 0;
public:
    bool isGenerated() const { return isGenerated_; }
    void markGenerated() { isGenerated_ = true; }
};

/**
 * @brief Manages voxel terrain generation and rendering
 */
class VoxelTerrain {
public:
    VoxelTerrain(const core::GraphicsContext& context, unsigned int seed = 12345);
    ~VoxelTerrain();

    /**
     * @brief Update terrain around camera position
     * 
     * Loads/unloads chunks based on view distance and frustum visibility
     */
    void update(float cameraX, float cameraZ, const math::Frustum& frustum, int frameIndex);

    /**
     * @brief Rebuild mesh for a dirty chunk
     */
    void rebuildChunkMesh(Chunk* chunk);

    /**
     * @brief Get chunks visible in frustum
     */
    std::vector<Chunk*> getVisibleChunks(const math::Frustum& frustum);

    /**
     * @brief Get block at world coordinates
     */
    Block* getBlock(int worldX, int worldY, int worldZ);

    /**
     * @brief Set block at world coordinates
     */
    void setBlock(int worldX, int worldY, int worldZ, BlockType type);

    /**
     * @brief Get terrain height at world XZ position
     */
    int getTerrainHeight(int worldX, int worldZ) const;

    /**
     * @brief Enable/disable vegetation (wood/leaves) rendering
     */
    void setVegetationEnabled(bool enabled);
    bool vegetationEnabled() const { return vegetationEnabled_; }
    void setVegetationDensity(float d);
    float vegetationDensity() const { return vegetationDensity_; }
    void setTerrainModel(TerrainModel model);
    TerrainModel terrainModel() const { return terrainModel_; }
    void setSafeMode(bool enabled);
    Block* getBlockIfLoaded(int worldX, int worldY, int worldZ);
    
    // Resilience methods preserved for API compatibility but logic disabled
    void setResilienceEcol(float v);
    void setResilienceProd(float v);
    void setResilienceSoc(float v);
    float resilienceEcol() const { return resEcol_.load(std::memory_order_relaxed); }
    float resilienceProd() const { return resProd_.load(std::memory_order_relaxed); }
    float resilienceSoc() const { return resSoc_.load(std::memory_order_relaxed); }
    
    // New Slope Analysis Methods (v3.4.0)
    void setSlopeConfig(const SlopeConfig& config);
    SlopeConfig getSlopeConfig() const;
    /**
     * @brief Get slope in Percentage (0-100%+) at a specific coordinate
     */
    float getSlopeAt(int worldX, int worldZ) const;

    int chunkCount();
    int pendingTaskCount();
    int pendingVegetationCount();

    /**
     * @brief Probe terrain along a ray to find surface classification
     */
    bool probeSurface(const math::Ray& ray, float maxDistance, SurfaceHit& outHit) const;

    /**
     * @brief Clear all chunks and regenerate warmup area
     */
    void reset(int warmupRadius = 1);

    // Frame fences hook to allow safe deferred destruction without bloquear o render loop
    void setFrameFences(const std::vector<VkFence>* fences) { frameFences_ = fences; }

private:
    /**
     * @brief Get or create chunk
     */
    std::shared_ptr<Chunk> getChunk(int chunkX, int chunkZ, bool async);
    std::shared_ptr<Chunk> getChunk(int chunkX, int chunkZ) { return getChunk(chunkX, chunkZ, false); }

    /**
     * @brief Generate terrain for a chunk
     */
    void generateChunk(Chunk* chunk);
    
    /**
     * @brief Generate a tree at world position
     */
    void generateTree(int worldX, int worldY, int worldZ, int height = 5);

    /**
     * @brief Convert chunk coords to world coords
     */
    static void chunkCoordsFromWorld(int worldX, int worldZ, int& chunkX, int& chunkZ);
    
    /**
     * @brief Get block color based on type
     */
    static void getBlockColor(BlockType type, float& r, float& g, float& b);

    /**
     * @brief Check if block face should be rendered (neighbor is transparent)
     */
    bool shouldRenderFace(int x, int y, int z, int nx, int ny, int nz);

public:
    void setViewDistance(int d) { viewDistance_ = std::max(4, std::min(d, 16)); }
    int viewDistance() const { return viewDistance_; }
    void setChunkBudgetPerFrame(int v) { maxChunksPerFrame_ = std::max(1, std::min(v, 32)); }
    int chunkBudgetPerFrame() const { return maxChunksPerFrame_; }
    void setMeshBudgetPerFrame(int v) { maxMeshesPerFrame_ = std::max(1, std::min(v, 8)); }
    int meshBudgetPerFrame() const { return maxMeshesPerFrame_; }
    std::mutex& meshMutex() { return meshMutex_; }

private:
    void pruneFarChunks(int centerChunkX, int centerChunkZ, int frameIndex);
    void warmupInitialArea(int radius);
    struct TerrainProfile {
        float baseFreq = 0.01f;
        float detailFreq = 0.05f;
        int baseOctaves = 3;
        int detailOctaves = 2;
        float baseWeight = 0.7f;
        float detailWeight = 0.3f;
        int plainsBaseHeight = 20;
        int plainsRange = 20;
        int mountainBaseHeight = 24;
        int mountainRange = 40;
        float biomeFreq = 0.008f;
        float mountainCurve = 1.1f;
    };
    TerrainProfile profileForModel(TerrainModel model) const;
    struct ResilienceDerived {
        float ecol = 0.5f;
        float prod = 0.5f;
        float soc = 0.5f;
        float baseFreqMul = 1.0f;
        float detailFreqMul = 1.0f;
        float detailWeightMul = 1.0f;
        float plainsRangeMul = 1.0f;
        float mountainRangeMul = 1.0f;
        float mountainCurveMul = 1.0f;
        float fertilityMoistureWeight = 0.6f;
        float fertilityHeightWeight = 0.4f;
        float fertilityBias = 0.0f;
        float corridorSpacing = 24.0f;
        float corridorStrength = 0.0f;
        float smoothingFactor = 0.0f;
    };
    ResilienceDerived resilienceDerived() const;
    float socialCorridorMask(int worldX, int worldZ, const ResilienceDerived& derived) const;
    void logResilienceState(const char* reason) const;
    int sampleHeight(int worldX, int worldZ, BlockType& surfaceBlock, BlockType& subBlock) const;
    TerrainClass classifyTerrain(int worldX, int worldZ, int centerHeight) const;
    float moistureAt(int worldX, int worldZ) const;
    float slopePctAt(int worldX, int worldZ, int centerHeight) const;
    void refreshVegetation(Chunk* chunk);
    VegetationClass sampleVegetationAt(int worldX, int worldZ) const;
    void startWorkers();
    void workerLoop();
    void enqueueGenerate(const std::shared_ptr<Chunk>& chunk);
    void enqueueMesh(const std::shared_ptr<Chunk>& chunk);
    void enqueueVegetation(const std::shared_ptr<Chunk>& chunk);
    std::vector<std::shared_ptr<Chunk>> snapshotChunks();
    void clearPendingTasks();
    void queueVegetationRegen(const std::shared_ptr<Chunk>& chunk);
    void processPendingVegetation();
    void clearPendingVegetation();
    void flushDeferredDestroy(bool force);
    void waitForWorkersIdle();
    void gcFrameResources();

    const core::GraphicsContext& context_;
    math::PerlinNoise noise_;
    
    // Chunk storage (keyed by chunk coordinates)
    std::map<std::pair<int, int>, std::shared_ptr<Chunk>> chunks_;
    mutable std::mutex chunksMutex_;
    std::set<std::pair<int, int>> pendingGenerate_;
    std::set<std::pair<int, int>> pendingMesh_;
    
    int viewDistance_ = 6;       // chunks
    int maxChunksPerFrame_ = 4;  // async gen budget
    int maxMeshesPerFrame_ = 2;  // rebuild budget
    std::atomic<bool> vegetationEnabled_{false};
    float vegetationDensity_ = 1.0f; // 0 = none, 1 = default, 2 = dense
    uint32_t vegetationVersion_ = 1;
    TerrainModel terrainModel_ = TerrainModel::SmoothHills;
    bool safeMode_ = true;
    SlopeConfig slopeConfig_; // v3.4.0
    mutable std::mutex configMutex_; 
    std::atomic<float> resEcol_{0.5f};
    std::atomic<float> resProd_{0.5f};
    std::atomic<float> resSoc_{0.5f};

    struct Task {
        enum class Type { Generate, Mesh, Vegetation };
        Type type;
        std::shared_ptr<Chunk> chunk;
    };
    std::queue<Task> taskQueue_;
    std::mutex taskMutex_;
    std::condition_variable taskCv_;
    std::atomic<bool> stopWorkers_{false};
    std::atomic<int> activeTasks_{0};
    std::vector<std::thread> workers_;
    const size_t maxPendingTasks_ = 64;
    std::mutex meshMutex_;
    std::mutex vegMutex_;
    std::vector<std::shared_ptr<Chunk>> pendingVegRegen_;
    std::set<std::pair<int,int>> pendingVegRegenKeys_;
    const size_t vegRegenPerFrame_ = 1;

    // Garbage Collection for GPU Resources (Fence-Aware)
    std::mutex garbageMutex_;
    std::vector<std::shared_ptr<void>> meshGarbage_; // Thread-safe handover from workers
    std::vector<std::pair<VkFence, std::shared_ptr<void>>> deferredResources_; // Paired with fence

    std::chrono::steady_clock::time_point lastDestroyFlush_ = std::chrono::steady_clock::now();
    const std::vector<VkFence>* frameFences_ = nullptr;
};

} // namespace graphics
