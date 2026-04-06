#pragma once

#include "../ecs/Registry.hpp"
#include "../components/TerrainChunkComponent.hpp"
#include "../components/HeightMapComponent.hpp"
#include "../components/TerrainSettingsComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../utils/Biome.hpp"
#include "../utils/Noise.hpp"
#include "../utils/TerrainHeightField.hpp"

#include <cmath>
#include <cstdlib>

// -----------------------------------------------------------------------------
// Streams heightmap terrain chunks around the player. Fills HeightMapComponent
// using biome noise; mirrors data into TerrainHeightField for O(1) CPU sampling.
// -----------------------------------------------------------------------------

class TerrainChunkSystem {
public:
    void update(Registry& registry, Vec3 playerPos, TerrainHeightField* heightField)
    {
        auto settingsEntities = registry.getEntitiesWith<TerrainSettingsComponent>();
        if (settingsEntities.empty())
            return;

        auto& settings = registry.getComponent<TerrainSettingsComponent>(settingsEntities[0]);

        const int chunkSize = settings.chunkSize;
        const float cellSize = settings.scale;
        const float stride = static_cast<float>(chunkSize) * cellSize;
        const int radius = settings.renderRadius;

        const int playerChunkX = static_cast<int>(std::floor(playerPos.x / stride));
        const int playerChunkZ = static_cast<int>(std::floor(playerPos.z / stride));

        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                const int cx = playerChunkX + x;
                const int cz = playerChunkZ + z;

                if (!chunkExists(registry, cx, cz))
                    createChunk(registry, cx, cz, settings, heightField, stride, cellSize);
            }
        }

        auto chunks = registry.getEntitiesWith<TerrainChunkComponent>();

        for (Entity e : chunks) {
            auto& chunk = registry.getComponent<TerrainChunkComponent>(e);

            const int dx = chunk.chunkX - playerChunkX;
            const int dz = chunk.chunkZ - playerChunkZ;

            if (std::abs(dx) > radius || std::abs(dz) > radius) {
                if (heightField)
                    heightField->unregisterChunk(chunk.chunkX, chunk.chunkZ);
                registry.destroyEntity(e);
            }
        }
    }

private:
    static void fillHeightMap(HeightMapComponent& hm, int cx, int cz, float stride, float cellSize)
    {
        const int n = hm.size;
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const float wx = static_cast<float>(cx) * stride + static_cast<float>(x) * cellSize;
                const float wz = static_cast<float>(cz) * stride + static_cast<float>(z) * cellSize;
                BiomeComponent biome = getBiome(wx, wz);
                const float h = fbm(wx, wz, biome) * biome.heightMultiplier;
                hm.set(x, z, h);
            }
        }
    }

    static bool chunkExists(Registry& registry, int x, int z)
    {
        for (Entity e : registry.getEntitiesWith<TerrainChunkComponent>()) {
            const auto& c = registry.getComponent<TerrainChunkComponent>(e);
            if (c.chunkX == x && c.chunkZ == z)
                return true;
        }
        return false;
    }

    static void createChunk(
        Registry& registry,
        int cx,
        int cz,
        TerrainSettingsComponent& settings,
        TerrainHeightField* heightField,
        float stride,
        float cellSize)
    {
        Entity e = registry.createEntity();

        HeightMapComponent hm{};
        hm.size = settings.chunkSize + 1;
        hm.heights.assign(static_cast<size_t>(hm.size * hm.size), 0.0f);
        fillHeightMap(hm, cx, cz, stride, cellSize);

        TerrainChunkComponent tc{};
        tc.chunkX = cx;
        tc.chunkZ = cz;
        tc.size = settings.chunkSize;
        tc.scale = settings.scale;
        tc.generated = true;

        TransformComponent tf{};
        tf.position = {cx * stride, 0.0f, cz * stride};

        registry.addComponent(e, hm);
        registry.addComponent(e, tc);
        registry.addComponent(e, tf);
        registry.addComponent(e, WorldTransformComponent{});

        if (heightField) {
            const auto& stored = registry.getComponent<HeightMapComponent>(e);
            heightField->registerChunk(cx, cz, stored.size, settings.scale, stored.heights);
        }
    }
};
