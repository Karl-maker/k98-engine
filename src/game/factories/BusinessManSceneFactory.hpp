#pragma once

#include "../ModelAssetMapper.hpp"
#include "../../animation/SkinMatrix.hpp"
#include "../../components/AnimationComponent.hpp"
#include "../../components/BoneAttachmentComponent.hpp"
#include "../../components/BoneControlComponent.hpp"
#include "../../components/CameraComponent.hpp"
#include "../../components/CapsuleColliderComponent.hpp"
#include "../../components/ColliderFilterComponent.hpp"
#include "../../components/GpuSkinPaletteComponent.hpp"
#include "../../components/RigidBodyComponent.hpp"
#include "../../components/ThirdPersonComponent.hpp"
#include "../../components/HdriEnvironmentComponent.hpp"
#include "../../components/LightingComponent.hpp"
#include "../../components/MaterialComponent.hpp"
#include "../../components/MeshComponent.hpp"
#include "../../components/PlayerTagComponent.hpp"
#include "../../components/PoseComponent.hpp"
#include "../../components/PrimitivePyramidComponent.hpp"
#include "../../components/RenderableMeshComponent.hpp"
#include "../../components/SkeletonComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/WorldTransformComponent.hpp"
#include "../../core/assets/AssetManager.hpp"
#include "../../core/assets/ModelAsset.hpp"
#include "../../ecs/Registry.hpp"
#include "../../graphics/IGraphicsRenderer.hpp"
#include "../../math/MathOps.hpp"
#include "../../math/Vec3.hpp"
#include "../../physics/CollisionLayers.hpp"
#include "BusinessManExtraAnimations.hpp"
#include <cmath>
#include <memory>
#include <string>

namespace game::factories {

inline std::string defaultManGltfPath()
{
#ifdef GAME_ENGINE_PROJECT_ROOT
    std::string root = GAME_ENGINE_PROJECT_ROOT;
    if (!root.empty() && root.back() != '/')
        root += '/';
    return root + "assets/business/man/scene.gltf";
#else
    return "assets/business/man/scene.gltf";
#endif
}

inline std::string defaultHdriPath()
{
#ifdef GAME_ENGINE_PROJECT_ROOT
    std::string root = GAME_ENGINE_PROJECT_ROOT;
    if (!root.empty() && root.back() != '/')
        root += '/';
    return root + "assets/lighting/hdri.webp";
#else
    return "assets/lighting/hdri.webp";
#endif
}

/// Spawns HDRI + directional light + rigged character (scene.gltf) + pyramid hat on head bone.
struct BusinessManSceneHandles {
    Entity character = INVALID_ENTITY;
    Entity hat = INVALID_ENTITY;
    Entity hdri = INVALID_ENTITY;
    Entity sun = INVALID_ENTITY;
    Entity camera = INVALID_ENTITY;
    std::string assetCacheKey;
};

inline BusinessManSceneHandles spawnBusinessManScene(
    Registry& registry,
    IGraphicsRenderer& renderer,
    AssetManager& assets,
    const std::string& gltfPath = {})
{
    BusinessManSceneHandles out;
    std::string path = gltfPath.empty() ? defaultManGltfPath() : gltfPath;
    std::shared_ptr<IAsset> a = assets.load(path);
    auto model = std::dynamic_pointer_cast<ModelAsset>(a);
    if (!model || model->meshes.empty()) {
        return out;
    }

    out.assetCacheKey = path;
    if (!renderer.uploadStaticModel(*model, out.assetCacheKey)) {
        return out;
    }

    out.character = registry.createEntity();
    registry.addComponent(out.character, TransformComponent{});
    registry.addComponent(out.character, WorldTransformComponent{});
    auto& tf = registry.getComponent<TransformComponent>(out.character);
    tf.position = {0.f, 0.f, 0.f};
    const float yaw = 0.85f;
    tf.rotation = {0.f, std::sin(yaw * 0.5f), 0.f, std::cos(yaw * 0.5f)};

    SkeletonComponent sk;
    mapSkeletonFromModel(*model, sk);
    registry.addComponent(out.character, std::move(sk));
    auto& skel = registry.getComponent<SkeletonComponent>(out.character);

    PoseComponent pose;
    initRestPoseFromSkeleton(skel, pose);
    registry.addComponent(out.character, std::move(pose));

    AnimationComponent anim;
    anim.clips = model->clips;
    appendBusinessManSecondaryClip(skel, anim);
    anim.currentClip = anim.clips.empty() ? -1 : 0;
    anim.currentTime = 0.f;
    anim.looping = true;
    registry.addComponent(out.character, std::move(anim));

    registry.addComponent(out.character, BoneControlComponent{});
    registry.addComponent(out.character, GpuSkinPaletteComponent{});
    registry.addComponent(out.character, PlayerTagComponent{});

    const int meshIndex = 0;
    if (static_cast<size_t>(meshIndex) < model->meshes.size()) {
        MeshComponent meshComp;
        mapMeshFromModel(model->meshes[static_cast<size_t>(meshIndex)], meshComp);
        registry.addComponent(out.character, std::move(meshComp));

        MaterialComponent matComp;
        mapMaterialFromModel(*model, model->meshes[static_cast<size_t>(meshIndex)].materialIndex, matComp);
        registry.addComponent(out.character, std::move(matComp));
    }

    RenderableMeshComponent rm;
    rm.assetCacheKey = out.assetCacheKey;
    rm.gpuRegistered = true;
    rm.uniformScale = 1.f;
    registry.addComponent(out.character, std::move(rm));

    const int headBi = findBoneIndexByNameSubstring(skel, "Head");
    out.hat = registry.createEntity();
    registry.addComponent(out.hat, TransformComponent{});
    registry.addComponent(out.hat, WorldTransformComponent{});
    registry.addComponent(out.hat, PrimitivePyramidComponent{});
    {
        auto& pyr = registry.getComponent<PrimitivePyramidComponent>(out.hat);
        pyr.scale = 0.42f;
    }
    BoneAttachmentComponent attach{};
    attach.skeletonEntity = out.character;
    attach.boneIndex = headBi >= 0 ? headBi : 0;
    attach.localOffset = {0.f, 0.34f, 0.f};
    attach.localRotation = {0.f, 0.f, 0.f, 1.f};
    registry.addComponent(out.hat, attach);

    out.hdri = registry.createEntity();
    {
        HdriEnvironmentComponent h;
        h.hdriAssetPath = defaultHdriPath();
        h.enabled = true;
        h.intensity = 1.f;
        registry.addComponent(out.hdri, h);
    }

    out.sun = registry.createEntity();
    registry.addComponent(out.sun, TransformComponent{});
    registry.addComponent(out.sun, WorldTransformComponent{});
    {
        auto& st = registry.getComponent<TransformComponent>(out.sun);
        st.position = {3.f, 8.f, 2.f};
        LightingComponent L;
        L.type = LightType::Directional;
        L.enabled = true;
        L.intensity = 1.2f;
        L.color = {1.f, 0.98f, 0.92f};
        L.useEntityAxis = true;
        registry.addComponent(out.sun, L);
    }

    out.camera = registry.createEntity();
    registry.addComponent(out.camera, TransformComponent{});
    registry.addComponent(out.camera, CameraComponent{});
    registry.addComponent(out.camera, ThirdPersonComponent{});
    {
        auto& ctf = registry.getComponent<TransformComponent>(out.camera);
        ctf.position = {0.f, 0.f, 0.f};
        auto& cam = registry.getComponent<CameraComponent>(out.camera);
        cam.active = true;
        cam.enableLookAt = true;
        cam.lookAtTarget = out.character;
        cam.lookAtOffset = {0.f, 0.9f, 0.f};
    }

    return out;
}

/// Extra rigged character using the same glTF path and GPU cache key as `spawnBusinessManScene` (upload may be a no-op if already uploaded).
inline Entity spawnBusinessManCharacter(
    Registry& registry,
    IGraphicsRenderer& renderer,
    AssetManager& assets,
    const std::string& gltfPath,
    const Vec3& position,
    float yawRadians,
    bool addPlayerTag)
{
    std::shared_ptr<IAsset> a = assets.load(gltfPath);
    auto model = std::dynamic_pointer_cast<ModelAsset>(a);
    if (!model || model->meshes.empty())
        return INVALID_ENTITY;
    const std::string cacheKey = gltfPath;
    if (!renderer.uploadStaticModel(*model, cacheKey))
        return INVALID_ENTITY;

    Entity character = registry.createEntity();
    registry.addComponent(character, TransformComponent{});
    registry.addComponent(character, WorldTransformComponent{});
    auto& tf = registry.getComponent<TransformComponent>(character);
    tf.position = position;
    tf.rotation = {0.f, std::sin(yawRadians * 0.5f), 0.f, std::cos(yawRadians * 0.5f)};

    SkeletonComponent sk;
    mapSkeletonFromModel(*model, sk);
    registry.addComponent(character, std::move(sk));
    auto& skel = registry.getComponent<SkeletonComponent>(character);

    PoseComponent pose;
    initRestPoseFromSkeleton(skel, pose);
    registry.addComponent(character, std::move(pose));

    AnimationComponent anim;
    anim.clips = model->clips;
    appendBusinessManSecondaryClip(skel, anim);
    anim.currentClip = anim.clips.empty() ? -1 : 0;
    anim.currentTime = 0.f;
    anim.looping = true;
    registry.addComponent(character, std::move(anim));

    registry.addComponent(character, BoneControlComponent{});
    registry.addComponent(character, GpuSkinPaletteComponent{});
    if (addPlayerTag)
        registry.addComponent(character, PlayerTagComponent{});

    const int meshIndex = 0;
    if (static_cast<size_t>(meshIndex) < model->meshes.size()) {
        MeshComponent meshComp;
        mapMeshFromModel(model->meshes[static_cast<size_t>(meshIndex)], meshComp);
        registry.addComponent(character, std::move(meshComp));

        MaterialComponent matComp;
        mapMaterialFromModel(*model, model->meshes[static_cast<size_t>(meshIndex)].materialIndex, matComp);
        registry.addComponent(character, std::move(matComp));
    }

    RenderableMeshComponent rm;
    rm.assetCacheKey = cacheKey;
    rm.gpuRegistered = true;
    rm.uniformScale = 1.f;
    registry.addComponent(character, std::move(rm));

    RigidBodyComponent rb{};
    rb.mass = 1.f;
    rb.invMass = 1.f;
    rb.linearDamping = 2.8f;
    rb.friction = 0.38f;
    registry.addComponent(character, rb);

    CapsuleColliderComponent cap{};
    cap.radius = 0.26f;
    cap.halfHeight = 0.40f;
    cap.offset = {0.f, 0.72f, 0.f};
    registry.addComponent(character, cap);

    ColliderFilterComponent layers{};
    layers.categoryBits = CollisionLayer::Player;
    layers.collideMask = 0xFFFFFFFFu;
    registry.addComponent(character, layers);

    return character;
}

inline void updateCharacterSkinPalette(Registry& registry, Entity character)
{
    if (!registry.hasComponent<SkeletonComponent>(character) || !registry.hasComponent<PoseComponent>(character) ||
        !registry.hasComponent<GpuSkinPaletteComponent>(character))
        return;
    auto& sk = registry.getComponent<SkeletonComponent>(character);
    auto& pose = registry.getComponent<PoseComponent>(character);
    auto& pal = registry.getComponent<GpuSkinPaletteComponent>(character);
    computeJointSkinMatrices(sk, pose, pal);
}

} // namespace game::factories
