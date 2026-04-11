#pragma once

#include "../core/IGame.hpp"
#include "../ecs/Entity.hpp"
#include "../components/GameSessionComponent.hpp"
#include "../systems/PhysicsSystem.hpp"
#include "../systems/AudioSystem.hpp"
#include "../systems/RaycastSystem.hpp"
#include "../systems/TerrainChunkSystem.hpp"
#include "../utils/TerrainHeightField.hpp"
#include "terrain/TerrainWorldMap.hpp"
#include "systems/AiControllerSystem.hpp"
#include "systems/CollisionSystem.hpp"
#include "systems/HitHurtTriggerSystem.hpp"
#include "systems/PlayerAiContactDamageSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/PlayerControllerSystem.hpp"
#include "../systems/SpawnCatalogGridSystem.hpp"

class Registry;
class AssetManager;
class ThreadService;
class OpenGLVer2Renderer;

class SuperHero final : public IGame {
public:
    using Settings = GameSessionComponent::Settings;

    explicit SuperHero(const Settings& settings);
    ~SuperHero() override;

    void onStart() override;
    void onInput() override;
    void onUpdate(double dt) override;
    void onRender(double alpha) override;
    void onStop() override;

    bool shouldClose() const override;

private:
    void registerComponents();
    void updateThirdPersonCamera(float dt);
    void updateRightArmAim();
    void updateAnimClipCrossFade(float dt);
    /// Copies transforms on the main thread, then ThreadService::parallelRange checksums chunks (no Registry on workers).
    void runParallelTransformSnapshotPass();

    /// Until `onStart` creates `m_sessionEntity`, holds ctor settings (registry does not exist yet).
    GameSessionComponent::Settings m_sessionBootstrap{};
    Entity m_sessionEntity = INVALID_ENTITY;

    Registry* m_registry = nullptr;
    AssetManager* m_assetManager = nullptr;
    ThreadService* m_threadService = nullptr;
    OpenGLVer2Renderer* m_renderer = nullptr;

    AudioSystem m_audioSystem{};

    Entity m_character = INVALID_ENTITY;
    Entity m_hat = INVALID_ENTITY;
    Entity m_camera = INVALID_ENTITY;
    /// Bone-attached line-of-sight ray (e.g. head forward).
    Entity m_headRay = INVALID_ENTITY;

    PlayerControllerSystem m_playerController{};
    AIControllerSystem m_aiController{};
    MovementSystem m_movementSystem{};
    CollisionSystem m_collisionSystem{};
    PhysicsSystem m_physicsSys{};
    RaycastSystem m_raycastSys{};
    HitHurtTriggerSystem m_hitHurtTrigger{};
    PlayerAiContactDamageSystem m_contactDamage{};

    TerrainChunkSystem m_terrainChunks{};
    TerrainHeightField m_terrainHeights{};
    TerrainWorldMap m_terrainMap{};

    int m_terrainChunkSize = 32;
    float m_terrainScale = 1.f;

    SpawnCatalogGridSystem m_spawnGrid{};
};
