#include "world.h"

#include <algorithm>

Entity World::createEntity() {
    Entity id = static_cast<Entity>(entities_.size());
    entities_.push_back(id);
    alive_.push_back(true);
    transforms_.push_back(Transform{});
    cameras_.push_back(Camera{});
    meshes_.push_back(Mesh{});
    materials_.push_back(Material{});
    return id;
}

void World::destroyEntity(Entity e) {
    if (e >= entities_.size()) return;
    alive_[e] = false;
    transforms_[e] = Transform{};
    cameras_[e] = Camera{};
    meshes_[e] = Mesh{};
    materials_[e] = Material{};
}

void World::setTransform(Entity e, const Transform& t) {
    if (e >= transforms_.size()) return;
    transforms_[e] = t;
}

Transform* World::getTransform(Entity e) {
    if (e >= transforms_.size()) return nullptr;
    return &transforms_[e];
}

const Transform* World::getTransform(Entity e) const {
    if (e >= transforms_.size()) return nullptr;
    return &transforms_[e];
}

void World::setCamera(Entity e, const Camera& c) {
    if (e >= cameras_.size()) return;
    cameras_[e] = c;
}

Camera* World::getCamera(Entity e) {
    if (e >= cameras_.size()) return nullptr;
    return &cameras_[e];
}

const Camera* World::getCamera(Entity e) const {
    if (e >= cameras_.size()) return nullptr;
    return &cameras_[e];
}

void World::setMesh(Entity e, const Mesh& m) {
    if (e >= meshes_.size()) return;
    meshes_[e] = m;
}

Mesh* World::getMesh(Entity e) {
    if (e >= meshes_.size()) return nullptr;
    return &meshes_[e];
}

const Mesh* World::getMesh(Entity e) const {
    if (e >= meshes_.size()) return nullptr;
    return &meshes_[e];
}

void World::setMaterial(Entity e, const Material& m) {
    if (e >= materials_.size()) return;
    materials_[e] = m;
}

Material* World::getMaterial(Entity e) {
    if (e >= materials_.size()) return nullptr;
    return &materials_[e];
}

const Material* World::getMaterial(Entity e) const {
    if (e >= materials_.size()) return nullptr;
    return &materials_[e];
}

std::size_t World::aliveCount() const {
    std::size_t n = 0;
    for (bool a : alive_) if (a) ++n;
    return n;
}

void World::update(float dtSeconds) {
    const float spinSpeed = 0.5f; // radianos por segundo
    for (size_t i = 0; i < entities_.size(); ++i) {
        if (!alive_[i]) continue;
        transforms_[i].rotationEuler[1] += spinSpeed * dtSeconds;
        if (transforms_[i].rotationEuler[1] > 6.28318f) {
            transforms_[i].rotationEuler[1] -= 6.28318f;
        }
    }
}
