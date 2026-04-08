#pragma once

#include "../core/assets/ModelAsset.hpp"
#include "../skeleton/Bone.hpp"
#include "../components/MeshComponent.hpp"
#include "../components/MaterialComponent.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../texture/TextureTypes.hpp"
#include <string>

inline void mapMeshFromModel(const Mesh& src, MeshComponent& out)
{
    out.vertices = src.vertices;
    out.boneData = src.boneData;
    out.indices.clear();
    out.indices.reserve(src.indices.size());
    for (int i : src.indices)
        out.indices.push_back(static_cast<uint32_t>(i));
}

inline void mapSkeletonFromModel(const ModelAsset& model, SkeletonComponent& out)
{
    out.bones.clear();
    out.boneMap = model.skeleton.boneMap;
    out.bones.reserve(model.skeleton.bones.size());
    for (const auto& mb : model.skeleton.bones) {
        SkeletonBone b;
        b.name = mb.name;
        b.parentIndex = mb.parentIndex;
        b.localPosition = mb.restTranslation;
        b.localRotation = mb.restRotation;
        b.localScale = mb.restScale;
        b.bindPosition = mb.restTranslation;
        b.bindRotation = mb.restRotation;
        b.bindScale = mb.restScale;
        b.inverseBind = mb.inverseBind;
        out.bones.push_back(b);
    }
}

inline void initRestPoseFromSkeleton(SkeletonComponent& sk, PoseComponent& pose)
{
    pose.localPose.resize(sk.bones.size());
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        pose.localPose[i].position = sk.bones[i].localPosition;
        pose.localPose[i].rotation = sk.bones[i].localRotation;
        pose.localPose[i].scale = sk.bones[i].localScale;
    }
    pose.dirty = true;
}

inline void mapMaterialFromModel(const ModelAsset& model, int materialIndex, MaterialComponent& out)
{
    out.textures.clear();
    if (materialIndex < 0 || static_cast<size_t>(materialIndex) >= model.materials.size())
        return;
    const Material& m = model.materials[static_cast<size_t>(materialIndex)];
    auto add = [&](const std::string& path, TextureType ty) {
        if (!path.empty())
            out.textures.push_back(TextureComponent(path, ty));
    };
    add(m.albedoTexture, TextureType::Diffuse);
    add(m.normalTexture, TextureType::Normal);
    add(m.metallicRoughnessTexture, TextureType::Metallic);
    add(m.occlusionTexture, TextureType::AmbientOcclusion);
}

inline int findBoneIndexByNameSubstring(const SkeletonComponent& sk, const char* substr)
{
    for (const auto& kv : sk.boneMap) {
        if (kv.first.find(substr) != std::string::npos)
            return kv.second;
    }
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        if (sk.bones[i].name.find(substr) != std::string::npos)
            return static_cast<int>(i);
    }
    return -1;
}
