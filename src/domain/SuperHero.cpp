#include "SuperHero.hpp"

#include "components/ChaseComponent.hpp"
#include "components/MovementComponent.hpp"
#include "states/MovementStateEnums.hpp"

#include "../core/ThreadService.hpp"
#include "../core/assets/AssetManager.hpp"
#include "../core/assets/importers/GltfModelImporter.hpp"
#include "../ecs/Registry.hpp"
#include "../graphics/opengl/OpenGLVer2Renderer.hpp"
#include "../graphics/GraphicsTypes.hpp"
#include "../game/factories/BusinessManSceneFactory.hpp"
#include "../game/ModelAssetMapper.hpp"

#include "../components/AnimationComponent.hpp"
#include "../components/BoneAttachmentComponent.hpp"
#include "../components/BoneControlComponent.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/ThirdPersonComponent.hpp"
#include "../components/EntityAttachmentComponent.hpp"
#include "../components/GpuSkinPaletteComponent.hpp"
#include "../components/HdriEnvironmentComponent.hpp"
#include "../components/HeightMapComponent.hpp"
#include "../components/LightingComponent.hpp"
#include "../components/MaterialComponent.hpp"
#include "../components/PbrMaterialPresetComponent.hpp"
#include "../components/MeshComponent.hpp"
#include "../components/BoxColliderComponent.hpp"
#include "../components/CapsuleColliderComponent.hpp"
#include "../components/ColliderFilterComponent.hpp"
#include "../components/RaycastComponent.hpp"
#include "../components/PlayerTagComponent.hpp"
#include "../components/RigidBodyComponent.hpp"
#include "../components/SphereColliderComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/PrimitiveBoxComponent.hpp"
#include "../components/PrimitivePyramidComponent.hpp"
#include "../components/RenderableMeshComponent.hpp"
#include "../components/TerrainChunkComponent.hpp"
#include "../components/TerrainSettingsComponent.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"

#include "../systems/AnimationSystem.hpp"
#include "../systems/BoneAttachmentSystem.hpp"
#include "../systems/BoneControlSystem.hpp"
#include "../systems/EntityAttachmentSystem.hpp"
#include "../systems/PoseSystem.hpp"
#include "../systems/WorldTransformSyncSystem.hpp"
#include "systems/ThirdPersonCameraCollisionSystem.hpp"
#include "../physics/CollisionLayers.hpp"

#include "../math/Mat4.hpp"
#include "../math/MathOps.hpp"
#include "../math/Vec3.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {

const char* movementStateToString(MovementState s)
{
    switch (s) {
    case MovementState::Idle:
        return "Idle";
    case MovementState::Walk:
        return "Walk";
    case MovementState::Run:
        return "Run";
    case MovementState::Sprint:
        return "Sprint";
    case MovementState::Crouch:
        return "Crouch";
    case MovementState::Crawl:
        return "Crawl";
    case MovementState::Jump:
        return "Jump";
    case MovementState::Falling:
        return "Falling";
    default:
        return "?";
    }
}

/// Piecewise cycle: rest → blend to point → hold point → blend to rest → rest (6s loop).
float rightHandAimCycleWeight(float cycleTimeSec)
{
    constexpr float period = 6.f;
    const float t = std::fmod(cycleTimeSec, period);
    if (t < 1.2f)
        return 0.f;
    if (t < 2.2f)
        return t - 1.2f;
    if (t < 3.8f)
        return 1.f;
    if (t < 4.8f)
        return 4.8f - t;
    return 0.f;
}

} // namespace

static bool tryLoadTerrainMap(TerrainWorldMap& out)
{
    static const char* const paths[] = {
        "domain/terrain/world_map.txt",
        "../domain/terrain/world_map.txt",
        "../../domain/terrain/world_map.txt",
        "../../../game-engine/domain/terrain/world_map.txt",
    };
    for (const char* p : paths) {
        if (out.loadFromFile(p))
            return true;
    }
    return false;
}

SuperHero::SuperHero(const Settings& settings)
    : m_settings(settings)
    , m_debugHudEnabled(settings.openglDebugHud)
{
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
    m_registry->registerComponent<EntityAttachmentComponent>();
    m_registry->registerComponent<GpuSkinPaletteComponent>();
    m_registry->registerComponent<RenderableMeshComponent>();
    m_registry->registerComponent<PrimitiveBoxComponent>();
    m_registry->registerComponent<PrimitivePyramidComponent>();
    m_registry->registerComponent<PlayerTagComponent>();
    m_registry->registerComponent<HdriEnvironmentComponent>();
    m_registry->registerComponent<LightingComponent>();
    m_registry->registerComponent<CameraComponent>();
    m_registry->registerComponent<ThirdPersonComponent>();
    /// Required for OpenGLVer2Renderer terrain / preset queries (unique component type ids).
    m_registry->registerComponent<PbrMaterialPresetComponent>();
    m_registry->registerComponent<TerrainChunkComponent>();
    m_registry->registerComponent<TerrainSettingsComponent>();
    m_registry->registerComponent<HeightMapComponent>();
    m_registry->registerComponent<RigidBodyComponent>();
    m_registry->registerComponent<BoxColliderComponent>();
    m_registry->registerComponent<CapsuleColliderComponent>();
    m_registry->registerComponent<SphereColliderComponent>();
    m_registry->registerComponent<ColliderFilterComponent>();
    m_registry->registerComponent<RaycastComponent>();
    m_registry->registerComponent<MovementComponent>();
    m_registry->registerComponent<ChaseComponent>();
}

void SuperHero::updateRightArmAim()
{
    m_debugDetailText.clear();
    if (!m_registry || m_character == INVALID_ENTITY || m_camera == INVALID_ENTITY)
        return;
    if (!m_registry->hasComponent<SkeletonComponent>(m_character) || !m_registry->hasComponent<PoseComponent>(m_character) ||
        !m_registry->hasComponent<BoneControlComponent>(m_character) || !m_registry->hasComponent<TransformComponent>(m_camera))
        return;

    auto& sk = m_registry->getComponent<SkeletonComponent>(m_character);
    auto& pose = m_registry->getComponent<PoseComponent>(m_character);
    auto& ctrl = m_registry->getComponent<BoneControlComponent>(m_character);

    const int iArm = findBoneIndexByNameSubstring(sk, "RightArm");
    const int iFore = findBoneIndexByNameSubstring(sk, "RightForeArm");
    if (iArm < 0 || iFore < 0)
        return;
    if (static_cast<size_t>(iArm) >= pose.worldMatrix.size() || static_cast<size_t>(iFore) >= pose.worldMatrix.size() ||
        static_cast<size_t>(iArm) >= pose.localPose.size())
        return;

    const float aimWeight = rightHandAimCycleWeight(m_handAnimTime);
    const Quat R_anim = pose.localPose[static_cast<size_t>(iArm)].rotation;

    if (aimWeight < 1e-5f) {
        ctrl.overrides.erase(static_cast<int>(iArm));
        if (m_debugHudEnabled) {
            char buf[256];
            std::snprintf(
                buf,
                sizeof(buf),
                "Right arm IK\naim weight: %.2f (rest)", static_cast<double>(aimWeight));
            m_debugDetailText = buf;
        }
        return;
    }

    const int iParent = sk.bones[static_cast<size_t>(iArm)].parentIndex;
    Mat4 W_parent = Mat4::Identity();
    if (iParent >= 0 && static_cast<size_t>(iParent) < pose.worldMatrix.size())
        W_parent = pose.worldMatrix[static_cast<size_t>(iParent)];

    const Mat4& W_arm = pose.worldMatrix[static_cast<size_t>(iArm)];
    const Mat4& W_fore = pose.worldMatrix[static_cast<size_t>(iFore)];

    Vec3 Aw{W_arm.m[12], W_arm.m[13], W_arm.m[14]};
    Vec3 Fw{W_fore.m[12], W_fore.m[13], W_fore.m[14]};
    const Vec3 dCur = normalize(Vec3{Fw.x - Aw.x, Fw.y - Aw.y, Fw.z - Aw.z});
    if (lengthSquared(dCur) < 1e-12f)
        return;

    const Vec3 camPos = m_registry->getComponent<TransformComponent>(m_camera).position;
    Vec3 dWant{camPos.x - Aw.x, camPos.y - Aw.y, camPos.z - Aw.z};
    if (lengthSquared(dWant) < 1e-12f)
        return;
    dWant = normalize(dWant);

    const Quat qDelta = quatRotationBetweenUnit(dCur, dWant);
    const Quat R_parent = Mat4::RotationToQuat(W_parent);
    const Quat R_arm_world = quatMul(R_parent, R_anim);
    const Quat R_new_world = quatMul(qDelta, R_arm_world);
    const Quat R_point_local = quatMul(quatInverseUnit(R_parent), R_new_world);

    BoneOverrideEntry entry;
    entry.value = pose.localPose[static_cast<size_t>(iArm)];
    entry.value.rotation = R_point_local;
    entry.blendWeight = aimWeight;
    ctrl.overrides[static_cast<int>(iArm)] = entry;

    if (m_debugHudEnabled) {
        const float angleDeg =
            std::acos(std::max(-1.f, std::min(1.f, dot(dCur, dWant)))) * 180.0f / 3.14159265f;
        char buf[640];
        std::snprintf(
            buf,
            sizeof(buf),
            "Right arm IK\n"
            "aim weight: %.2f (0=clip 1=point)\n"
            "bones: arm %d fore %d par %d\n"
            "aim: camera\n"
            "align deg: %.1f\n"
            "Aw (%.2f, %.2f, %.2f)\n"
            "cam (%.2f, %.2f, %.2f)",
            static_cast<double>(aimWeight),
            iArm,
            iFore,
            iParent,
            static_cast<double>(angleDeg),
            static_cast<double>(Aw.x),
            static_cast<double>(Aw.y),
            static_cast<double>(Aw.z),
            static_cast<double>(camPos.x),
            static_cast<double>(camPos.y),
            static_cast<double>(camPos.z));
        m_debugDetailText = buf;
    }
}

void SuperHero::updateAnimClipCrossFade(float dt)
{
    if (!m_registry || m_character == INVALID_ENTITY)
        return;
    if (!m_registry->hasComponent<AnimationComponent>(m_character))
        return;
    auto& anim = m_registry->getComponent<AnimationComponent>(m_character);
    if (anim.clips.size() < 2)
        return;

    constexpr float segmentSec = 6.f;
    constexpr float crossFadeSec = 0.45f;
    m_animClipTimer += dt;
    const float cycle = segmentSec * 2.f;
    const float t = std::fmod(m_animClipTimer, cycle);
    const int seg = (t < segmentSec) ? 0 : 1;
    if (seg != m_animClipSegment) {
        m_animClipSegment = seg;
        anim.requestCrossFadeToClip(seg, crossFadeSec);
    }
}

void SuperHero::updateThirdPersonCamera(float /*dt*/)
{
    if (!m_registry || m_camera == INVALID_ENTITY || !m_registry->hasComponent<CameraComponent>(m_camera) ||
        !m_registry->hasComponent<ThirdPersonComponent>(m_camera) || !m_registry->hasComponent<TransformComponent>(m_camera))
        return;
    if (m_character == INVALID_ENTITY || !m_registry->hasComponent<TransformComponent>(m_character))
        return;
    if (!m_renderer || !m_renderer->window())
        return;

    m_physicsSys.grid.clear();
    for (Entity ge : m_registry->getEntitiesWith<TransformComponent>()) {
        auto& gt = m_registry->getComponent<TransformComponent>(ge);
        m_physicsSys.grid.insert(ge, gt.position);
    }

    GLFWwindow* w = m_renderer->window();
    auto& cam = m_registry->getComponent<CameraComponent>(m_camera);
    auto& tp = m_registry->getComponent<ThirdPersonComponent>(m_camera);
    auto& camTf = m_registry->getComponent<TransformComponent>(m_camera);
    auto& charTf = m_registry->getComponent<TransformComponent>(m_character);

    if (glfwGetWindowAttrib(w, GLFW_FOCUSED)) {
        double mx = 0.0;
        double my = 0.0;
        glfwGetCursorPos(w, &mx, &my);
        if (!m_cameraMouseInitialized) {
            m_lastCamMouseX = mx;
            m_lastCamMouseY = my;
            m_cameraMouseInitialized = true;
        } else {
            const float dx = static_cast<float>(mx - m_lastCamMouseX);
            const float dy = static_cast<float>(my - m_lastCamMouseY);
            m_lastCamMouseX = mx;
            m_lastCamMouseY = my;
            tp.orbitYaw -= dx * tp.mouseSensitivity;
            tp.orbitPitch -= dy * tp.mouseSensitivity;
            constexpr float pitchLimit = 1.45f;
            if (tp.orbitPitch > pitchLimit)
                tp.orbitPitch = pitchLimit;
            if (tp.orbitPitch < -pitchLimit)
                tp.orbitPitch = -pitchLimit;
        }
    }

    const Vec3 pivot{
        charTf.position.x,
        charTf.position.y + tp.orbitPivotHeight,
        charTf.position.z};
    const Vec3 target{
        charTf.position.x + cam.lookAtOffset.x,
        charTf.position.y + cam.lookAtOffset.y,
        charTf.position.z + cam.lookAtOffset.z};

    const float cy = std::cos(tp.orbitYaw);
    const float sy = std::sin(tp.orbitYaw);
    const float cp = std::cos(tp.orbitPitch);
    const float sp = std::sin(tp.orbitPitch);
    const Vec3 orbitDir{cp * sy, sp, cp * cy};
    const Vec3 eyeDesired{
        pivot.x + tp.orbitDistance * orbitDir.x,
        pivot.y + tp.orbitDistance * orbitDir.y,
        pivot.z + tp.orbitDistance * orbitDir.z};

    Vec3 eye = eyeDesired;
    if (tp.cameraCollisionEnabled) {
        ThirdPersonCameraCollisionSystem::Settings camCol{};
        camCol.cameraCollisionRadius = tp.cameraCollisionRadius;
        camCol.minOrbitDistance = tp.cameraMinOrbitDistance;
        camCol.groundClearance = tp.cameraGroundClearance;
        camCol.sideProbeOffset = tp.cameraSideProbeOffset;
        camCol.obstructionRelaxMeters = tp.cameraObstructionRelaxMeters;
        camCol.obstructionLayerMask = tp.cameraObstructionLayerMask;
        camCol.blockDynamicColliders = tp.cameraBlockDynamicColliders;
        ThirdPersonCameraCollisionSystem::resolve(
            *m_registry,
            m_physicsSys.grid,
            &m_terrainHeights,
            m_character,
            pivot,
            eyeDesired,
            camCol,
            eye);
    }

    camTf.position = eye;
    cam.viewMatrix = Mat4::inverse(Mat4::LookAt(eye, target, {0.f, 1.f, 0.f}));

    const Vec3 toTarget{target.x - eye.x, target.y - eye.y, target.z - eye.z};
    Vec3 flat{toTarget.x, 0.f, toTarget.z};
    if (lengthSquared(flat) < 1e-10f) {
        tp.planarMoveForward = {0.f, 0.f, -1.f};
    } else {
        tp.planarMoveForward = normalize(flat);
    }
    tp.planarMoveRight = normalize(cross({0.f, 1.f, 0.f}, tp.planarMoveForward));
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

    if (m_registry->hasComponent<ThirdPersonComponent>(m_camera)) {
        auto& tpInit = m_registry->getComponent<ThirdPersonComponent>(m_camera);
        tpInit.orbitYaw = 3.14159265f;
        tpInit.orbitPitch = 0.22f;
        tpInit.orbitDistance = 8.35f;
        tpInit.orbitPivotHeight = 0.9f;
        tpInit.mouseSensitivity = 0.0025f;
    }

    if (GLFWwindow* win = m_renderer->window()) {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    m_physicsSys.useProceduralTerrainGround = true;
    m_physicsSys.terrainHeightField = &m_terrainHeights;
    m_physicsSys.grid.cellSize = 8.f;

    {
        Entity terrainSettingsEntity = m_registry->createEntity();
        TerrainSettingsComponent ts{};
        if (tryLoadTerrainMap(m_terrainMap)) {
            ts.chunkSize = m_terrainMap.chunkSize;
            ts.scale = m_terrainMap.scale;
            ts.renderRadius = m_terrainMap.renderRadius;
        }
        m_registry->addComponent(terrainSettingsEntity, ts);

        auto& ctf0 = m_registry->getComponent<TransformComponent>(m_character);
        m_terrainChunks.update(
            *m_registry,
            ctf0.position,
            &m_terrainHeights,
            m_terrainMap.loaded ? &m_terrainMap : nullptr);
        float surfaceY = ctf0.position.y;
        if (m_terrainHeights.trySampleHeight(ctf0.position.x, ctf0.position.z, surfaceY)) {
            // Match PhysicsSystem::resolveGround (feet at surfaceY + kTerrainFootClearance).
            constexpr float capOy = 0.72f;
            constexpr float capHalfH = 0.40f;
            constexpr float capR = 0.26f;
            ctf0.position.y = surfaceY + kTerrainFootClearance - capOy + capHalfH + capR;
        } else
            ctf0.position.y = 2.0f;
    }

    {
        RigidBodyComponent playerBody{};
        playerBody.mass = 1.f;
        playerBody.invMass = 1.f;
        playerBody.linearDamping = 2.8f;
        playerBody.friction = 0.38f;
        m_registry->addComponent(m_character, playerBody);

        CapsuleColliderComponent playerCap{};
        playerCap.radius = 0.26f;
        playerCap.halfHeight = 0.40f;
        playerCap.offset = {0.f, 0.82f, 0.f};
        m_registry->addComponent(m_character, playerCap);

        ColliderFilterComponent playerLayers{};
        playerLayers.categoryBits = CollisionLayer::Player;
        playerLayers.collideMask = 0xFFFFFFFFu;
        m_registry->addComponent(m_character, playerLayers);
    }

    m_registry->addComponent(m_character, MovementComponent{});

    m_aiChaser = game::factories::spawnBusinessManCharacter(
        *m_registry, *m_renderer, *m_assetManager, scene.assetCacheKey, {5.f, 2.f, -3.f}, 0.f, false);
    if (m_aiChaser != INVALID_ENTITY) {
        MovementComponent chaseMove{};
        chaseMove.runSpeed = 2.35f;
        chaseMove.walkSpeed = 1.6f;
        chaseMove.sprintSpeed = 2.8f;
        chaseMove.acceleration = 14.f;
        m_registry->addComponent(m_aiChaser, chaseMove);
        m_registry->addComponent(m_aiChaser, ChaseComponent{m_character});

        auto& aiTf = m_registry->getComponent<TransformComponent>(m_aiChaser);
        float aiY = aiTf.position.y;
        if (m_terrainHeights.trySampleHeight(aiTf.position.x, aiTf.position.z, aiY)) {
            constexpr float capOy = 0.72f;
            constexpr float capHalfH = 0.40f;
            constexpr float capR = 0.26f;
            aiTf.position.y = aiY + kTerrainFootClearance - capOy + capHalfH + capR;
        }
    }

    // Dynamic prop boxes along the same world direction as the head ray (local +Z → character rotation).
    // `PrimitiveBoxComponent` draws them; place in front of the player so the camera sees them.
    constexpr float propBoxHalfY = 0.18f;
    constexpr float gapAboveHead = 0.08f;
    constexpr float playerSpawnY = 2.f;
    constexpr float capTop = playerSpawnY + 0.72f + 0.40f + 0.26f;
    const float propBoxCenterY = capTop + gapAboveHead + propBoxHalfY;
    const auto& charTf0 = m_registry->getComponent<TransformComponent>(m_character);
    const Vec3 rayLocal{0.f, 0.f, 1.f};
    Vec3 worldRayDir = Mat4::transformDirection(Mat4::FromQuat(charTf0.rotation), rayLocal);
    worldRayDir = normalize(worldRayDir);
    const Vec3 rayAnchor{charTf0.position.x, propBoxCenterY, charTf0.position.z};
    const Vec3 propPos1{
        rayAnchor.x + worldRayDir.x * 2.5f,
        rayAnchor.y + worldRayDir.y * 2.5f,
        rayAnchor.z + worldRayDir.z * 2.5f};
    const Vec3 propPos2{
        rayAnchor.x + worldRayDir.x * 5.0f,
        rayAnchor.y + worldRayDir.y * 5.0f,
        rayAnchor.z + worldRayDir.z * 5.0f};

    m_headBox = m_registry->createEntity();
    m_registry->addComponent(m_headBox, TransformComponent{});
    m_registry->addComponent(m_headBox, WorldTransformComponent{});
    {
        auto& htf = m_registry->getComponent<TransformComponent>(m_headBox);
        htf.position = propPos1;
        htf.rotation = {0.f, 0.f, 0.f, 1.f};

        RigidBodyComponent headRb{};
        headRb.mass = 1.f;
        headRb.invMass = 1.f;
        headRb.linearDamping = 4.5f;
        headRb.friction = 0.88f;
        m_registry->addComponent(m_headBox, headRb);

        BoxColliderComponent headCol{};
        headCol.halfExtents = {0.42f, propBoxHalfY, 0.42f};
        headCol.offset = {0.f, 0.f, 0.f};
        m_registry->addComponent(m_headBox, headCol);

        ColliderFilterComponent headPropLayers{};
        headPropLayers.categoryBits = CollisionLayer::Prop;
        headPropLayers.collideMask = 0xFFFFFFFFu;
        m_registry->addComponent(m_headBox, headPropLayers);

        PrimitiveBoxComponent headVis{};
        headVis.halfExtents = headCol.halfExtents;
        headVis.color[0] = 0.92f;
        headVis.color[1] = 0.62f;
        headVis.color[2] = 0.18f;
        m_registry->addComponent(m_headBox, headVis);
    }

    m_headBox2 = m_registry->createEntity();
    m_registry->addComponent(m_headBox2, TransformComponent{});
    m_registry->addComponent(m_headBox2, WorldTransformComponent{});
    {
        auto& htf = m_registry->getComponent<TransformComponent>(m_headBox2);
        htf.position = propPos2;
        htf.rotation = {0.f, 0.f, 0.f, 1.f};

        RigidBodyComponent headRb{};
        headRb.mass = 1.f;
        headRb.invMass = 1.f;
        headRb.linearDamping = 4.5f;
        headRb.friction = 0.88f;
        m_registry->addComponent(m_headBox2, headRb);

        BoxColliderComponent headCol{};
        headCol.halfExtents = {0.42f, propBoxHalfY, 0.42f};
        headCol.offset = {0.f, 0.f, 0.f};
        m_registry->addComponent(m_headBox2, headCol);

        ColliderFilterComponent head2PropLayers{};
        head2PropLayers.categoryBits = CollisionLayer::Prop;
        head2PropLayers.collideMask = 0xFFFFFFFFu;
        m_registry->addComponent(m_headBox2, head2PropLayers);

        PrimitiveBoxComponent headVis{};
        headVis.halfExtents = headCol.halfExtents;
        headVis.color[0] = 0.95f;
        headVis.color[1] = 0.45f;
        headVis.color[2] = 0.15f;
        m_registry->addComponent(m_headBox2, headVis);
    }

    if (m_registry->hasComponent<SkeletonComponent>(m_character)) {
        auto& sk = m_registry->getComponent<SkeletonComponent>(m_character);
        const int headBi = findBoneIndexByNameSubstring(sk, "Head");
        if (headBi >= 0) {
            m_headRay = m_registry->createEntity();
            m_registry->addComponent(m_headRay, TransformComponent{});
            BoneAttachmentComponent headAttach{};
            headAttach.skeletonEntity = m_character;
            headAttach.boneIndex = headBi;
            headAttach.localOffset = {0.f, 0.1f, 0.08f};
            m_registry->addComponent(m_headRay, headAttach);
            RaycastComponent ray{};
            ray.localDirection = {0.f, 0.f, 1.f};
            ray.maxDistance = 45.f;
            // Single ray: set `useCone = false` (default). Cone: `useCone = true` and tune rings/segments
            // (total dirs ≈ 1 + coneRings * coneSegments, max 48).
            ray.useCone = true;
            ray.coneHalfAngleDeg = 14.f;
            ray.coneRings = 2;
            ray.coneSegments = 10;
            ray.layerMask = CollisionLayer::MaskAllButPlayer;
            ray.ignoreEntity = m_character;
            ray.debugDraw = true;
            m_registry->addComponent(m_headRay, ray);
        }
    }

    EntityAttachmentSystem attachEntSys;
    attachEntSys.update(*m_registry);
    WorldTransformSyncSystem worldSnap;
    worldSnap.update(*m_registry);
    updateThirdPersonCamera(0.f);
}

void SuperHero::onInput()
{
    if (!m_renderer || !m_renderer->window())
        return;
    GLFWwindow* w = m_renderer->window();
    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS)
        m_shouldClose = true;

    const int lNow = glfwGetKey(w, GLFW_KEY_L);
    if (lNow == GLFW_PRESS && !m_debugHudKeyLHeld)
        m_debugHudEnabled = !m_debugHudEnabled;
    m_debugHudKeyLHeld = (lNow == GLFW_PRESS);
}

void SuperHero::onUpdate(double dt)
{
    if (m_shouldClose || !m_registry)
        return;

    const float fdt = static_cast<float>(dt);
    m_handAnimTime += fdt;
    updateAnimClipCrossFade(fdt);

    if (m_character != INVALID_ENTITY && m_registry->hasComponent<TransformComponent>(m_character)) {
        const Vec3 pp = m_registry->getComponent<TransformComponent>(m_character).position;
        m_terrainChunks.update(
            *m_registry,
            pp,
            &m_terrainHeights,
            m_terrainMap.loaded ? &m_terrainMap : nullptr);
    }
    m_physicsSys.terrainHeightField = &m_terrainHeights;

    updateThirdPersonCamera(fdt);

    GLFWwindow* w = m_renderer ? m_renderer->window() : nullptr;
    if (w)
        m_playerController.update(*m_registry, w, m_camera, fdt);
    m_aiController.update(*m_registry);
    m_movementSystem.update(*m_registry, fdt);
    m_physicsSys.update(*m_registry, fdt);
    m_collisionSystem.update(*m_registry, fdt);

    AnimationSystem animSys;
    animSys.update(*m_registry, fdt);

    PoseSystem poseSys;
    poseSys.update(*m_registry);

    updateRightArmAim();

    BoneControlSystem boneCtrl;
    boneCtrl.update(*m_registry);

    poseSys.update(*m_registry);

    if (m_character != INVALID_ENTITY)
        game::factories::updateCharacterSkinPalette(*m_registry, m_character);
    if (m_aiChaser != INVALID_ENTITY)
        game::factories::updateCharacterSkinPalette(*m_registry, m_aiChaser);

    BoneAttachmentSystem attachSys;
    attachSys.update(*m_registry);

    EntityAttachmentSystem entityAttachSys;
    entityAttachSys.update(*m_registry);

    WorldTransformSyncSystem worldSys;
    worldSys.update(*m_registry);

    m_raycastSys.update(*m_registry, m_physicsSys.grid);

    if (m_debugHudEnabled && m_headRay != INVALID_ENTITY && m_registry->hasComponent<RaycastComponent>(m_headRay)) {
        const auto& hr = m_registry->getComponent<RaycastComponent>(m_headRay);
        if (hr.hasHit) {
            char buf[160];
            std::snprintf(
                buf,
                sizeof(buf),
                "Head cone hit: t=%.2f entity=%u\n",
                hr.hitDistance,
                static_cast<unsigned>(hr.hitEntity));
            m_debugDetailText += buf;
        }
    }
}

void SuperHero::onRender(double)
{
    if (m_shouldClose || !m_renderer || !m_registry)
        return;

    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    if (m_haveFrameTime) {
        const double frameDt = std::chrono::duration<double>(now - m_lastFrameTime).count();
        if (frameDt > 1e-9) {
            const float inst = static_cast<float>(1.0 / frameDt);
            m_fpsSmooth = m_fpsSmooth * 0.92f + inst * 0.08f;
        }
    } else {
        m_haveFrameTime = true;
    }
    m_lastFrameTime = now;

    OpenGLDebugHudSnapshot hud;
    hud.enabled = m_debugHudEnabled;
    hud.fps = m_fpsSmooth;
    hud.entityCount = static_cast<int>(m_registry->getAliveEntityCount());
    hud.targetFpsPreset = m_settings.targetFpsPreset;
    hud.locomotionState = "SuperHero";
    if (m_character != INVALID_ENTITY && m_registry->hasComponent<MovementComponent>(m_character))
        hud.movementState = movementStateToString(m_registry->getComponent<MovementComponent>(m_character).state);
    if (m_debugHudEnabled)
        hud.debugDetail = m_debugDetailText;
    m_renderer->setDebugHudSnapshot(std::move(hud));

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
