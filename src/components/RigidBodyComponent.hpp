#pragma once
#include "../math/Vec3.hpp"

struct RigidBodyComponent
{
    Vec3 velocity{0,0,0};
    Vec3 forces{0,0,0};

    float mass = 1.0f;
    float invMass = 1.0f;

    /// Exponential velocity damping per second (0 = disabled). Reduces jitter on contacts.
    float linearDamping = 0.f;

    bool isGrounded = false;
};