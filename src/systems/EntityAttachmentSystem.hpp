#pragma once

#include "../components/EntityAttachmentComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../math/MathOps.hpp"
#include "../math/Mat4.hpp"

/// Keeps attached entities aligned with their parent each frame (position; optional orientation).
class EntityAttachmentSystem {
public:
    void update(Registry& registry)
    {
        for (Entity e : registry.getEntitiesWith<EntityAttachmentComponent, TransformComponent>()) {
            auto& att = registry.getComponent<EntityAttachmentComponent>(e);
            auto& child = registry.getComponent<TransformComponent>(e);
            if (att.parent == INVALID_ENTITY || !registry.hasComponent<TransformComponent>(att.parent))
                continue;

            const auto& parent = registry.getComponent<TransformComponent>(att.parent);

            Vec3 worldOffset;
            if (att.rotateOffsetWithParent) {
                worldOffset = Mat4::transformDirection(Mat4::FromQuat(parent.rotation), att.localOffset);
            } else {
                worldOffset = att.localOffset;
            }

            const Vec3 targetPos{
                parent.position.x + worldOffset.x,
                parent.position.y + worldOffset.y,
                parent.position.z + worldOffset.z};

            float fb = att.followBlend;
            if (fb < 0.f)
                fb = 0.f;
            else if (fb > 1.f)
                fb = 1.f;

            if (fb >= 1.f - 1e-6f) {
                child.position = targetPos;
            } else {
                child.position = vec3Lerp(child.position, targetPos, fb);
            }

            if (att.inheritParentOrientation) {
                const Quat targetRot = quatNormalize(quatMul(parent.rotation, att.localRotation));
                if (fb >= 1.f - 1e-6f)
                    child.rotation = targetRot;
                else
                    child.rotation = quatSlerp(child.rotation, targetRot, fb);
            }
        }
    }
};
