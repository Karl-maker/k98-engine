#pragma once

#include "../../ecs/Entity.hpp"
#include "../../math/Vec3.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace spawn {

using json = nlohmann::json;

/// One row from a spawn catalog — factories interpret `attributes` and `cluster`.
/// Per-cell JSON files (`catalog_(gx,gz).json`) may use `localPosition` instead of `position`; resolved at load time.
struct SpawnEntryDesc {
    std::string id;
    Vec3 position{};
    std::string archetype;
    float spawnProbability = 1.f;
    float spawnRadius = 40.f;
    float despawnRadius = 70.f;
    json attributes = json::object();
    /// Optional: `{ "count": 4, "radius": 2 }` or `{ "instances": [ { "offset": { "x", "y", "z" } }, ... ] }`
    json cluster = json::object();
};

struct SpawnCatalogData {
    int version = 1;
    uint32_t worldSeed = 0;
    std::vector<SpawnEntryDesc> spawns;
};

struct SpawnResult {
    std::vector<Entity> entities;
};

inline SpawnResult ok(Entity e)
{
    SpawnResult r;
    if (e != INVALID_ENTITY)
        r.entities.push_back(e);
    return r;
}

inline SpawnResult ok(std::vector<Entity> es)
{
    SpawnResult r;
    r.entities = std::move(es);
    return r;
}

} // namespace spawn
