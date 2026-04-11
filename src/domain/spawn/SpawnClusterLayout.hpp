#pragma once

#include "SpawnTypes.hpp"

#include "../../math/Vec3.hpp"

#include <cmath>
#include <vector>

namespace spawn {

/// Resolves `cluster` JSON into world positions (anchor + offsets or ring layout).
inline std::vector<Vec3> clusterWorldPositions(const Vec3& anchor, const json& cluster)
{
    std::vector<Vec3> out;
    if (!cluster.is_object() || cluster.empty()) {
        out.push_back(anchor);
        return out;
    }
    if (cluster.contains("instances") && cluster["instances"].is_array()) {
        for (const auto& inst : cluster["instances"]) {
            Vec3 o{anchor};
            if (inst.is_object() && inst.contains("offset") && inst["offset"].is_object()) {
                const auto& off = inst["offset"];
                o.x += off.value("x", 0.f);
                o.y += off.value("y", 0.f);
                o.z += off.value("z", 0.f);
            }
            out.push_back(o);
        }
        if (out.empty())
            out.push_back(anchor);
        return out;
    }

    int count = cluster.value("count", 1);
    if (count < 1)
        count = 1;
    const float radius = cluster.value("radius", 1.f);
    if (count == 1) {
        out.push_back(anchor);
        return out;
    }
    constexpr float twoPi = 6.28318530718f;
    for (int i = 0; i < count; ++i) {
        const float a = twoPi * static_cast<float>(i) / static_cast<float>(count);
        out.push_back(
            {anchor.x + std::cos(a) * radius, anchor.y, anchor.z + std::sin(a) * radius});
    }
    return out;
}

} // namespace spawn
