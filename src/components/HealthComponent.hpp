#pragma once

/// Simple health pool; gameplay systems clamp `current` to [0, max].
struct HealthComponent {
    float current = 100.f;
    float max = 100.f;
    /// Seconds until another AI contact can apply damage.
    float contactHitCooldown = 0.f;
};
