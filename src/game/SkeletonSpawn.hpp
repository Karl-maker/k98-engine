#pragma once

#include "../ecs/Registry.hpp"
#include "../core/assets/ModelAsset.hpp"
#include "../components/SkeletonInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/AnimationPlaybackComponent.hpp"
#include "../components/BoneInstanceComponent.hpp"
#include "../components/MeshRenderProxyComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"

#include <memory>
#include <string>
#include <vector>

/// Spawns skeleton root + optional tracked bone entities + mesh proxy children. Returns root entity.
inline Entity spawnSkinnedHierarchy(
    Registry& registry,
    std::shared_ptr<const ModelAsset> model,
    const std::vector<std::string>& trackedBoneNames,
    std::vector<Entity>* outAllEntities = nullptr) {
    Entity root = registry.createEntity();
    if (outAllEntities)
        outAllEntities->push_back(root);

    SkeletonInstanceComponent skelInst{};
    skelInst.model = model;
    for (const std::string& name : trackedBoneNames) {
        auto it = model->skeleton.boneMap.find(name);
        if (it != model->skeleton.boneMap.end())
            skelInst.syncBoneIndices.push_back(it->second);
    }
    registry.addComponent(root, std::move(skelInst));
    registry.addComponent(root, SkeletonPoseComponent{});
    registry.addComponent(root, AnimationPlaybackComponent{});
    registry.addComponent(root, TransformComponent{});
    registry.addComponent(root, WorldTransformComponent{});

    for (const std::string& name : trackedBoneNames) {
        auto it = model->skeleton.boneMap.find(name);
        if (it == model->skeleton.boneMap.end())
            continue;
        Entity be = registry.createEntity();
        if (outAllEntities)
            outAllEntities->push_back(be);
        registry.addComponent(be, BoneInstanceComponent{root, it->second});
        registry.addComponent(be, WorldTransformComponent{});
    }

    for (size_t mi = 0; mi < model->meshes.size(); ++mi) {
        Entity me = registry.createEntity();
        if (outAllEntities)
            outAllEntities->push_back(me);
        int matIdx = model->meshes[mi].materialIndex;
        registry.addComponent(me, MeshRenderProxyComponent{root, static_cast<int>(mi), matIdx});
        registry.addComponent(me, TransformComponent{Vec3{}, Quat::Identity(), Vec3{1, 1, 1}, root});
        registry.addComponent(me, WorldTransformComponent{});
    }

    return root;
}

inline void destroySkinnedSpawn(Registry& registry, const std::vector<Entity>& entities) {
    for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
        Entity e = *it;
        if (registry.hasComponent<SkeletonInstanceComponent>(e))
            registry.removeComponent<SkeletonInstanceComponent>(e);
        if (registry.hasComponent<SkeletonPoseComponent>(e))
            registry.removeComponent<SkeletonPoseComponent>(e);
        if (registry.hasComponent<AnimationPlaybackComponent>(e))
            registry.removeComponent<AnimationPlaybackComponent>(e);
        if (registry.hasComponent<BoneInstanceComponent>(e))
            registry.removeComponent<BoneInstanceComponent>(e);
        if (registry.hasComponent<MeshRenderProxyComponent>(e))
            registry.removeComponent<MeshRenderProxyComponent>(e);
        if (registry.hasComponent<TransformComponent>(e))
            registry.removeComponent<TransformComponent>(e);
        if (registry.hasComponent<WorldTransformComponent>(e))
            registry.removeComponent<WorldTransformComponent>(e);
        registry.destroyEntity(e);
    }
}
