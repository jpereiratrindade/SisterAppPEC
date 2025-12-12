#include "voxel_terrain.h"
#include <cmath>
#include <algorithm>
#include <utility>
#include <vulkan/vulkan.h>
#include <iostream>
#include <chrono>

namespace graphics {

// ============================================================================
// Chunk Implementation
// ============================================================================

Chunk::Chunk(int worldX, int worldZ) 
    : worldX_(worldX), worldZ_(worldZ) {
    
    // Initialize all blocks to air
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                blocks_[x][y][z].type = BlockType::Air;
            }
        }
    }
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            terrainClass_[x][z] = TerrainClass::Flat;
        }
    }

    // Calculate world-space AABB
    float wx = static_cast<float>(worldX * CHUNK_SIZE);
    float wz = static_cast<float>(worldZ * CHUNK_SIZE);
    aabb_ = math::AABB(
        wx, 0.0f, wz,
        wx + CHUNK_SIZE, CHUNK_HEIGHT, wz + CHUNK_SIZE
    );
}

Block* Chunk::getBlock(int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return nullptr;
    }
    return &blocks_[x][y][z];
}

const Block* Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return nullptr;
    }
    return &blocks_[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_SIZE) {
        blocks_[x][y][z].type = type;
        markDirty();
    }
}

// ============================================================================
// VoxelTerrain Implementation
// ============================================================================

VoxelTerrain::VoxelTerrain(const core::GraphicsContext& context, unsigned int seed)
    : context_(context), noise_(seed) {
    startWorkers();
    logResilienceState("init");
    warmupInitialArea(1); // generate uma área mínima de spawn de forma síncrona
}

VoxelTerrain::~VoxelTerrain() {
    stopWorkers_.store(true);
    taskCv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    clearPendingTasks();
}

void VoxelTerrain::startWorkers() {
    unsigned int hw = std::thread::hardware_concurrency();
    unsigned int workerCount = hw > 0 ? std::clamp(hw / 4, 2u, 6u) : 4u;
    workers_.reserve(workerCount);
    for (unsigned int i = 0; i < workerCount; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

void VoxelTerrain::clearPendingTasks() {
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        std::queue<Task> empty;
        std::swap(taskQueue_, empty);
    }
    std::lock_guard<std::mutex> lock(chunksMutex_);
    pendingGenerate_.clear();
    pendingMesh_.clear();
    clearPendingVegetation();
    std::lock_guard<std::mutex> gLock(garbageMutex_);
    deferredResources_.clear();
    meshGarbage_.clear();
}

std::vector<std::shared_ptr<Chunk>> VoxelTerrain::snapshotChunks() {
    std::vector<std::shared_ptr<Chunk>> out;
    std::lock_guard<std::mutex> lock(chunksMutex_);
    out.reserve(chunks_.size());
    for (auto& kv : chunks_) {
        out.push_back(kv.second);
    }
    return out;
}

void VoxelTerrain::clearPendingVegetation() {
    std::lock_guard<std::mutex> lock(vegMutex_);
    pendingVegRegen_.clear();
    pendingVegRegenKeys_.clear();
}

void VoxelTerrain::workerLoop() {
    for (;;) {
        Task task;
        size_t queueRemaining = 0;
        {
            std::unique_lock<std::mutex> lock(taskMutex_);
            taskCv_.wait(lock, [this] { return stopWorkers_.load() || !taskQueue_.empty(); });
            if (stopWorkers_.load() && taskQueue_.empty()) {
                return;
            }
            task = taskQueue_.front();
            taskQueue_.pop();
            queueRemaining = taskQueue_.size();
        }

        if (!task.chunk) continue;

        int activeNow = activeTasks_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (activeNow > 1) {
            std::cout << "[Workers] active=" << activeNow
                      << " queue=" << queueRemaining
                      << " task=" << (task.type == Task::Type::Generate ? "gen" :
                                      task.type == Task::Type::Mesh ? "mesh" : "veg")
                      << " chunk=(" << task.chunk->getWorldX() << "," << task.chunk->getWorldZ() << ")"
                      << std::endl;
        }

        auto key = std::make_pair(task.chunk->getWorldX(), task.chunk->getWorldZ());
        if (task.type == Task::Type::Generate) {
            generateChunk(task.chunk.get());
            task.chunk->markDirty();
            {
                std::lock_guard<std::mutex> lock(chunksMutex_);
                pendingGenerate_.erase(key);
            }
            enqueueMesh(task.chunk);
        } else if (task.type == Task::Type::Mesh) {
            rebuildChunkMesh(task.chunk.get());
            task.chunk->markClean();
            {
                std::lock_guard<std::mutex> lock(chunksMutex_);
                pendingMesh_.erase(key);
            }
        } else if (task.type == Task::Type::Vegetation) {
            refreshVegetation(task.chunk.get());
            task.chunk->setVegetationVersion(vegetationVersion_);
            task.chunk->markDirty();
        }
        activeTasks_.fetch_sub(1, std::memory_order_relaxed);
        taskCv_.notify_all();
    }
}

void VoxelTerrain::chunkCoordsFromWorld(int worldX, int worldZ, int& chunkX, int& chunkZ) {
    chunkX = worldX >= 0 ? worldX / Chunk::CHUNK_SIZE : (worldX - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE;
    chunkZ = worldZ >= 0 ? worldZ / Chunk::CHUNK_SIZE : (worldZ - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE;
}

void VoxelTerrain::enqueueGenerate(const std::shared_ptr<Chunk>& chunk) {
    if (!chunk) return;
    auto key = std::make_pair(chunk->getWorldX(), chunk->getWorldZ());
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        std::lock_guard<std::mutex> qlock(taskMutex_);
        size_t totalPending = pendingGenerate_.size() + pendingMesh_.size() + taskQueue_.size();
        if (totalPending >= maxPendingTasks_) return; // throttle to avoid runaway queue
    }
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        if (pendingGenerate_.count(key)) return;
        pendingGenerate_.insert(key);
    }
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskQueue_.push(Task{Task::Type::Generate, chunk});
    }
    taskCv_.notify_one();
}

void VoxelTerrain::enqueueMesh(const std::shared_ptr<Chunk>& chunk) {
    if (!chunk) return;
    auto key = std::make_pair(chunk->getWorldX(), chunk->getWorldZ());
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        std::lock_guard<std::mutex> qlock(taskMutex_);
        size_t totalPending = pendingGenerate_.size() + pendingMesh_.size() + taskQueue_.size();
        if (totalPending >= maxPendingTasks_) return; // throttle
    }
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        if (pendingMesh_.count(key)) return;
        pendingMesh_.insert(key);
    }
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskQueue_.push(Task{Task::Type::Mesh, chunk});
    }
    taskCv_.notify_one();
}

void VoxelTerrain::enqueueVegetation(const std::shared_ptr<Chunk>& chunk) {
    if (!chunk) return;
    size_t totalPending = 0;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        totalPending = taskQueue_.size();
    }
    {
        std::lock_guard<std::mutex> lockChunks(chunksMutex_);
        totalPending += pendingGenerate_.size();
        totalPending += pendingMesh_.size();
    }
    if (totalPending >= maxPendingTasks_) return;

    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskQueue_.push(Task{Task::Type::Vegetation, chunk});
    }
    taskCv_.notify_one();
}

std::shared_ptr<Chunk> VoxelTerrain::getChunk(int chunkX, int chunkZ, bool async) {
    std::shared_ptr<Chunk> chunk;
    bool newlyCreated = false;
    auto key = std::make_pair(chunkX, chunkZ);
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        auto it = chunks_.find(key);
        if (it != chunks_.end()) {
            chunk = it->second;
        } else {
            chunk = std::make_shared<Chunk>(chunkX, chunkZ);
            chunks_[key] = chunk;
            newlyCreated = true;
        }
    }

    if (newlyCreated) {
        if (async) {
            enqueueGenerate(chunk);
        } else {
            generateChunk(chunk.get());
            chunk->markDirty();
            rebuildChunkMesh(chunk.get());
            chunk->markClean();
        }
    }

    return chunk;
}

void VoxelTerrain::pruneFarChunks(int centerChunkX, int centerChunkZ, int frameIndex) {
    // Remove chunks that are far beyond the active radius to keep draw calls and memory lower
    std::vector<std::pair<int, int>> toErase;
    int maxRadius = viewDistance_ + 1; // small hysteresis
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        for (const auto& kv : chunks_) {
            int dx = std::abs(kv.first.first - centerChunkX);
            int dz = std::abs(kv.first.second - centerChunkZ);
            if (dx > maxRadius || dz > maxRadius) {
                toErase.push_back(kv.first);
            }
        }
    }
    
    // Safety check for frame fences
    VkFence currentFence = (frameFences_ && frameIndex >= 0 && frameIndex < static_cast<int>(frameFences_->size())) ? (*frameFences_)[frameIndex] : VK_NULL_HANDLE;

    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        std::lock_guard<std::mutex> gLock(garbageMutex_);
        for (const auto& key : toErase) {
            auto it = chunks_.find(key);
            if (it != chunks_.end()) {
                // Keep the chunk alive in deferred list associated with the fence
                deferredResources_.push_back({currentFence, it->second});
            }
            pendingGenerate_.erase(key);
            pendingMesh_.erase(key);
            chunks_.erase(key);
        }
    }

    // Avoid leaking memory: periodically drain deferred destroys when it's safe
    // Note: pruneFarChunks is called from update(), so deferred destruction is handled in gcFrameResources()
}

void VoxelTerrain::warmupInitialArea(int radius) {
    for (int cx = -radius; cx <= radius; ++cx) {
        for (int cz = -radius; cz <= radius; ++cz) {
            getChunk(cx, cz, /*async*/false);
        }
    }
}

void VoxelTerrain::reset(int warmupRadius) {
    waitForWorkersIdle();
    // Ensure GPU is idle before destroying meshes referenced by in-flight command buffers
    vkDeviceWaitIdle(context_.device());
    clearPendingTasks();
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        chunks_.clear();
    }
    {
        std::lock_guard<std::mutex> gLock(garbageMutex_);
        deferredResources_.clear();
        meshGarbage_.clear();
    }
    logResilienceState("reset");
    warmupInitialArea(warmupRadius);
}

void VoxelTerrain::refreshVegetation(Chunk* chunk) {
    std::lock_guard<std::mutex> meshLock(meshMutex_);
    if (!chunk) return;
    ResilienceDerived derived = resilienceDerived();

    // Remove existing vegetation
    for (int x = 0; x < Chunk::CHUNK_SIZE; x++) {
        for (int z = 0; z < Chunk::CHUNK_SIZE; z++) {
            for (int y = 0; y < Chunk::CHUNK_HEIGHT; y++) {
                Block* b = chunk->getBlock(x, y, z);
                if (b && (b->type == BlockType::Wood || b->type == BlockType::Leaves)) {
                    b->type = BlockType::Air;
                }
            }
        }
    }

    if (!vegetationEnabled_) return;

    int chunkWorldX = chunk->getWorldX() * Chunk::CHUNK_SIZE;
    int chunkWorldZ = chunk->getWorldZ() * Chunk::CHUNK_SIZE;

    for (int x = 2; x < Chunk::CHUNK_SIZE - 2; x++) {
        for (int z = 2; z < Chunk::CHUNK_SIZE - 2; z++) {
            TerrainClass tClass = chunk->terrainClass(x, z);
            float treeNoise = noise_.noise2D(static_cast<float>(chunkWorldX + x), static_cast<float>(chunkWorldZ + z));
            float moisture = moistureAt(chunkWorldX + x, chunkWorldZ + z);
            float density = vegetationDensity_;
            float corridorMask = socialCorridorMask(chunkWorldX + x, chunkWorldZ + z, derived);

            int surfaceY = -1;
            Block* surfaceBlock = nullptr;
            for (int y = Chunk::CHUNK_HEIGHT - 1; y >= 0; y--) {
                Block* b = chunk->getBlock(x, y, z);
                if (b && b->type != BlockType::Air && b->type != BlockType::Water) {
                    surfaceY = y;
                    surfaceBlock = b;
                    break;
                }
            }
            float normHeight = surfaceY >= 0 ? static_cast<float>(surfaceY) / static_cast<float>(Chunk::CHUNK_HEIGHT) : 1.0f;
            float fertility = derived.fertilityMoistureWeight * moisture +
                              derived.fertilityHeightWeight * (1.0f - normHeight) +
                              derived.fertilityBias;
            fertility = std::clamp(fertility, 0.0f, 1.0f);

            float treeThreshold = 1.1f;
            if (tClass == TerrainClass::Flat) {
                float densityBias = (density - 1.0f) * 0.35f;      // [-0.35, +0.35]
                float moistureBias = (moisture - 0.5f) * 0.25f;    // [-0.125, +0.125]
                float fertilityBias = (fertility - 0.5f) * 0.35f;  // [-0.175, +0.175]
                float corridorPenalty = corridorMask * derived.corridorStrength * 0.6f;
                treeThreshold = 0.7f - densityBias - moistureBias - fertilityBias + corridorPenalty;
            } else if (tClass == TerrainClass::Slope) {
                float densityBias = (density - 1.0f) * 0.3f;
                float moistureBias = (moisture - 0.5f) * 0.2f;
                float fertilityBias = (fertility - 0.5f) * 0.25f;
                float corridorPenalty = corridorMask * derived.corridorStrength * 0.6f;
                treeThreshold = 0.45f - densityBias - moistureBias - fertilityBias + corridorPenalty;
            } else if (tClass == TerrainClass::Mountain) {
                float corridorPenalty = corridorMask * derived.corridorStrength * 0.4f;
                treeThreshold = 1.2f + corridorPenalty;
            }
            treeThreshold = std::clamp(treeThreshold, -0.2f, 1.25f);

            if (treeNoise > treeThreshold && surfaceY > 0 && surfaceY < Chunk::CHUNK_HEIGHT - 10) {
                if (surfaceBlock && surfaceBlock->type == BlockType::Grass) {
                    int treeH = 4 + static_cast<int>(treeNoise * 2.0f) + static_cast<int>(fertility * 2.0f);
                    treeH = std::max(3, std::min(treeH, 8));
                    generateTree(chunkWorldX + x, surfaceY + 1, chunkWorldZ + z, treeH);
                }
            }
        }
    }

    chunk->setVegetationVersion(vegetationVersion_);
}

VoxelTerrain::TerrainProfile VoxelTerrain::profileForModel(TerrainModel model) const {
    switch (model) {
        case TerrainModel::RippledFlat:
            return TerrainProfile{
                /*baseFreq*/0.006f, /*detailFreq*/0.024f,
                /*baseOctaves*/3, /*detailOctaves*/2,
                /*baseWeight*/0.85f, /*detailWeight*/0.15f,
                /*plainsBaseHeight*/18, /*plainsRange*/12,
                /*mountainBaseHeight*/22, /*mountainRange*/22,
                /*biomeFreq*/0.006f, /*mountainCurve*/1.05f
            };
        case TerrainModel::RollingHills:
            return TerrainProfile{
                /*baseFreq*/0.016f, /*detailFreq*/0.07f,
                /*baseOctaves*/4, /*detailOctaves*/3,
                /*baseWeight*/0.6f, /*detailWeight*/0.4f,
                /*plainsBaseHeight*/22, /*plainsRange*/24,
                /*mountainBaseHeight*/26, /*mountainRange*/36,
                /*biomeFreq*/0.01f, /*mountainCurve*/1.35f
            };
        case TerrainModel::SmoothHills:
        default:
            return TerrainProfile{
                /*baseFreq*/0.01f, /*detailFreq*/0.05f,
                /*baseOctaves*/4, /*detailOctaves*/2,
                /*baseWeight*/0.7f, /*detailWeight*/0.3f,
                /*plainsBaseHeight*/20, /*plainsRange*/18,
                /*mountainBaseHeight*/24, /*mountainRange*/30,
                /*biomeFreq*/0.008f, /*mountainCurve*/1.1f
            };
    }
}

int VoxelTerrain::sampleHeight(int worldX, int worldZ, BlockType& surfaceBlock, BlockType& subBlock) const {
    TerrainProfile profile = profileForModel(terrainModel_);
    ResilienceDerived derived = resilienceDerived();

    profile.baseFreq *= derived.baseFreqMul;
    profile.detailFreq *= derived.detailFreqMul;
    int plainsRange = static_cast<int>(std::round(static_cast<float>(profile.plainsRange) * derived.plainsRangeMul));
    int mountainRange = static_cast<int>(std::round(static_cast<float>(profile.mountainRange) * derived.mountainRangeMul));
    plainsRange = std::max(8, std::min(plainsRange, Chunk::CHUNK_HEIGHT));
    mountainRange = std::max(12, std::min(mountainRange, Chunk::CHUNK_HEIGHT));
    float mountainCurve = profile.mountainCurve * derived.mountainCurveMul;

    float corridorMask = socialCorridorMask(worldX, worldZ, derived);

    float detailWeight = std::clamp(profile.detailWeight * derived.detailWeightMul, 0.15f, 0.6f);
    float baseWeight = std::max(0.2f, profile.baseWeight);
    float weightSum = baseWeight + detailWeight;
    baseWeight /= weightSum;
    detailWeight /= weightSum;
    if (corridorMask > 0.0f) {
        float smoothing = derived.smoothingFactor * corridorMask;
        detailWeight *= (1.0f - 0.5f * smoothing);
        weightSum = baseWeight + detailWeight;
        if (weightSum > 0.0001f) {
            baseWeight /= weightSum;
            detailWeight /= weightSum;
        }
    }
    detailWeight = std::clamp(detailWeight, 0.12f, 0.55f);
    weightSum = baseWeight + detailWeight;
    if (weightSum > 0.0001f) {
        baseWeight = std::clamp(baseWeight / weightSum, 0.35f, 0.9f);
        detailWeight = 1.0f - baseWeight;
    }

    // Multi-layer noise tuned per terrain model with resilience modifiers
    float baseNoise = noise_.octaveNoise(static_cast<float>(worldX) * profile.baseFreq, static_cast<float>(worldZ) * profile.baseFreq, profile.baseOctaves, 0.55f);
    float detailNoise = noise_.octaveNoise(static_cast<float>(worldX) * profile.detailFreq, static_cast<float>(worldZ) * profile.detailFreq, profile.detailOctaves, 0.4f);
    float biomeNoise = noise_.noise2D(static_cast<float>(worldX) * profile.biomeFreq, static_cast<float>(worldZ) * profile.biomeFreq);

    float heightValue = baseNoise * baseWeight + detailNoise * detailWeight;
    heightValue = std::clamp(heightValue, 0.0f, 1.0f);

    surfaceBlock = BlockType::Grass;
    subBlock = BlockType::Dirt;
    int baseHeight = profile.plainsBaseHeight;
    int heightRange = plainsRange;

    if (biomeNoise > 0.45f) {
        // MOUNTAINS biome - cooler rock underneath, sharper peaks depending on model
        subBlock = BlockType::Stone;
        baseHeight = profile.mountainBaseHeight;
        heightRange = mountainRange;
        float curve = mountainCurve * (1.0f - corridorMask * 0.35f * derived.corridorStrength);
        curve = std::max(0.85f, curve);
        heightValue = std::pow(heightValue, curve);
    }

    if (corridorMask > 0.001f) {
        float flatten = derived.corridorStrength * corridorMask;
        float target = 0.52f; // Keep midlands near 0.5 for corridors/clareiras
        heightValue = heightValue * (1.0f - flatten) + target * flatten;
    }

    int height = baseHeight + static_cast<int>(heightValue * static_cast<float>(heightRange));
    height = std::max(10, std::min(Chunk::CHUNK_HEIGHT - 4, height));  // Clamp to [10, 60]
    return height;
}

float VoxelTerrain::moistureAt(int worldX, int worldZ) const {
    // Low-frequency noise for moisture mask
    return noise_.noise2D(static_cast<float>(worldX) * 0.005f, static_cast<float>(worldZ) * 0.005f) * 0.5f + 0.5f; // [0,1]
}

float VoxelTerrain::slopeDegAt(int worldX, int worldZ, int centerHeight) const {
    (void)centerHeight;
    BlockType dummySurface, dummySub;
    float hL = static_cast<float>(sampleHeight(worldX - 1, worldZ, dummySurface, dummySub));
    float hR = static_cast<float>(sampleHeight(worldX + 1, worldZ, dummySurface, dummySub));
    float hF = static_cast<float>(sampleHeight(worldX, worldZ + 1, dummySurface, dummySub));
    float hB = static_cast<float>(sampleHeight(worldX, worldZ - 1, dummySurface, dummySub));

    float dx = (hR - hL) * 0.5f;
    float dz = (hF - hB) * 0.5f;
    float slope = std::sqrt(dx * dx + dz * dz);
    return std::atan(slope) * 57.29578f; // rad->deg
}

VegetationClass VoxelTerrain::sampleVegetationAt(int worldX, int worldZ) const {
    int chunkX, chunkZ;
    chunkCoordsFromWorld(worldX, worldZ, chunkX, chunkZ);
    auto it = chunks_.find({chunkX, chunkZ});
    if (it == chunks_.end() || !it->second || !it->second->isGenerated()) {
        return VegetationClass::None;
    }
    int lx = worldX - chunkX * Chunk::CHUNK_SIZE;
    int lz = worldZ - chunkZ * Chunk::CHUNK_SIZE;
    if (lx < 0 || lx >= Chunk::CHUNK_SIZE || lz < 0 || lz >= Chunk::CHUNK_SIZE) return VegetationClass::None;

    // Look for leaves/wood along the column
    bool hasLeaves = false;
    bool hasWood = false;
    for (int y = Chunk::CHUNK_HEIGHT - 1; y >= 0; --y) {
        const Block* b = it->second->getBlock(lx, y, lz);
        if (!b) continue;
        if (b->type == BlockType::Leaves) hasLeaves = true;
        if (b->type == BlockType::Wood) hasWood = true;
        if (hasLeaves || hasWood) break;
    }
    if (hasLeaves || hasWood) return VegetationClass::Rich;

    // If no trees, but grass surface, mark sparse
    const Block* surface = it->second->getBlock(lx, 0, lz); // will be adjusted below
    int surfaceY = -1;
    for (int y = Chunk::CHUNK_HEIGHT - 1; y >= 0; --y) {
        const Block* b = it->second->getBlock(lx, y, lz);
        if (b && b->type != BlockType::Air && b->type != BlockType::Water) {
            surface = b;
            surfaceY = y;
            break;
        }
    }
    if (surface && surfaceY >= 0 && surface->type == BlockType::Grass) {
        return VegetationClass::Sparse;
    }
    return VegetationClass::None;
}

TerrainClass VoxelTerrain::classifyTerrain(int worldX, int worldZ, int centerHeight) const {
    float slopeDeg = slopeDegAt(worldX, worldZ, centerHeight);

    if (centerHeight > 48 || slopeDeg > 12.0f) {
        return TerrainClass::Mountain;
    } else if (slopeDeg < 3.0f && centerHeight < 38) {
        return TerrainClass::Flat;
    } else {
        return TerrainClass::Slope;
    }
}

bool VoxelTerrain::probeSurface(const math::Ray& ray, float maxDistance, SurfaceHit& outHit) const {
    float step = 0.5f;
    float traveled = 0.0f;
    float prevDelta = 0.0f;
    bool hasPrev = false;
    while (traveled <= maxDistance) {
        math::Vec3 p = ray.origin + ray.direction * traveled;
        int wx = static_cast<int>(std::floor(p.x));
        int wz = static_cast<int>(std::floor(p.z));
        BlockType surfaceBlock, subBlock;
        int h = sampleHeight(wx, wz, surfaceBlock, subBlock);
        float surfaceY = static_cast<float>(h);
        float delta = p.y - surfaceY;

        if (hasPrev && delta <= 0.0f && prevDelta > 0.0f) {
            outHit.worldX = wx;
            outHit.worldZ = wz;
            outHit.worldY = h;
            outHit.surfaceBlock = surfaceBlock;

            // Use stored classification if chunk is loaded/generated
            int cx, cz;
            chunkCoordsFromWorld(wx, wz, cx, cz);
            std::shared_ptr<Chunk> hitChunk;
            {
                std::lock_guard<std::mutex> lock(chunksMutex_);
                auto it = chunks_.find({cx, cz});
                if (it != chunks_.end()) hitChunk = it->second;
            }
            if (hitChunk && hitChunk->isGenerated()) {
                int lx = wx - cx * Chunk::CHUNK_SIZE;
                int lz = wz - cz * Chunk::CHUNK_SIZE;
                lx = std::max(0, std::min(lx, Chunk::CHUNK_SIZE - 1));
                lz = std::max(0, std::min(lz, Chunk::CHUNK_SIZE - 1));
                outHit.terrainClass = hitChunk->terrainClass(lx, lz);
            } else {
                outHit.terrainClass = classifyTerrain(wx, wz, h);
            }
            outHit.slopeDeg = slopeDegAt(wx, wz, h);
            outHit.moisture = moistureAt(wx, wz);
            outHit.vegetation = sampleVegetationAt(wx, wz);
            outHit.valid = true;
            return true;
        }

        hasPrev = true;
        prevDelta = delta;
        traveled += step;
    }

    outHit.valid = false;
    return false;
}

// Header change is needed first!
// ... implementation update ...
void VoxelTerrain::update(float cameraX, float cameraZ, const math::Frustum& frustum, int frameIndex) {
    gcFrameResources();

    // Process garbage from workers safely
    VkFence currentFence = (frameFences_ && frameIndex >= 0 && frameIndex < static_cast<int>(frameFences_->size())) ? (*frameFences_)[frameIndex] : VK_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> gLock(garbageMutex_);
        if (!meshGarbage_.empty()) {
            for (auto& res : meshGarbage_) {
                deferredResources_.push_back({currentFence, res});
            }
            meshGarbage_.clear();
        }
    }

    int centerChunkX, centerChunkZ;
    chunkCoordsFromWorld(static_cast<int>(cameraX), static_cast<int>(cameraZ), centerChunkX, centerChunkZ);

    auto chunksSnapshot = snapshotChunks();
    processPendingVegetation();

    // Vegetation regen - ... (keep existing logic) ...
    for (auto& chunk : chunksSnapshot) {
        if (chunk && chunk->isGenerated() && chunk->vegetationVersion() != vegetationVersion_) {
            if (safeMode_) {
                queueVegetationRegen(chunk);
            } else {
                refreshVegetation(chunk.get());
                chunk->setVegetationVersion(vegetationVersion_);
                chunk->markDirty();
            }
        }
    }

    // Progressive chunk loading with Priority Queue
    int chunksQueued = 0;
    const int MAX_CHUNKS_PER_FRAME = maxChunksPerFrame_;
    
    struct ChunkCandidate {
        int x, z;
        float distSq;
        bool visible;
    };
    std::vector<ChunkCandidate> candidates;
    candidates.reserve((viewDistance_ * 2 + 1) * (viewDistance_ * 2 + 1));

    for (int cx = centerChunkX - viewDistance_; cx <= centerChunkX + viewDistance_; cx++) {
        for (int cz = centerChunkZ - viewDistance_; cz <= centerChunkZ + viewDistance_; cz++) {
            // Check if already exists
            std::shared_ptr<Chunk> chunk;
            {
                std::lock_guard<std::mutex> lock(chunksMutex_);
                auto it = chunks_.find({cx, cz});
                if (it != chunks_.end()) chunk = it->second;
            }
            if (chunk) continue; // Already loaded

            // Check visibility
            float wx = static_cast<float>(cx * Chunk::CHUNK_SIZE);
            float wz = static_cast<float>(cz * Chunk::CHUNK_SIZE);
            math::AABB aabb(wx, 0.0f, wz, wx + Chunk::CHUNK_SIZE, Chunk::CHUNK_HEIGHT, wz + Chunk::CHUNK_SIZE);
            bool visible = math::testAABBFrustum(aabb, frustum);
            
            float dx = static_cast<float>(cx - centerChunkX);
            float dz = static_cast<float>(cz - centerChunkZ);
            candidates.push_back({cx, cz, dx*dx + dz*dz, visible});
        }
    }

    // Sort: Visible first, then by distance
    std::sort(candidates.begin(), candidates.end(), [](const ChunkCandidate& a, const ChunkCandidate& b) {
        if (a.visible != b.visible) return a.visible; // prioritize true > false
        return a.distSq < b.distSq; // closest first
    });

    auto pendingCount = [this]() {
        std::lock_guard<std::mutex> lockA(chunksMutex_);
        std::lock_guard<std::mutex> lockB(taskMutex_);
        return pendingGenerate_.size() + pendingMesh_.size() + taskQueue_.size();
    };

    for (const auto& cand : candidates) {
        if (pendingCount() >= maxPendingTasks_) break;
        if (chunksQueued >= MAX_CHUNKS_PER_FRAME) break;
        
        // If invisible and we have plenty of visible tasks pending, maybe skip/throttle invisible?
        // For now, simple priority is enough.
        
        getChunk(cand.x, cand.z, /*async*/true);
        chunksQueued++;
    }


    // Queue mesh rebuilds for dirty chunks respecting budget
    int meshesQueued = 0;
    const int MAX_MESHES_PER_FRAME = maxMeshesPerFrame_;
    
    struct DirtyChunk {
        std::shared_ptr<Chunk> chunk;
        float distSq;
        bool visible;
    };
    std::vector<DirtyChunk> dirtyChunks;
    dirtyChunks.reserve(chunksSnapshot.size());

    for (auto& chunk : chunksSnapshot) {
        if (!chunk || !chunk->isGenerated() || !chunk->isDirty()) continue;

        float dx = static_cast<float>(chunk->getWorldX() - centerChunkX);
        float dz = static_cast<float>(chunk->getWorldZ() - centerChunkZ);
        bool visible = math::testAABBFrustum(chunk->getAABB(), frustum);
        dirtyChunks.push_back({chunk, dx*dx + dz*dz, visible});
    }

    std::sort(dirtyChunks.begin(), dirtyChunks.end(), [](const DirtyChunk& a, const DirtyChunk& b) {
        if (a.visible != b.visible) return a.visible; // prioritize true > false
        return a.distSq < b.distSq; // closest first
    });

    for (const auto& item : dirtyChunks) {
        if (pendingCount() >= maxPendingTasks_) break;
        if (meshesQueued >= MAX_MESHES_PER_FRAME) break;
        enqueueMesh(item.chunk);
        meshesQueued++;
    }

    pruneFarChunks(centerChunkX, centerChunkZ, frameIndex);
    // Attempt to reclaim resources tied to completed frames
    gcFrameResources();
}

void VoxelTerrain::setVegetationEnabled(bool enabled) {
    if (vegetationEnabled_.load() == enabled) return;
    vegetationEnabled_.store(enabled);
    if (safeMode_) {
        clearPendingTasks();
        auto chunks = snapshotChunks();
        for (auto& c : chunks) {
            queueVegetationRegen(c);
        }
        return;
    }
    for (auto& chunk : snapshotChunks()) {
        if (chunk && chunk->isGenerated()) {
            refreshVegetation(chunk.get());
            chunk->markDirty();
        }
    }
}

void VoxelTerrain::setVegetationDensity(float d) {
    float clamped = std::max(0.0f, std::min(d, 2.0f));
    if (std::abs(clamped - vegetationDensity_) < 0.001f) return;
    vegetationDensity_ = clamped;
    if (safeMode_) {
        clearPendingTasks();
        auto chunks = snapshotChunks();
        for (auto& c : chunks) {
            queueVegetationRegen(c);
        }
        return;
    }
    for (auto& chunk : snapshotChunks()) {
        if (chunk && chunk->isGenerated()) {
            refreshVegetation(chunk.get());
            chunk->markDirty();
        }
    }
}

void VoxelTerrain::setTerrainModel(TerrainModel model) {
    if (terrainModel_ == model) return;
    terrainModel_ = model;
    vegetationVersion_++; // ensure vegetation regens on new heights
    if (safeMode_) {
        clearPendingTasks();
        auto chunks = snapshotChunks();
        for (auto& c : chunks) {
            queueVegetationRegen(c);
        }
    }
}

void VoxelTerrain::setSafeMode(bool enabled) {
    safeMode_ = enabled;
}

VoxelTerrain::ResilienceDerived VoxelTerrain::resilienceDerived() const {
    ResilienceDerived derived;
    derived.ecol = std::clamp(resEcol_.load(std::memory_order_relaxed), 0.0f, 1.0f);
    derived.prod = std::clamp(resProd_.load(std::memory_order_relaxed), 0.0f, 1.0f);
    derived.soc = std::clamp(resSoc_.load(std::memory_order_relaxed), 0.0f, 1.0f);

    derived.baseFreqMul = 0.8f + derived.ecol * 0.45f;           // [0.8, 1.25]
    derived.detailFreqMul = 0.7f + derived.ecol * 0.7f;          // [0.7, 1.4]
    derived.detailWeightMul = 0.65f + derived.ecol * 0.7f;       // [0.65, 1.35]
    derived.plainsRangeMul = 0.75f + derived.ecol * 0.65f;       // [0.75, 1.4]
    derived.mountainRangeMul = 0.8f + derived.ecol * 0.6f;       // [0.8, 1.4]
    derived.mountainCurveMul = 0.95f + derived.ecol * 0.25f;     // [0.95, 1.2]

    derived.fertilityMoistureWeight = 0.55f + derived.prod * 0.25f; // [0.55, 0.8]
    derived.fertilityHeightWeight = 1.0f - derived.fertilityMoistureWeight;
    derived.fertilityBias = (derived.prod - 0.5f) * 0.25f;       // [-0.125, +0.125]

    derived.corridorSpacing = 18.0f + (1.0f - derived.soc) * 12.0f; // [18, 30]
    derived.corridorStrength = derived.soc * 0.85f;              // cap flattening
    derived.smoothingFactor = 0.15f + derived.soc * 0.35f;       // [0.15, 0.5]
    return derived;
}

float VoxelTerrain::socialCorridorMask(int worldX, int worldZ, const ResilienceDerived& derived) const {
    if (derived.corridorStrength <= 0.001f) return 0.0f;

    float spacing = derived.corridorSpacing;
    float dx = std::fmod(std::fabs(static_cast<float>(worldX)), spacing);
    float dz = std::fmod(std::fabs(static_cast<float>(worldZ)), spacing);
    dx = std::min(dx, spacing - dx);
    dz = std::min(dz, spacing - dz);
    float nearest = std::min(dx, dz);

    float falloffRadius = spacing * 0.6f + 0.0001f;
    float falloff = std::exp(- (nearest * nearest) / (falloffRadius * falloffRadius));
    float jitter = noise_.noise2D(static_cast<float>(worldX) * 0.03f, static_cast<float>(worldZ) * 0.03f) * 0.5f + 0.5f;
    float mask = falloff * 0.7f + jitter * 0.3f;
    return std::clamp(mask, 0.0f, 1.0f);
}

void VoxelTerrain::logResilienceState(const char* reason) const {
    auto d = resilienceDerived();
    std::cout << "[Resilience] (" << (reason ? reason : "update") << ") "
              << "Ecol=" << d.ecol
              << " Prod=" << d.prod
              << " Soc=" << d.soc
              << " | freqMul=" << d.baseFreqMul << "/" << d.detailFreqMul
              << " | rangeMul P/M=" << d.plainsRangeMul << "/" << d.mountainRangeMul
              << " | fertilityW=" << d.fertilityMoistureWeight
              << " | corridor spacing=" << d.corridorSpacing
              << " strength=" << d.corridorStrength
              << std::endl;
}

void VoxelTerrain::setResilienceEcol(float v) {
    float clamped = std::clamp(v, 0.0f, 1.0f);
    float prev = resEcol_.load(std::memory_order_relaxed);
    if (std::abs(prev - clamped) < 0.0001f) return;
    resEcol_.store(clamped, std::memory_order_relaxed);
    vegetationVersion_++; // Trees/grass palettes depend on ecology
    logResilienceState("resEcol");
}

void VoxelTerrain::setResilienceProd(float v) {
    float clamped = std::clamp(v, 0.0f, 1.0f);
    float prev = resProd_.load(std::memory_order_relaxed);
    if (std::abs(prev - clamped) < 0.0001f) return;
    resProd_.store(clamped, std::memory_order_relaxed);
    vegetationVersion_++; // Fertility alters vegetation thresholds
    logResilienceState("resProd");
}

void VoxelTerrain::setResilienceSoc(float v) {
    float clamped = std::clamp(v, 0.0f, 1.0f);
    float prev = resSoc_.load(std::memory_order_relaxed);
    if (std::abs(prev - clamped) < 0.0001f) return;
    resSoc_.store(clamped, std::memory_order_relaxed);
    vegetationVersion_++; // Corridors clear vegetation
    logResilienceState("resSoc");
}

void VoxelTerrain::queueVegetationRegen(const std::shared_ptr<Chunk>& chunk) {
    if (!chunk) return;
    auto key = std::make_pair(chunk->getWorldX(), chunk->getWorldZ());
    std::lock_guard<std::mutex> lock(vegMutex_);
    if (pendingVegRegenKeys_.count(key)) return;
    pendingVegRegenKeys_.insert(key);
    pendingVegRegen_.push_back(chunk);
}

void VoxelTerrain::processPendingVegetation() {
    std::vector<std::shared_ptr<Chunk>> batch;
    {
        std::lock_guard<std::mutex> lock(vegMutex_);
        size_t count = 0;
        while (!pendingVegRegen_.empty() && count < vegRegenPerFrame_) {
            auto c = pendingVegRegen_.back();
            pendingVegRegen_.pop_back();
            auto key = std::make_pair(c->getWorldX(), c->getWorldZ());
            pendingVegRegenKeys_.erase(key);
            batch.push_back(c);
            ++count;
        }
    }
    for (auto& c : batch) {
        enqueueVegetation(c);
    }
}

void VoxelTerrain::flushDeferredDestroy(bool force) {
    std::lock_guard<std::mutex> lock(garbageMutex_);
    if (deferredResources_.empty()) return;
    if (force) {
        deferredResources_.clear();
        meshGarbage_.clear();
        lastDestroyFlush_ = std::chrono::steady_clock::now();
    }
}

void VoxelTerrain::gcFrameResources() {
    std::lock_guard<std::mutex> lock(garbageMutex_);
    if (deferredResources_.empty()) return;

    // Iterate through ALL resources and check if their fence is signaled.
    // Do NOT assume FIFO order.
    auto it = deferredResources_.begin();
    while (it != deferredResources_.end()) {
        bool unsafe = true;
        
        // If fence refers to a resource not tied to any frame (VK_NULL_HANDLE), we can destroy immediately IF we assume it's not used. 
        // BUT, for safety in this context, we treat null fence as "safe" only if we are sure (e.g. force flush).
        // Actually, if fence is null, it means no specific frame protects it, so it might be safe?
        // Let's assume Valid Fence -> check status. Null Fence -> safe (or error in logic, but let's assume safe).
        
        VkFence fence = it->first;
        if (fence == VK_NULL_HANDLE) {
            unsafe = false;
        } else {
            VkResult status = vkGetFenceStatus(context_.device(), fence);
            if (status == VK_SUCCESS) {
                unsafe = false;
            }
        }

        if (!unsafe) {
            // Fence signaled or null, safe to destroy
            it = deferredResources_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Optional: Log warning if list grows too large
    if (deferredResources_.size() > 500) {
        // std::cerr << "[VoxelTerrain] Warning: Deferred destroy list size: " << deferredResources_.size() << std::endl;
    }
}

void VoxelTerrain::waitForWorkersIdle() {
    for (;;) {
        int active = activeTasks_.load(std::memory_order_relaxed);
        bool emptyQueue = false;
        {
            std::lock_guard<std::mutex> lock(taskMutex_);
            emptyQueue = taskQueue_.empty();
        }
        if (active == 0 && emptyQueue) {
            return;
        }
        std::unique_lock<std::mutex> lock(taskMutex_);
        taskCv_.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void VoxelTerrain::generateChunk(Chunk* chunk) {
    int chunkWorldX = chunk->getWorldX() * Chunk::CHUNK_SIZE;
    int chunkWorldZ = chunk->getWorldZ() * Chunk::CHUNK_SIZE;
    ResilienceDerived derived = resilienceDerived();
    float fertilityMap[Chunk::CHUNK_SIZE][Chunk::CHUNK_SIZE] = {};

    constexpr int kWaterLevel = 28; // elevar o nível para garantir lagos visíveis
    for (int x = 0; x < Chunk::CHUNK_SIZE; x++) {
        for (int z = 0; z < Chunk::CHUNK_SIZE; z++) {
            int worldX = chunkWorldX + x;
            int worldZ = chunkWorldZ + z;

            BlockType surfaceBlock = BlockType::Grass;
            BlockType subBlock = BlockType::Dirt;
            int height = sampleHeight(worldX, worldZ, surfaceBlock, subBlock);
            TerrainClass tClass = classifyTerrain(worldX, worldZ, height);
            chunk->setTerrainClass(x, z, tClass);
            float moisture = moistureAt(worldX, worldZ);
            float normHeight = static_cast<float>(height) / static_cast<float>(Chunk::CHUNK_HEIGHT);
            float fertility = derived.fertilityMoistureWeight * moisture +
                              derived.fertilityHeightWeight * (1.0f - normHeight) +
                              derived.fertilityBias;
            fertility = std::clamp(fertility, 0.0f, 1.0f);
            fertilityMap[x][z] = fertility;

            if (fertility < 0.2f) {
                surfaceBlock = BlockType::Dirt; // áreas pobres ficam sem grama
            }

            // Fill column
            for (int y = 0; y < Chunk::CHUNK_HEIGHT; y++) {
                BlockType type = BlockType::Air;

                if (y < height - 4) {
                    type = BlockType::Stone;  // Deep underground - always stone
                    if (fertility > 0.65f && y >= height - 6 && y < height - 3) {
                        type = BlockType::Dirt; // camada fértil extra
                    }
                } else if (y < height - 1) {
                    type = subBlock;   // Subsurface - biome dependent
                    if (fertility > 0.65f && type == BlockType::Stone) {
                        type = BlockType::Dirt;
                    }
                } else if (y < height) {
                    type = surfaceBlock;  // Surface - biome dependent
                } else if (y <= kWaterLevel) {  // Water level elevado para ser visível
                    type = BlockType::Water;
                }

                chunk->setBlock(x, y, z, type);
            }
        }
    }
    
    // Generate Trees (Simple) - density varies with terrain class
    // Only generate in this chunk to prevent thread safety issues with neighbors for now
    if (vegetationEnabled_) {
        for (int x = 2; x < Chunk::CHUNK_SIZE - 2; x++) {  // Avoid edges
            for (int z = 2; z < Chunk::CHUNK_SIZE - 2; z++) {
                // Chance for tree
                float treeNoise = noise_.noise2D(static_cast<float>(chunkWorldX + x), static_cast<float>(chunkWorldZ + z)); // Deterministic
                TerrainClass tClass = chunk->terrainClass(x, z);
                
                // Define thresholds per terrain class with moisture & density
                float moisture = moistureAt(chunkWorldX + x, chunkWorldZ + z);
                float density = vegetationDensity_;
                float fertility = fertilityMap[x][z];
                float corridorMask = socialCorridorMask(chunkWorldX + x, chunkWorldZ + z, derived);

                float treeThreshold = 1.1f; // default: no trees
                if (tClass == TerrainClass::Flat) {
                    float densityBias = (density - 1.0f) * 0.12f;
                    float moistureBias = (moisture - 0.5f) * 0.18f;
                    float fertilityBias = (fertility - 0.5f) * (0.25f + derived.prod * 0.15f);
                    float corridorPenalty = corridorMask * derived.corridorStrength * 0.6f;
                    treeThreshold = 0.55f - densityBias - moistureBias - fertilityBias + corridorPenalty;
                } else if (tClass == TerrainClass::Slope) {
                    float densityBias = (density - 1.0f) * 0.1f;
                    float moistureBias = (moisture - 0.5f) * 0.12f;
                    float fertilityBias = (fertility - 0.5f) * 0.2f;
                    float corridorPenalty = corridorMask * derived.corridorStrength * 0.65f;
                    treeThreshold = 0.3f - densityBias - moistureBias - fertilityBias + corridorPenalty;
                } else if (tClass == TerrainClass::Mountain) {
                    float corridorPenalty = corridorMask * derived.corridorStrength * 0.45f;
                    treeThreshold = 1.2f + corridorPenalty; // effectively disable
                }
                treeThreshold = std::clamp(treeThreshold, -0.25f, 1.25f);

                if (treeNoise > treeThreshold) { // Higher noise = more trees as threshold lowers
                    // Find surface height
                    int surfaceY = -1;
                    for(int y = Chunk::CHUNK_HEIGHT - 1; y >= 0; y--) {
                        Block* b = chunk->getBlock(x, y, z);
                        if (b && b->type != BlockType::Air && b->type != BlockType::Water) {
                            surfaceY = y;
                            break;
                        }
                    }
                    
                    if (surfaceY > 0 && surfaceY < Chunk::CHUNK_HEIGHT - 10) {
                         Block* b = chunk->getBlock(x, surfaceY, z);
                         // Trees only on Grass
                         if (b && b->type == BlockType::Grass) {
                             int treeH = 4 + static_cast<int>(treeNoise * 2.0f) + static_cast<int>(fertility * 2.0f);
                             treeH = std::max(3, std::min(treeH, 8));
                             generateTree(chunkWorldX + x, surfaceY + 1, chunkWorldZ + z, treeH);
                         }
                    }
                }
            }
        }
    }

    // Stamp vegetation version used
    chunk->setVegetationVersion(vegetationVersion_);

    chunk->markGenerated();
}

int VoxelTerrain::getTerrainHeight(int worldX, int worldZ) const {
    BlockType surface, sub;
    return sampleHeight(worldX, worldZ, surface, sub);
}

Block* VoxelTerrain::getBlock(int worldX, int worldY, int worldZ) {
    int chunkX, chunkZ;
    chunkCoordsFromWorld(worldX, worldZ, chunkX, chunkZ);
    
    auto chunk = getChunk(chunkX, chunkZ, /*async*/false);
    if (!chunk) return nullptr;

    int localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    int localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;
    return chunk->getBlock(localX, worldY, localZ);
}

void VoxelTerrain::setBlock(int worldX, int worldY, int worldZ, BlockType type) {
    int chunkX, chunkZ;
    chunkCoordsFromWorld(worldX, worldZ, chunkX, chunkZ);
    
    auto chunk = getChunk(chunkX, chunkZ, /*async*/false);
    if (!chunk) return;

    int localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    int localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;
    chunk->setBlock(localX, worldY, localZ, type);
}

Block* VoxelTerrain::getBlockIfLoaded(int worldX, int worldY, int worldZ) {
    int chunkX, chunkZ;
    chunkCoordsFromWorld(worldX, worldZ, chunkX, chunkZ);
    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(chunksMutex_);
        auto it = chunks_.find({chunkX, chunkZ});
        if (it == chunks_.end()) return nullptr;
        chunk = it->second;
    }
    if (!chunk) return nullptr;
    int localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    int localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;
    if (localX < 0 || localX >= Chunk::CHUNK_SIZE || localZ < 0 || localZ >= Chunk::CHUNK_SIZE) {
        return nullptr;
    }
    if (worldY < 0 || worldY >= Chunk::CHUNK_HEIGHT) return nullptr;
    return chunk->getBlock(localX, worldY, localZ);
}

std::vector<Chunk*> VoxelTerrain::getVisibleChunks(const math::Frustum& frustum) {
    std::vector<Chunk*> visible;
    std::lock_guard<std::mutex> lock(chunksMutex_);
    for (auto& pair : chunks_) {
        Chunk* chunk = pair.second.get();
        if (chunk && math::testAABBFrustum(chunk->getAABB(), frustum)) {
            visible.push_back(chunk);
        }
    }
    return visible;
}

int VoxelTerrain::chunkCount() {
    std::lock_guard<std::mutex> lock(chunksMutex_);
    return static_cast<int>(chunks_.size());
}

int VoxelTerrain::pendingTaskCount() {
    std::lock_guard<std::mutex> lock(chunksMutex_);
    std::lock_guard<std::mutex> lock2(taskMutex_);
    return static_cast<int>(pendingGenerate_.size() + pendingMesh_.size() + taskQueue_.size());
}

int VoxelTerrain::pendingVegetationCount() {
    std::lock_guard<std::mutex> lock(vegMutex_);
    return static_cast<int>(pendingVegRegen_.size());
}

void VoxelTerrain::generateTree(int worldX, int worldY, int worldZ, int height) {
    // Safety check: don't generate if too close to chunk height limit
    if (worldY + height + 3 >= Chunk::CHUNK_HEIGHT) return;
    if (worldY < 0) return;
    
    // Get the chunk this tree belongs to
    int chunkX, chunkZ;
    chunkCoordsFromWorld(worldX, worldZ, chunkX, chunkZ);
    auto chunkPtr = getChunk(chunkX, chunkZ, /*async*/false);
    if (!chunkPtr) return;
    Chunk* chunk = chunkPtr.get();
    
    // Convert to local coordinates
    int localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    int localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;
    
    // Trunk (vertical column of wood) - only in this chunk
    for (int y = 0; y < height; y++) {
        chunk->setBlock(localX, worldY + y, localZ, BlockType::Wood);
    }
    
    // Leaves (SMALL sphere-like canopy) - only within chunk bounds
    int canopyY = worldY + height - 1;
    int canopyRadius = 1;  // Smaller radius to stay in chunk
    
    for (int dx = -canopyRadius; dx <= canopyRadius; dx++) {
        for (int dy = 0; dy <= canopyRadius + 1; dy++) {
            for (int dz = -canopyRadius; dz <= canopyRadius; dz++) {
                // Skip the trunk center
                if (dx == 0 && dz == 0 && dy == 0) continue;
                
                int lx = localX + dx;
                int ly = canopyY + dy;
                int lz = localZ + dz;
                
                // CRITICAL: Stay within chunk bounds
                if (lx < 0 || lx >= Chunk::CHUNK_SIZE) continue;
                if (lz < 0 || lz >= Chunk::CHUNK_SIZE) continue;
                if (ly < 0 || ly >= Chunk::CHUNK_HEIGHT) continue;
                
                // Rough sphere shape
                float distance = std::sqrt(static_cast<float>(dx*dx + dy*dy + dz*dz));
                if (distance <= static_cast<float>(canopyRadius) + 0.8f) {
                    const Block* existing = chunk->getBlock(lx, ly, lz);
                    if (existing && existing->type == BlockType::Air) {
                        chunk->setBlock(lx, ly, lz, BlockType::Leaves);
                    }
                }
            }
        }
    }
}

void VoxelTerrain::getBlockColor(BlockType type, float& r, float& g, float& b) {
    switch (type) {
        case BlockType::Grass:
            r = 0.3f; g = 0.8f; b = 0.3f; break;  // Vibrant green
        case BlockType::Dirt:
            r = 0.55f; g = 0.35f; b = 0.2f; break; // Rich brown
        case BlockType::Stone:
            r = 0.6f; g = 0.6f; b = 0.65f; break; // Cool gray
        case BlockType::Sand:
            r = 0.95f; g = 0.9f; b = 0.6f; break;  // Warm sand
        case BlockType::Water:
            r = 0.2f; g = 0.5f; b = 0.9f; break;  // Deep blue
        case BlockType::Wood:
            r = 0.4f; g = 0.3f; b = 0.15f; break; // Dark wood
        case BlockType::Leaves:
            r = 0.2f; g = 0.6f; b = 0.2f; break;  // Forest leaves
        default:
            r = 1.0f; g = 0.0f; b = 1.0f; // Magenta
    }
}

bool VoxelTerrain::shouldRenderFace(int x, int y, int z, int nx, int ny, int nz) {
    (void)x; (void)y; (void)z;
    Block* neighbor = getBlockIfLoaded(nx, ny, nz);
    if (!neighbor) return true;  // Render if outside world or neighbor chunk not yet loaded
    return !neighbor->isSolid();  // Render if neighbor is transparent
}


void VoxelTerrain::rebuildChunkMesh(Chunk* chunk) {
    std::lock_guard<std::mutex> meshLock(meshMutex_);
    std::vector<Vertex> verticesSolid;
    std::vector<uint16_t> indicesSolid;
    std::vector<Vertex> verticesWater;
    std::vector<uint16_t> indicesWater;
    ResilienceDerived derived = resilienceDerived();
    bool vegEnabled = vegetationEnabled_.load();

    const int chunkWorldX = chunk->getWorldX() * Chunk::CHUNK_SIZE;
    const int chunkWorldZ = chunk->getWorldZ() * Chunk::CHUNK_SIZE;
    const int dims[3] = {Chunk::CHUNK_SIZE, Chunk::CHUNK_HEIGHT, Chunk::CHUNK_SIZE};

    auto colorForBlock = [&](const Block& block, int lx, int ly, int lz) -> std::array<float,3> {
        (void)ly;
        float r, g, b;
        
        // VISUALIZATION TWEAK: If vegetation is disabled, render Grass as Dirt to mimic barren soil
        BlockType typeForColor = block.type;
        if (!vegEnabled && typeForColor == BlockType::Grass) {
            typeForColor = BlockType::Dirt;
        }

        getBlockColor(typeForColor, r, g, b);
        TerrainClass tClass = chunk->terrainClass(lx, lz);
        float corridorMask = 0.0f;
        if (block.type == BlockType::Grass || block.type == BlockType::Leaves) {
            corridorMask = socialCorridorMask(chunkWorldX + lx, chunkWorldZ + lz, derived);
        }
        
        // Only apply grass coloring tweaks if we are still treating it as grass (i.e. Veg On)
        if (typeForColor == BlockType::Grass) {
            float moisture = moistureAt(chunkWorldX + lx, chunkWorldZ + lz);
            if (tClass == TerrainClass::Slope) {
                r *= 0.9f; g *= 0.9f; b *= 0.93f;
            } else if (tClass == TerrainClass::Mountain) {
                r *= 0.75f; g *= 0.7f; b *= 0.78f;
            }
            r *= (0.9f + moisture * 0.2f);
            g *= (0.95f + moisture * 0.1f);
            if (corridorMask > 0.0f) {
                float corridorTint = 1.0f - 0.15f * corridorMask * derived.corridorStrength;
                r *= corridorTint; g *= corridorTint; b *= corridorTint;
            }
        } else if (typeForColor == BlockType::Leaves) {
            if (tClass == TerrainClass::Mountain) {
                r *= 0.85f; g *= 0.8f; b *= 0.85f;
            }
            if (corridorMask > 0.0f) {
                float corridorTint = 1.0f - 0.1f * corridorMask * derived.corridorStrength;
                r *= corridorTint; g *= corridorTint; b *= corridorTint;
            }
        }
        return {r, g, b};
    };

    struct FaceCell {
        bool present = false;
        float r = 0, g = 0, b = 0;
        float nx = 0, ny = 0, nz = 0;
    };

    auto sameCell = [](const FaceCell& a, const FaceCell& b) {
        return a.present == b.present &&
               a.r == b.r && a.g == b.g && a.b == b.b &&
               a.nx == b.nx && a.ny == b.ny && a.nz == b.nz;
    };

    auto runGreedy = [&](bool waterPass, std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIdx) {
        auto appendQuad = [&](float base[3], float du[3], float dv[3], const FaceCell& cell, bool invertWinding) {
            uint16_t baseIdx = static_cast<uint16_t>(outVerts.size());
            Vertex v0{}, v1{}, v2{}, v3{};
            v0.pos[0] = base[0];             v0.pos[1] = base[1];             v0.pos[2] = base[2];
            v1.pos[0] = base[0] + dv[0];     v1.pos[1] = base[1] + dv[1];     v1.pos[2] = base[2] + dv[2];
            v2.pos[0] = base[0] + du[0] + dv[0]; v2.pos[1] = base[1] + du[1] + dv[1]; v2.pos[2] = base[2] + du[2] + dv[2];
            v3.pos[0] = base[0] + du[0];     v3.pos[1] = base[1] + du[1];     v3.pos[2] = base[2] + du[2];
            for (Vertex* v : {&v0, &v1, &v2, &v3}) {
                v->color[0] = cell.r;
                v->color[1] = cell.g;
                v->color[2] = cell.b;
                v->normal[0] = cell.nx;
                v->normal[1] = cell.ny;
                v->normal[2] = cell.nz;
            }
            outVerts.push_back(v0);
            outVerts.push_back(v1);
            outVerts.push_back(v2);
            outVerts.push_back(v3);

            if (!invertWinding) {
                outIdx.push_back(baseIdx + 0);
                outIdx.push_back(baseIdx + 1);
                outIdx.push_back(baseIdx + 2);
                outIdx.push_back(baseIdx + 0);
                outIdx.push_back(baseIdx + 2);
                outIdx.push_back(baseIdx + 3);
            } else {
                outIdx.push_back(baseIdx + 0);
                outIdx.push_back(baseIdx + 2);
                outIdx.push_back(baseIdx + 1);
                outIdx.push_back(baseIdx + 0);
                outIdx.push_back(baseIdx + 3);
                outIdx.push_back(baseIdx + 2);
            }
        };

        for (int d = 0; d < 3; ++d) {
            int u = (d + 1) % 3;
            int v = (d + 2) % 3;
            int uSize = dims[u];
            int vSize = dims[v];
            const size_t uSizeSz = static_cast<size_t>(uSize);
            std::vector<FaceCell> mask(uSizeSz * static_cast<size_t>(vSize));
            auto maskIndex = [uSizeSz](int i, int j) {
                return static_cast<size_t>(i) + static_cast<size_t>(j) * uSizeSz;
            };

            for (int dirSign : {1, -1}) {
                float nx = (d == 0 ? static_cast<float>(dirSign) : 0.0f);
                float ny = (d == 1 ? static_cast<float>(dirSign) : 0.0f);
                float nz = (d == 2 ? static_cast<float>(dirSign) : 0.0f);
                float brightness = 0.85f;
                if (d == 1 && dirSign == 1) brightness = 1.0f;
                else if (d == 1 && dirSign == -1) brightness = 0.6f;
                else if (d == 0) brightness = 0.75f;

                for (int q = 0; q <= dims[d]; ++q) {
                    std::fill(mask.begin(), mask.end(), FaceCell{});

                    for (int j = 0; j < vSize; ++j) {
                        for (int i = 0; i < uSize; ++i) {
                            int coord[3] = {0,0,0};
                            int neigh[3] = {0,0,0};
                            coord[u] = i; coord[v] = j; coord[d] = (dirSign == 1 ? q - 1 : q);
                            neigh[u] = i; neigh[v] = j; neigh[d] = coord[d] + dirSign;

                            if (coord[d] < 0 || coord[d] >= dims[d]) continue;
                            const Block* blk = chunk->getBlock(coord[0], coord[1], coord[2]);
                            if (!blk) continue;
                            bool isWater = blk->type == BlockType::Water;
                            bool renderable = waterPass ? isWater : blk->isSolid();
                            if (!renderable) continue;
                            if (!vegEnabled && (blk->type == BlockType::Wood || blk->type == BlockType::Leaves)) continue;

                            int nwx = chunkWorldX + neigh[0];
                            int nwy = neigh[1];
                            int nwz = chunkWorldZ + neigh[2];
                            const Block* nb = getBlockIfLoaded(nwx, nwy, nwz);
                            bool renderFace;
                            if (waterPass) {
                                renderFace = (!nb || nb->type != BlockType::Water);
                            } else {
                                renderFace = (!nb || !nb->isSolid());
                            }
                            if (!renderFace) continue;

                            auto color = colorForBlock(*blk, coord[0], coord[1], coord[2]);
                            // Per-face AO: count solid neighbors (for water, only solids/non-water)
                            const int offsets[6][3] = {
                                { 1, 0, 0}, {-1, 0, 0},
                                { 0, 1, 0}, { 0,-1, 0},
                                { 0, 0, 1}, { 0, 0,-1}
                            };
                            int solidCount = 0;
                            for (int oi = 0; oi < 6; ++oi) {
                                int od = (oi / 2);
                                int sign = (oi % 2 == 0) ? 1 : -1;
                                if (od == d && sign == dirSign) continue;
                                int nLocal[3] = {coord[0] + offsets[oi][0], coord[1] + offsets[oi][1], coord[2] + offsets[oi][2]};
                                int nWorld[3] = {chunkWorldX + nLocal[0], nLocal[1], chunkWorldZ + nLocal[2]};
                                const Block* nb2 = getBlockIfLoaded(nWorld[0], nWorld[1], nWorld[2]);
                                if (nb2) {
                                    if (waterPass) {
                                        if (nb2->type != BlockType::Water && nb2->isSolid()) solidCount++;
                                    } else {
                                        if (nb2->isSolid()) solidCount++;
                                    }
                                }
                            }
                            float ao = 1.0f - 0.08f * static_cast<float>(solidCount);
                            ao = std::clamp(ao, 0.55f, 1.0f);

                            FaceCell cell;
                            cell.present = true;
                            cell.r = color[0] * brightness * ao;
                            cell.g = color[1] * brightness * ao;
                            cell.b = color[2] * brightness * ao;
                            cell.nx = nx; cell.ny = ny; cell.nz = nz;
                            mask[maskIndex(i, j)] = cell;
                        }
                    }

                    for (int j = 0; j < vSize; ++j) {
                        for (int i = 0; i < uSize; ) {
                            FaceCell cell = mask[maskIndex(i, j)];
                            if (!cell.present) { ++i; continue; }

                            int width = 1;
                            while (i + width < uSize && sameCell(cell, mask[maskIndex(i + width, j)])) {
                                ++width;
                            }

                            int height = 1;
                            bool done = false;
                            while (j + height < vSize && !done) {
                                for (int k = 0; k < width; ++k) {
                                    if (!sameCell(cell, mask[maskIndex(i + k, j + height)])) {
                                        done = true;
                                        break;
                                    }
                                }
                                if (!done) ++height;
                            }

                            float base[3] = {static_cast<float>(chunkWorldX), 0.0f, static_cast<float>(chunkWorldZ)};
                            base[u] += static_cast<float>(i);
                            base[v] += static_cast<float>(j);
                            base[d] += static_cast<float>(q);

                            float du[3] = {0,0,0};
                            float dv[3] = {0,0,0};
                            du[u] = static_cast<float>(width);
                            dv[v] = static_cast<float>(height);

                            bool invert = (dirSign == -1);
                            appendQuad(base, du, dv, cell, invert);

                            for (int y2 = 0; y2 < height; ++y2) {
                                for (int x2 = 0; x2 < width; ++x2) {
                                    mask[maskIndex(i + x2, j + y2)].present = false;
                                }
                            }
                            i += width;
                        }
                    }
                }
            }
        }
    };

    runGreedy(false, verticesSolid, indicesSolid);
    runGreedy(true, verticesWater, indicesWater);

    if (!verticesSolid.empty()) {
        auto oldMesh = chunk->getMeshShared();
        if (oldMesh) {
            std::lock_guard<std::mutex> gLock(garbageMutex_);
            meshGarbage_.push_back(oldMesh);
        }
        chunk->setMesh(std::make_shared<Mesh>(context_, verticesSolid, indicesSolid));
    } else {
        auto oldMesh = chunk->getMeshShared();
        if (oldMesh) {
            std::lock_guard<std::mutex> gLock(garbageMutex_);
            meshGarbage_.push_back(oldMesh);
        }
        chunk->setMesh(std::shared_ptr<Mesh>{});
    }
    if (!verticesWater.empty()) {
        auto oldMesh = chunk->getWaterMesh();
        if (oldMesh) {
            std::lock_guard<std::mutex> gLock(garbageMutex_);
            meshGarbage_.push_back(oldMesh);
        }
        chunk->setWaterMesh(std::make_shared<Mesh>(context_, verticesWater, indicesWater));
    } else {
        auto oldMesh = chunk->getWaterMesh();
        if (oldMesh) {
            std::lock_guard<std::mutex> gLock(garbageMutex_);
            meshGarbage_.push_back(oldMesh);
        }
        chunk->setWaterMesh(std::shared_ptr<Mesh>{});
    }
}

} // namespace graphics
