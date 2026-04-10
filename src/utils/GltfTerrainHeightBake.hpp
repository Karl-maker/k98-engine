#pragma once

#include "../components/HeightMapComponent.hpp"
#include "../core/assets/ModelAsset.hpp"
#include "../math/MathOps.hpp"
#include "../math/Vec3.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

// -----------------------------------------------------------------------------
// Project a triangle mesh onto the XZ plane and sample world Y at each heightmap
// grid corner. Used so TerrainHeightField / PhysicsSystem still treat streamed
// chunks as height-based "ground" while visuals can come from the same glTF.
// -----------------------------------------------------------------------------

namespace detail {

inline bool pointInTriangleXZ(float px, float pz, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const auto cross2 = [](float x0, float z0, float x1, float z1) { return x0 * z1 - x1 * z0; };
    const float c0 = cross2(b.x - a.x, b.z - a.z, px - a.x, pz - a.z);
    const float c1 = cross2(c.x - b.x, c.z - b.z, px - b.x, pz - b.z);
    const float c2 = cross2(a.x - c.x, a.z - c.z, px - c.x, pz - c.z);
    const float eps = 1e-5f;
    const bool hasNeg = (c0 < -eps) || (c1 < -eps) || (c2 < -eps);
    const bool hasPos = (c0 > eps) || (c1 > eps) || (c2 > eps);
    return !(hasNeg && hasPos);
}

/// Interpolate Y at (wx, wz) from triangle plane; false if triangle is vertical in XZ.
inline bool heightOnTriangleAtXZ(float wx, float wz, const Vec3& a, const Vec3& b, const Vec3& c, float& outY)
{
    const Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
    const Vec3 n = cross(e1, e2);
    if (std::fabs(n.y) < 1e-7f)
        return false;
    const float d = dot(n, a);
    outY = (d - n.x * wx - n.z * wz) / n.y;
    return true;
}

} // namespace detail

/// For each heightmap sample, take the highest intersecting triangle (handles overlaps / skirts).
inline void bakeMeshTrianglesToHeightMap(
    const Mesh& mesh,
    int chunkX,
    int chunkZ,
    float stride,
    float cellSize,
    float uniformScale,
    HeightMapComponent& hm,
    float fallbackHeight)
{
    const int n = hm.size;
    if (n <= 0 || mesh.vertices.empty())
        return;

    const float ox = static_cast<float>(chunkX) * stride;
    const float oz = static_cast<float>(chunkZ) * stride;

    auto processTri = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2) {
        const Vec3 w0{ox + uniformScale * p0.x, uniformScale * p0.y, oz + uniformScale * p0.z};
        const Vec3 w1{ox + uniformScale * p1.x, uniformScale * p1.y, oz + uniformScale * p1.z};
        const Vec3 w2{ox + uniformScale * p2.x, uniformScale * p2.y, oz + uniformScale * p2.z};

        float minX = std::min({w0.x, w1.x, w2.x});
        float maxX = std::max({w0.x, w1.x, w2.x});
        float minZ = std::min({w0.z, w1.z, w2.z});
        float maxZ = std::max({w0.z, w1.z, w2.z});

        int ix0 = static_cast<int>(std::floor((minX - ox) / cellSize));
        int ix1 = static_cast<int>(std::ceil((maxX - ox) / cellSize));
        int iz0 = static_cast<int>(std::floor((minZ - oz) / cellSize));
        int iz1 = static_cast<int>(std::ceil((maxZ - oz) / cellSize));
        ix0 = std::max(0, std::min(n - 1, ix0));
        ix1 = std::max(0, std::min(n - 1, ix1));
        iz0 = std::max(0, std::min(n - 1, iz0));
        iz1 = std::max(0, std::min(n - 1, iz1));
        if (ix0 > ix1 || iz0 > iz1)
            return;

        for (int iz = iz0; iz <= iz1; ++iz) {
            for (int ix = ix0; ix <= ix1; ++ix) {
                const float wx = ox + static_cast<float>(ix) * cellSize;
                const float wz = oz + static_cast<float>(iz) * cellSize;
                if (!detail::pointInTriangleXZ(wx, wz, w0, w1, w2))
                    continue;
                float y = fallbackHeight;
                if (!detail::heightOnTriangleAtXZ(wx, wz, w0, w1, w2, y))
                    continue;
                const float cur = hm.get(ix, iz);
                if (y > cur)
                    hm.set(ix, iz, y);
            }
        }
    };

    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x)
            hm.set(x, z, fallbackHeight);
    }

    if (!mesh.indices.empty()) {
        for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const int i0 = mesh.indices[t];
            const int i1 = mesh.indices[t + 1];
            const int i2 = mesh.indices[t + 2];
            if (i0 < 0 || i1 < 0 || i2 < 0 ||
                static_cast<size_t>(i0) >= mesh.vertices.size() ||
                static_cast<size_t>(i1) >= mesh.vertices.size() ||
                static_cast<size_t>(i2) >= mesh.vertices.size())
                continue;
            processTri(
                mesh.vertices[static_cast<size_t>(i0)].position,
                mesh.vertices[static_cast<size_t>(i1)].position,
                mesh.vertices[static_cast<size_t>(i2)].position);
        }
    } else {
        for (size_t t = 0; t + 2 < mesh.vertices.size(); t += 3) {
            processTri(
                mesh.vertices[t].position,
                mesh.vertices[t + 1].position,
                mesh.vertices[t + 2].position);
        }
    }
}
