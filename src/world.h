#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

using Entity = std::uint32_t;

struct Transform {
    float position[3] = {0.f, 0.f, 0.f};
    float rotationEuler[3] = {0.f, 0.f, 0.f}; // yaw/pitch/roll em radianos
    float scale[3] = {1.f, 1.f, 1.f};
};

struct Camera {
    float fovDegrees = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

struct Mesh {
    // Placeholder for mesh handle; in futuro podemos armazenar ids para buffers/pipelines.
    bool valid = false;
};

struct Material {
    float color[3] = {1.f, 1.f, 1.f};
};

class World {
public:
    Entity createEntity();
    void destroyEntity(Entity e);

    void setTransform(Entity e, const Transform& t);
    Transform* getTransform(Entity e);
    const Transform* getTransform(Entity e) const;

    void setCamera(Entity e, const Camera& c);
    Camera* getCamera(Entity e);
    const Camera* getCamera(Entity e) const;

    void setMesh(Entity e, const Mesh& m);
    Mesh* getMesh(Entity e);
    const Mesh* getMesh(Entity e) const;

    void setMaterial(Entity e, const Material& m);
    Material* getMaterial(Entity e);
    const Material* getMaterial(Entity e) const;

    const std::vector<Entity>& entities() const { return entities_; }
    std::size_t aliveCount() const;

    void update(float dtSeconds);

private:
    std::vector<Entity> entities_;
    std::vector<bool> alive_;
    std::vector<Transform> transforms_;
    std::vector<Camera> cameras_;
    std::vector<Mesh> meshes_;
    std::vector<Material> materials_;
};
