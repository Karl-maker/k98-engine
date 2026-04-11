#pragma once

#include "AssetManager.hpp"
#include "ModelAsset.hpp"
#include "../../game/SkeletonSpawn.hpp"
#include "../../components/StreamingAnchorComponent.hpp"
#include "../../components/StreamableModelComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/WorldTransformComponent.hpp"
#include "../../ecs/Registry.hpp"
#include "../../utils/ProximityUtils.hpp"
#include "../ThreadService.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct PendingModelLoad {
    Entity streamableEntity = INVALID_ENTITY;
    std::future<std::shared_ptr<ModelAsset>> future;
};

/// Single-threaded completion: poll futures on main thread; spawn ECS when load finishes.
/// Loads are dispatched with ThreadService::submit when `threads` is non-null and running;
/// otherwise std::async is used.
class StreamingLoadService {
public:
    explicit StreamingLoadService(AssetManager& assets, ThreadService* threads = nullptr)
        : m_assets(assets)
        , m_threads(threads) {}

    void setTrackedBoneNames(std::vector<std::string> names) {
        m_trackedBones = std::move(names);
    }

    void update(Registry& registry) {
        syncAnchors(registry);
        processProximity(registry);
        pollCompletes(registry);
    }

private:
    void syncAnchors(Registry& registry) {
        auto withTransform = registry.getEntitiesWith<StreamingAnchorComponent, TransformComponent>();
        for (Entity e : withTransform) {
            auto& t = registry.getComponent<TransformComponent>(e);
            auto& a = registry.getComponent<StreamingAnchorComponent>(e);
            a.worldPosition = t.position;
        }
    }

    void processProximity(Registry& registry) {
        auto streamables = registry.getEntitiesWith<StreamableModelComponent, StreamingAnchorComponent>();

        for (Entity e : streamables) {
            auto& sm = registry.getComponent<StreamableModelComponent>(e);
            auto& anchor = registry.getComponent<StreamingAnchorComponent>(e);

            Vec3 viewerPos = anchor.worldPosition;
            if (anchor.viewerEntity != INVALID_ENTITY &&
                registry.hasComponent<TransformComponent>(anchor.viewerEntity)) {
                const auto& vt = registry.getComponent<TransformComponent>(anchor.viewerEntity);
                viewerPos = vt.position;
            }

            float dist = ProximityUtils::distance(anchor.worldPosition, viewerPos);

            if (sm.state == StreamableLoadState::Unloaded && dist <= anchor.loadRadius && !sm.modelPath.empty()) {
                sm.state = StreamableLoadState::Loading;
                PendingModelLoad pending;
                pending.streamableEntity = e;
                auto loadTask = [this, path = sm.modelPath]() {
                    return std::static_pointer_cast<ModelAsset>(m_assets.loadSharedAsync(path).get());
                };
                if (m_threads && m_threads->isRunning()) {
                    pending.future = m_threads->submit(std::move(loadTask));
                } else {
                    pending.future = std::async(std::launch::async, std::move(loadTask));
                }
                std::lock_guard<std::mutex> lock(m_mutex);
                m_pending.push_back(std::move(pending));
            }

            if (sm.state == StreamableLoadState::Loaded && dist >= anchor.unloadRadius && !sm.spawnedEntities.empty()) {
                destroySkinnedSpawn(registry, sm.spawnedEntities);
                sm.spawnedEntities.clear();
                sm.state = StreamableLoadState::Unloaded;
            }
        }
    }

    void pollCompletes(Registry& registry) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (it->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                ++it;
                continue;
            }

            std::shared_ptr<ModelAsset> model = it->future.get();
            Entity se = it->streamableEntity;
            it = m_pending.erase(it);

            if (se == INVALID_ENTITY || !registry.hasComponent<StreamableModelComponent>(se))
                continue;

            auto& sm = registry.getComponent<StreamableModelComponent>(se);
            if (sm.state != StreamableLoadState::Loading)
                continue;

            if (!model || model->skeleton.bones.empty()) {
                sm.state = StreamableLoadState::Unloaded;
                continue;
            }

            sm.spawnedEntities.clear();
            std::shared_ptr<const ModelAsset> cmodel(model);
            spawnSkinnedHierarchy(registry, cmodel, m_trackedBones, &sm.spawnedEntities);
            sm.state = StreamableLoadState::Loaded;
        }
    }

    AssetManager& m_assets;
    ThreadService* m_threads = nullptr;
    std::vector<std::string> m_trackedBones;
    std::vector<PendingModelLoad> m_pending;
    std::mutex m_mutex;
};
