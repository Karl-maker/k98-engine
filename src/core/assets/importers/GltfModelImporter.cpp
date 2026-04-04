#include "GltfModelImporter.hpp"

#include "../ModelAsset.hpp"
#include "../../../math/Vertex.hpp"

// tiny_gltf.h includes stb_image.h once; do not include stb_image.h separately or STB_* gets defined twice.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace {

int componentTypeSize(int t) {
    switch (t) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: return 1;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
        case TINYGLTF_COMPONENT_TYPE_SHORT: return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
        case TINYGLTF_COMPONENT_TYPE_INT: return 4;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return 4;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return 4;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE: return 8;
        default: return 1;
    }
}

const unsigned char* accessorDataPtr(const tinygltf::Model& model, const tinygltf::Accessor& acc) {
    const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];
    return buf.data.data() + bv.byteOffset + acc.byteOffset;
}

bool readVec3(const tinygltf::Model& model, int accessorIndex, size_t vertexIndex, float out[3]) {
    if (accessorIndex < 0)
        return false;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    if (vertexIndex >= acc.count)
        return false;
    const unsigned char* base = accessorDataPtr(model, acc);
    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    if (stride == 0)
        stride = componentTypeSize(acc.componentType) * tinygltf::GetNumComponentsInType(acc.type);
    const unsigned char* p = base + vertexIndex * stride;
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        std::memcpy(out, p, sizeof(float) * 3);
        return true;
    }
    return false;
}

bool readVec4(const tinygltf::Model& model, int accessorIndex, size_t vertexIndex, float out[4]) {
    if (accessorIndex < 0)
        return false;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    if (vertexIndex >= acc.count)
        return false;
    const unsigned char* base = accessorDataPtr(model, acc);
    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    if (stride == 0)
        stride = componentTypeSize(acc.componentType) * tinygltf::GetNumComponentsInType(acc.type);
    const unsigned char* p = base + vertexIndex * stride;
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        std::memcpy(out, p, sizeof(float) * 4);
        return true;
    }
    return false;
}

void readJointIndices4(const tinygltf::Model& model, int accessorIndex, size_t vertexIndex, int out[4]) {
    for (int i = 0; i < 4; ++i)
        out[i] = 0;
    if (accessorIndex < 0)
        return;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    if (vertexIndex >= acc.count)
        return;
    const unsigned char* base = accessorDataPtr(model, acc);
    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    if (stride == 0) {
        int comps = tinygltf::GetNumComponentsInType(acc.type);
        stride = static_cast<size_t>(componentTypeSize(acc.componentType) * comps);
    }
    const unsigned char* p = base + vertexIndex * stride;
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        for (int i = 0; i < 4; ++i)
            out[i] = p[i];
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const unsigned short* us = reinterpret_cast<const unsigned short*>(p);
        for (int i = 0; i < 4; ++i)
            out[i] = static_cast<int>(us[i]);
    }
}

Mat4 readMat4FromAccessor(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t matrixIndex) {
    Mat4 result = Mat4::Identity();
    const unsigned char* base = accessorDataPtr(model, acc);
    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    if (stride == 0)
        stride = 16 * sizeof(float);
    const float* m = reinterpret_cast<const float*>(base + matrixIndex * stride);
    std::memcpy(result.m, m, sizeof(float) * 16);
    return result;
}

void buildParentMap(const tinygltf::Model& model, std::vector<int>& parentOfNode) {
    parentOfNode.assign(model.nodes.size(), -1);
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        for (int c : model.nodes[i].children) {
            if (c >= 0 && static_cast<size_t>(c) < parentOfNode.size())
                parentOfNode[static_cast<size_t>(c)] = static_cast<int>(i);
        }
    }
}

int jointIndexForNode(const std::vector<int>& joints, int nodeIndex) {
    for (size_t i = 0; i < joints.size(); ++i) {
        if (joints[i] == nodeIndex)
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace

std::shared_ptr<IAsset> GltfModelImporter::import(const std::string& path) {
    auto modelAsset = std::make_shared<ModelAsset>();

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = false;
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".glb") == 0) {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    } else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    }

    if (!warn.empty())
        std::cerr << "glTF warn: " << warn << "\n";
    if (!ok) {
        std::cerr << "glTF load failed: " << err << " (" << path << ")\n";
        return modelAsset;
    }

    std::vector<int> parentOfNode;
    buildParentMap(model, parentOfNode);

    // --- Skeleton from first skin (if any) ---
    std::unordered_map<int, int> nodeIndexToBoneIndex;
    if (!model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];
        modelAsset->skeleton.bones.resize(skin.joints.size());
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int nodeIdx = skin.joints[i];
            nodeIndexToBoneIndex[nodeIdx] = static_cast<int>(i);
            Bone& b = modelAsset->skeleton.bones[i];
            if (nodeIdx >= 0 && static_cast<size_t>(nodeIdx) < model.nodes.size()) {
                b.name = model.nodes[nodeIdx].name;
                if (b.name.empty())
                    b.name = "bone_" + std::to_string(i);
                modelAsset->skeleton.boneMap[b.name] = static_cast<int>(i);
            }
            if (skin.inverseBindMatrices >= 0) {
                const tinygltf::Accessor& acc = model.accessors[skin.inverseBindMatrices];
                b.inverseBind = readMat4FromAccessor(model, acc, i);
            }
            if (nodeIdx >= 0 && static_cast<size_t>(nodeIdx) < model.nodes.size()) {
                const tinygltf::Node& node = model.nodes[nodeIdx];
                if (node.translation.size() >= 3) {
                    b.restTranslation = {static_cast<float>(node.translation[0]),
                                         static_cast<float>(node.translation[1]),
                                         static_cast<float>(node.translation[2])};
                }
                if (node.rotation.size() >= 4) {
                    b.restRotation = {static_cast<float>(node.rotation[0]),
                                      static_cast<float>(node.rotation[1]),
                                      static_cast<float>(node.rotation[2]),
                                      static_cast<float>(node.rotation[3])};
                }
                if (node.scale.size() >= 3) {
                    b.restScale = {static_cast<float>(node.scale[0]),
                                   static_cast<float>(node.scale[1]),
                                   static_cast<float>(node.scale[2])};
                }
            }
        }
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int nodeIdx = skin.joints[i];
            int pNode = (nodeIdx >= 0 && static_cast<size_t>(nodeIdx) < parentOfNode.size())
                ? parentOfNode[static_cast<size_t>(nodeIdx)]
                : -1;
            int pBone = jointIndexForNode(skin.joints, pNode);
            modelAsset->skeleton.bones[i].parentIndex = pBone;
        }
    }

    // --- Meshes ---
    for (const tinygltf::Mesh& mesh : model.meshes) {
        for (const tinygltf::Primitive& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1 && prim.mode != 4)
                continue;

            Mesh outMesh;
            outMesh.materialIndex = prim.material >= 0 ? prim.material : 0;

            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end())
                continue;
            int posAcc = posIt->second;
            const tinygltf::Accessor& posAccessor = model.accessors[posAcc];
            size_t vcount = posAccessor.count;

            int nAcc = -1;
            auto nIt = prim.attributes.find("NORMAL");
            if (nIt != prim.attributes.end())
                nAcc = nIt->second;

            int jAcc = -1, wAcc = -1;
            auto jIt = prim.attributes.find("JOINTS_0");
            if (jIt != prim.attributes.end())
                jAcc = jIt->second;
            auto wIt = prim.attributes.find("WEIGHTS_0");
            if (wIt != prim.attributes.end())
                wAcc = wIt->second;

            outMesh.vertices.resize(vcount);
            for (size_t vi = 0; vi < vcount; ++vi) {
                float p[3];
                if (!readVec3(model, posAcc, vi, p))
                    continue;
                Vertex& v = outMesh.vertices[vi];
                v.x = p[0];
                v.y = p[1];
                v.z = p[2];
                float n[3] = {0, 0, 1};
                if (nAcc >= 0)
                    readVec3(model, nAcc, vi, n);
                v.nx = n[0];
                v.ny = n[1];
                v.nz = n[2];

                if (jAcc >= 0 && wAcc >= 0 && !modelAsset->skeleton.bones.empty()) {
                    int ji[4];
                    readJointIndices4(model, jAcc, vi, ji);
                    float wgt[4] = {};
                    readVec4(model, wAcc, vi, wgt);
                    for (int k = 0; k < 4; ++k) {
                        int boneIdx = ji[k];
                        if (boneIdx >= 0 && boneIdx < static_cast<int>(modelAsset->skeleton.bones.size()) &&
                            wgt[k] > 0.0f)
                            v.addBoneInfluence(boneIdx, wgt[k]);
                    }
                    v.normalizeWeights();
                }
            }

            // Indices
            if (prim.indices >= 0) {
                const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                outMesh.indices.resize(idxAcc.count);
                for (size_t ii = 0; ii < idxAcc.count; ++ii) {
                    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        unsigned short us;
                        const tinygltf::Accessor& acc = idxAcc;
                        const unsigned char* base = accessorDataPtr(model, acc);
                        size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
                        if (stride == 0)
                            stride = sizeof(unsigned short);
                        std::memcpy(&us, base + ii * stride, sizeof(us));
                        outMesh.indices[ii] = static_cast<int>(us);
                    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        unsigned int ui;
                        const unsigned char* base = accessorDataPtr(model, idxAcc);
                        size_t stride = idxAcc.ByteStride(model.bufferViews[idxAcc.bufferView]);
                        if (stride == 0)
                            stride = sizeof(unsigned int);
                        std::memcpy(&ui, base + ii * stride, sizeof(ui));
                        outMesh.indices[ii] = static_cast<int>(ui);
                    }
                }
            } else {
                for (size_t ii = 0; ii < vcount; ++ii)
                    outMesh.indices.push_back(static_cast<int>(ii));
            }

            modelAsset->meshes.push_back(std::move(outMesh));
        }
    }

    // Materials (paths left as URI strings for streaming cache)
    modelAsset->materials.resize(model.materials.size());
    for (size_t mi = 0; mi < model.materials.size(); ++mi) {
        const tinygltf::Material& mat = model.materials[mi];
        auto baseColorIt = mat.values.find("baseColorTexture");
        if (baseColorIt != mat.values.end()) {
            int texIdx = baseColorIt->second.TextureIndex();
            if (texIdx >= 0 && static_cast<size_t>(texIdx) < model.textures.size()) {
                int src = model.textures[texIdx].source;
                if (src >= 0 && static_cast<size_t>(src) < model.images.size()) {
                    modelAsset->materials[mi].albedoTexture = model.images[src].uri;
                }
            }
        }
    }
    if (modelAsset->materials.empty())
        modelAsset->materials.push_back(Material{});

    // --- Animations ---
    for (const tinygltf::Animation& anim : model.animations) {
        AnimationClipData clip;
        clip.name = anim.name.empty() ? "anim_" + std::to_string(modelAsset->clips.size()) : anim.name;
        clip.durationSec = 0.f;

        for (const tinygltf::AnimationChannel& ch : anim.channels) {
            int nodeIdx = ch.target_node;
            auto it = nodeIndexToBoneIndex.find(nodeIdx);
            if (it == nodeIndexToBoneIndex.end())
                continue;
            int boneIdx = it->second;

            const tinygltf::AnimationSampler& sampler = anim.samplers[ch.sampler];
            int inputAcc = sampler.input;
            int outputAcc = sampler.output;
            if (inputAcc < 0 || outputAcc < 0)
                continue;

            const tinygltf::Accessor& inAcc = model.accessors[inputAcc];
            size_t keyCount = inAcc.count;

            ClipBoneChannel outCh;
            outCh.boneIndex = boneIdx;

            if (ch.target_path == "translation") {
                outCh.path = AnimChannelPath::Translation;
                for (size_t k = 0; k < keyCount; ++k) {
                    float t = 0;
                    const unsigned char* tin = accessorDataPtr(model, inAcc);
                    size_t istride = inAcc.ByteStride(model.bufferViews[inAcc.bufferView]);
                    if (istride == 0)
                        istride = sizeof(float);
                    std::memcpy(&t, tin + k * istride, sizeof(float));
                    float v[3];
                    readVec3(model, outputAcc, k, v);
                    outCh.vecKeys.push_back(Vec3Keyframe{t, {v[0], v[1], v[2]}});
                    clip.durationSec = std::max(clip.durationSec, t);
                }
            } else if (ch.target_path == "rotation") {
                outCh.path = AnimChannelPath::Rotation;
                for (size_t k = 0; k < keyCount; ++k) {
                    float t = 0;
                    const unsigned char* tin = accessorDataPtr(model, inAcc);
                    size_t istride = inAcc.ByteStride(model.bufferViews[inAcc.bufferView]);
                    if (istride == 0)
                        istride = sizeof(float);
                    std::memcpy(&t, tin + k * istride, sizeof(float));
                    float q[4];
                    readVec4(model, outputAcc, k, q);
                    outCh.quatKeys.push_back(QuatKeyframe{t, {q[0], q[1], q[2], q[3]}});
                    clip.durationSec = std::max(clip.durationSec, t);
                }
            } else if (ch.target_path == "scale") {
                outCh.path = AnimChannelPath::Scale;
                for (size_t k = 0; k < keyCount; ++k) {
                    float t = 0;
                    const unsigned char* tin = accessorDataPtr(model, inAcc);
                    size_t istride = inAcc.ByteStride(model.bufferViews[inAcc.bufferView]);
                    if (istride == 0)
                        istride = sizeof(float);
                    std::memcpy(&t, tin + k * istride, sizeof(float));
                    float v[3];
                    readVec3(model, outputAcc, k, v);
                    outCh.vecKeys.push_back(Vec3Keyframe{t, {v[0], v[1], v[2]}});
                    clip.durationSec = std::max(clip.durationSec, t);
                }
            } else
                continue;

            clip.channels.push_back(std::move(outCh));
        }

        if (!clip.channels.empty())
            modelAsset->clips.push_back(std::move(clip));
    }

    return modelAsset;
}
