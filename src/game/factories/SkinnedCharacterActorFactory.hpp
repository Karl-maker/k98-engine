#pragma once

#include "../../ecs/Registry.hpp"
#include "../../ecs/Entity.hpp"
#include "../../core/assets/AssetManager.hpp"
#include "../../core/assets/IAsset.hpp"
#include "../../core/assets/ModelAsset.hpp"
#include "../../components/CollisionBoxComponent.hpp"
#include "../../components/GpuSkinPaletteComponent.hpp"
#include "../../components/MassComponent.hpp"
#include "../../components/Position.hpp"
#include "../../components/SkeletonInstanceComponent.hpp"
#include "../../components/SkeletonPoseComponent.hpp"
#include "../../components/AnimationPlaybackComponent.hpp"
#include "../../components/StaticMeshComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/Velocity.hpp"
#include "../../components/WorldTransformComponent.hpp"
#include "../../rendering/OpenGLRenderSystem.hpp"
#include "../../animation/AnimationSampling.hpp"

#include "../../math/Quat.hpp"
#include "../../math/Vec3.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace game::factories {

struct MassSpawnDesc {
    float massKg          = 1.0f;
    float gravityScale    = 1.0f;
    float footOffset      = 0.9f;
    float fallbackGroundY = 0.0f;
    uint32_t solidGroundMask = ~0u;
};

/// Spawns a rigged glTF character: simulation body + skeleton + idle clip playback.
/// After OpenGL init, call `uploadToGpuAndAttach`. Optionally add locomotion / tags in game code.
struct SkinnedCharacterSpawnDesc {
    Vec3 position{0.0f, 2.0f, 0.0f};
    float uniformScale        = 1.0f;
    Quat modelSpaceRotation     = Quat::Identity();
    float collisionHalfX        = 0.3f;
    float collisionHalfY        = 0.9f;
    float collisionHalfZ        = 0.3f;
    uint32_t physicsLayer       = 1u << 0;
    uint32_t collidesWithMask   = 1u << 2;
    MassSpawnDesc mass{};
};

struct SkinnedCharacterActorFactory {
    static int findIdleClipIndex(const ModelAsset& model)
    {
        for (size_t i = 0; i < model.clips.size(); ++i) {
            const std::string& n = model.clips[i].name;
            if (n.find("Idle") != std::string::npos || n.find("idle") != std::string::npos)
                return static_cast<int>(i);
        }
        return model.clips.empty() ? -1 : 0;
    }

    /// Loads `assets/business/man/scene.gltf` or the given path; sets `outCanonicalPath` for GPU cache key.
    static std::shared_ptr<ModelAsset> tryLoadGltf(AssetManager& assets, const std::string& pathIn, std::string& outCanonicalPath)
    {
        auto tryPath = [&](const std::string& path) -> std::shared_ptr<ModelAsset> {
            if (path.empty())
                return nullptr;
            std::shared_ptr<IAsset> asset = assets.load(path);
            if (!asset)
                return nullptr;
            auto model = std::dynamic_pointer_cast<ModelAsset>(asset);
            if (!model || model->meshes.empty())
                return nullptr;
            outCanonicalPath = path;
            return model;
        };

        if (!pathIn.empty()) {
            if (auto m = tryPath(pathIn))
                return m;
        }

#ifdef GAME_ENGINE_PROJECT_ROOT
        {
            const std::string root = GAME_ENGINE_PROJECT_ROOT;
            if (!root.empty()) {
                std::string combined = root;
                if (combined.back() != '/')
                    combined += '/';
                combined += "assets/business/man/scene.gltf";
                if (auto m = tryPath(combined))
                    return m;
            }
        }
#endif
        static const char* kRel[] = {
            "assets/business/man/scene.gltf",
            "../assets/business/man/scene.gltf",
        };
        for (const char* p : kRel) {
            if (auto m = tryPath(p))
                return m;
        }

        std::cerr << "SkinnedCharacterActorFactory: could not load glTF (try assets/business/man/scene.gltf).\n";
        return nullptr;
    }

    static Entity spawn(
        Registry& registry,
        const SkinnedCharacterSpawnDesc& desc,
        const std::shared_ptr<ModelAsset>& model)
    {
        Entity e = registry.createEntity();

        registry.addComponent(e, Position{desc.position.x, desc.position.y, desc.position.z});
        registry.addComponent(e, Velocity{0.0f, 0.0f, 0.0f});
        registry.addComponent(e, TransformComponent{});
        registry.addComponent(e, WorldTransformComponent{});

        auto& tf = registry.getComponent<TransformComponent>(e);
        tf.position = desc.position;

        CollisionBoxComponent box{};
        box.setBoxSize(desc.collisionHalfX * 2.0f, desc.collisionHalfY * 2.0f, desc.collisionHalfZ * 2.0f);
        box.lastPosition = desc.position;
        box.layer          = desc.physicsLayer;
        box.collidesWithMask = desc.collidesWithMask;
        registry.addComponent(e, box);

        MassComponent m{};
        m.mass              = desc.mass.massKg;
        m.gravityScale      = desc.mass.gravityScale;
        m.footOffset        = desc.mass.footOffset;
        m.fallbackGroundY   = desc.mass.fallbackGroundY;
        m.solidGroundMask   = desc.mass.solidGroundMask;
        registry.addComponent(e, m);

        if (model && !model->skeleton.bones.empty()) {
            SkeletonInstanceComponent skel{};
            skel.model = model;
            skel.syncBoneIndices.reserve(model->skeleton.bones.size());
            for (size_t i = 0; i < model->skeleton.bones.size(); ++i)
                skel.syncBoneIndices.push_back(static_cast<int>(i));
            registry.addComponent(e, std::move(skel));
            registry.addComponent(e, SkeletonPoseComponent{});

            const int idle = findIdleClipIndex(*model);
            AnimationPlaybackComponent play{};
            play.primaryClip   = idle >= 0 ? idle : -1;
            play.secondaryClip = -1;
            play.speedPrimary  = 1.f;
            play.speedSecondary = 0.f;
            play.blendAlpha    = 0.f;
            play.loopPrimary   = true;
            play.loopSecondary = true;
            play.invalidatePoseCache = true;
            registry.addComponent(e, std::move(play));
        }

        return e;
    }

    static bool uploadToGpuAndAttach(
        OpenGLRenderSystem& gl,
        Registry& registry,
        Entity e,
        const std::shared_ptr<ModelAsset>& model,
        const std::string& assetCacheKey,
        float uniformScale,
        Quat modelSpaceRotation,
        Vec3 modelSpaceTranslation = {0.0f, 0.0f, 0.0f})
    {
        if (!model || assetCacheKey.empty())
            return false;
        if (!gl.uploadStaticModel(*model, assetCacheKey)) {
            std::cerr << "SkinnedCharacterActorFactory: GPU upload failed for " << assetCacheKey << "\n";
            return false;
        }
        StaticMeshComponent sm{};
        sm.assetCacheKey             = assetCacheKey;
        sm.modelSpaceRotation        = modelSpaceRotation;
        sm.uniformScale              = uniformScale;
        sm.modelSpaceTranslation     = modelSpaceTranslation;
        sm.gpuRegistered             = true;
        registry.addComponent(e, std::move(sm));
        registry.addComponent(e, GpuSkinPaletteComponent{});
        return true;
    }

    /// After `AnimationSystem::update`, fills joint matrices for skinned drawing.
    static void updateGpuSkinPalette(Registry& registry, Entity e)
    {
        if (!registry.hasComponent<GpuSkinPaletteComponent>(e))
            return;
        if (!registry.hasComponent<SkeletonInstanceComponent>(e) || !registry.hasComponent<AnimationPlaybackComponent>(e))
            return;

        auto& skel = registry.getComponent<SkeletonInstanceComponent>(e);
        if (!skel.model || skel.model->skeleton.bones.empty())
            return;

        auto& play = registry.getComponent<AnimationPlaybackComponent>(e);
        std::vector<Mat4> global;
        sampleAnimationBlendedFull(
            *skel.model,
            play.primaryClip,
            play.timePrimary,
            play.secondaryClip,
            play.timeSecondary,
            play.blendAlpha,
            play.loopPrimary,
            play.loopSecondary,
            global);
        if (global.empty())
            return;

        auto& pal = registry.getComponent<GpuSkinPaletteComponent>(e);
        computeSkinMatrices(skel.model->skeleton, global, pal.jointSkinMatrices);
    }
};

} // namespace game::factories
