#include "SuperHero.hpp"

#include "../core/ThreadService.hpp"
#include "../core/assets/AssetManager.hpp"
#include "../core/assets/importers/GltfModelImporter.hpp"
#include "../ecs/Registry.hpp"
#include "../graphics/opengl/OpenGLVer2Renderer.hpp"
#include "../graphics/GraphicsTypes.hpp"
#include "../game/factories/BusinessManSceneFactory.hpp"

#include "../components/AnimationComponent.hpp"
#include "../components/BoneAttachmentComponent.hpp"
#include "../components/BoneControlComponent.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/GpuSkinPaletteComponent.hpp"
#include "../components/HdriEnvironmentComponent.hpp"
#include "../components/HeightMapComponent.hpp"
#include "../components/LightingComponent.hpp"
#include "../components/MaterialComponent.hpp"
#include "../components/PbrMaterialPresetComponent.hpp"
#include "../components/MeshComponent.hpp"
#include "../components/PlayerTagComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/PrimitivePyramidComponent.hpp"
#include "../components/RenderableMeshComponent.hpp"
#include "../components/TerrainChunkComponent.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"

#include "../systems/AnimationSystem.hpp"
#include "../systems/BoneAttachmentSystem.hpp"
#include "../systems/BoneControlSystem.hpp"
#include "../systems/PoseSystem.hpp"
#include "../systems/WorldTransformSyncSystem.hpp"

#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"

#include <iostream>
#include <memory>

SuperHero::SuperHero(const Settings& settings)
    : m_settings(settings)
    , m_debugHudEnabled(settings.openglDebugHud)
{
    (void)m_debugHudEnabled;
}

SuperHero::~SuperHero()
{
    delete m_renderer;
    delete m_registry;
    delete m_assetManager;
    delete m_threadService;
}

void SuperHero::registerComponents()
{
    m_registry->registerComponent<TransformComponent>();
    m_registry->registerComponent<WorldTransformComponent>();
    m_registry->registerComponent<MeshComponent>();
    m_registry->registerComponent<MaterialComponent>();
    m_registry->registerComponent<SkeletonComponent>();
    m_registry->registerComponent<PoseComponent>();
    m_registry->registerComponent<AnimationComponent>();
    m_registry->registerComponent<BoneControlComponent>();
    m_registry->registerComponent<BoneAttachmentComponent>();
    m_registry->registerComponent<GpuSkinPaletteComponent>();
    m_registry->registerComponent<RenderableMeshComponent>();
    m_registry->registerComponent<PrimitivePyramidComponent>();
    m_registry->registerComponent<PlayerTagComponent>();
    m_registry->registerComponent<HdriEnvironmentComponent>();
    m_registry->registerComponent<LightingComponent>();
    m_registry->registerComponent<CameraComponent>();
    /// Required for OpenGLVer2Renderer terrain / preset queries (unique component type ids).
    m_registry->registerComponent<PbrMaterialPresetComponent>();
    m_registry->registerComponent<TerrainChunkComponent>();
    m_registry->registerComponent<HeightMapComponent>();
}

void SuperHero::updateCamera()
{
    if (!m_registry || m_camera == INVALID_ENTITY || !m_registry->hasComponent<CameraComponent>(m_camera) ||
        !m_registry->hasComponent<TransformComponent>(m_camera))
        return;
    if (m_character == INVALID_ENTITY || !m_registry->hasComponent<TransformComponent>(m_character))
        return;

    auto& cam = m_registry->getComponent<CameraComponent>(m_camera);
    auto& camTf = m_registry->getComponent<TransformComponent>(m_camera);
    auto& charTf = m_registry->getComponent<TransformComponent>(m_character);

    const Vec3 eye = camTf.position;
    Vec3 target{
        charTf.position.x + cam.lookAtOffset.x,
        charTf.position.y + cam.lookAtOffset.y,
        charTf.position.z + cam.lookAtOffset.z};
    cam.viewMatrix = Mat4::inverse(Mat4::LookAt(eye, target, {0.f, 1.f, 0.f}));
}

void SuperHero::onStart()
{
    m_registry = new Registry();
    m_assetManager = new AssetManager();
    m_assetManager->registerImporter("gltf", std::make_shared<GltfModelImporter>());
    m_threadService = new ThreadService();
    m_renderer = new OpenGLVer2Renderer();

    m_threadService->configure({});
    m_threadService->start();

    registerComponents();

    GraphicsInitOptions glOpts;
    glOpts.swapInterval = m_settings.glSwapInterval;
    if (!m_renderer->init(1280, 720, "SuperHero", glOpts)) {
        std::cerr << "OpenGL renderer init failed\n";
        m_shouldClose = true;
        return;
    }
    m_renderer->installDefaultRenderPasses();

    const auto scene = game::factories::spawnBusinessManScene(*m_registry, *m_renderer, *m_assetManager);
    m_character = scene.character;
    m_hat = scene.hat;
    m_camera = scene.camera;
    if (m_character == INVALID_ENTITY) {
        std::cerr << "Failed to spawn business/man scene.gltf\n";
        m_shouldClose = true;
        return;
    }

    updateCamera();
}

void SuperHero::onInput()
{
}

void SuperHero::onUpdate(double dt)
{
    if (m_shouldClose || !m_registry)
        return;

    const float fdt = static_cast<float>(dt);
    AnimationSystem animSys;
    animSys.update(*m_registry, fdt);

    BoneControlSystem boneCtrl;
    boneCtrl.update(*m_registry);

    PoseSystem poseSys;
    poseSys.update(*m_registry);

    if (m_character != INVALID_ENTITY)
        game::factories::updateCharacterSkinPalette(*m_registry, m_character);

    BoneAttachmentSystem attachSys;
    attachSys.update(*m_registry);

    WorldTransformSyncSystem worldSys;
    worldSys.update(*m_registry);

    updateCamera();
}

void SuperHero::onRender(double)
{
    if (m_shouldClose || !m_renderer || !m_registry)
        return;

    m_renderer->renderFrame(*m_registry);
}

void SuperHero::onStop()
{
    if (m_renderer)
        m_renderer->shutdown();
}

bool SuperHero::shouldClose() const
{
    if (m_shouldClose)
        return true;
    if (m_renderer && m_renderer->shouldClose())
        return true;
    return false;
}
