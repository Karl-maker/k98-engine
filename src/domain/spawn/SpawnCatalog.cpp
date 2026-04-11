#include "SpawnCatalog.hpp"

#include <cmath>
#include <fstream>
#include <iostream>

namespace spawn {

static bool readFile(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return false;
    const std::streamsize sz = f.tellg();
    if (sz <= 0)
        return false;
    f.seekg(0);
    out.resize(static_cast<size_t>(sz));
    f.read(out.data(), sz);
    return static_cast<bool>(f);
}

static bool fileExists(const std::string& path)
{
    std::ifstream f(path);
    return static_cast<bool>(f);
}

static bool parseEntryCommonTail(const json& o, SpawnEntryDesc& e)
{
    e.spawnProbability = o.value("spawnProbability", 1.f);
    e.spawnRadius = o.value("spawnRadius", 40.f);
    e.despawnRadius = o.value("despawnRadius", 70.f);
    if (e.despawnRadius < e.spawnRadius) {
        std::cerr << "SpawnCatalog: entry \"" << e.id << "\" despawnRadius < spawnRadius; clamping.\n";
        e.despawnRadius = e.spawnRadius;
    }
    if (e.spawnProbability < 0.f)
        e.spawnProbability = 0.f;
    if (e.spawnProbability > 1.f)
        e.spawnProbability = 1.f;
    if (o.contains("attributes") && o["attributes"].is_object())
        e.attributes = o["attributes"];
    else
        e.attributes = json::object();
    if (o.contains("cluster") && o["cluster"].is_object())
        e.cluster = o["cluster"];
    else
        e.cluster = json::object();
    return true;
}

static bool parseEntry(const json& o, SpawnEntryDesc& e)
{
    if (!o.is_object())
        return false;
    if (!o.contains("id") || !o["id"].is_string())
        return false;
    e.id = o["id"].get<std::string>();
    if (!o.contains("archetype") || !o["archetype"].is_string())
        return false;
    e.archetype = o["archetype"].get<std::string>();
    if (!o.contains("position") || !o["position"].is_object())
        return false;
    const auto& p = o["position"];
    e.position.x = p.value("x", 0.f);
    e.position.y = p.value("y", 0.f);
    e.position.z = p.value("z", 0.f);
    return parseEntryCommonTail(o, e);
}

/// `position` (world) or `localPosition` (relative to chunk corner at gx,gz with given stride).
static bool parseEntryForGridCell(const json& o, int gx, int gz, float gridStrideWorld, SpawnEntryDesc& e)
{
    if (!o.is_object())
        return false;
    if (!o.contains("id") || !o["id"].is_string())
        return false;
    e.id = o["id"].get<std::string>();
    if (!o.contains("archetype") || !o["archetype"].is_string())
        return false;
    e.archetype = o["archetype"].get<std::string>();

    const bool hasWorld = o.contains("position") && o["position"].is_object();
    const bool hasLocal = o.contains("localPosition") && o["localPosition"].is_object();
    if (hasWorld) {
        const auto& p = o["position"];
        e.position.x = p.value("x", 0.f);
        e.position.y = p.value("y", 0.f);
        e.position.z = p.value("z", 0.f);
    } else if (hasLocal) {
        const auto& lp = o["localPosition"];
        const float lx = lp.value("x", 0.f);
        const float ly = lp.value("y", 0.f);
        const float lz = lp.value("z", 0.f);
        const float ox = static_cast<float>(gx) * gridStrideWorld;
        const float oz = static_cast<float>(gz) * gridStrideWorld;
        e.position = {ox + lx, ly, oz + lz};
    } else {
        return false;
    }
    return parseEntryCommonTail(o, e);
}

static bool loadSpawnCatalogFromParsedRoot(const json& root, int gx, int gz, float gridStrideWorld, bool useGridCellPositions, SpawnCatalogData& out)
{
    out.version = root.value("version", 1);
    if (root.contains("worldSeed")) {
        if (root["worldSeed"].is_number_unsigned())
            out.worldSeed = root["worldSeed"].get<uint32_t>();
        else if (root["worldSeed"].is_number_integer())
            out.worldSeed = static_cast<uint32_t>(root["worldSeed"].get<int64_t>());
    }
    out.spawns.clear();
    if (!root.contains("spawns")) {
        if (useGridCellPositions)
            return true;
        std::cerr << "SpawnCatalog: missing \"spawns\" in catalog\n";
        return false;
    }
    if (!root["spawns"].is_array()) {
        std::cerr << "SpawnCatalog: \"spawns\" must be an array\n";
        return false;
    }
    for (const auto& el : root["spawns"]) {
        SpawnEntryDesc e{};
        const bool ok = useGridCellPositions ? parseEntryForGridCell(el, gx, gz, gridStrideWorld, e) : parseEntry(el, e);
        if (ok)
            out.spawns.push_back(std::move(e));
        else
            std::cerr << "SpawnCatalog: skipped invalid spawn entry\n";
    }
    return true;
}

bool loadSpawnCatalogFromFile(const std::string& path, SpawnCatalogData& out)
{
    std::string text;
    if (!readFile(path, text)) {
        std::cerr << "SpawnCatalog: could not read " << path << "\n";
        return false;
    }
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& ex) {
        std::cerr << "SpawnCatalog: JSON parse error in " << path << ": " << ex.what() << "\n";
        return false;
    }
    if (!root.contains("spawns") || !root["spawns"].is_array()) {
        std::cerr << "SpawnCatalog: missing \"spawns\" array in " << path << "\n";
        return false;
    }
    return loadSpawnCatalogFromParsedRoot(root, 0, 0, 0.f, false, out);
}

std::string formatSpawnCatalogPathForGridCell(const std::string& directory, const std::string& baseName, int gx, int gz)
{
    std::string d = directory;
    if (!d.empty() && d.back() != '/' && d.back() != '\\')
        d += '/';
    return d + baseName + "_(" + std::to_string(gx) + "," + std::to_string(gz) + ").json";
}

bool loadSpawnCatalogForGridCell(
    const char* const* searchRoots,
    std::size_t numSearchRoots,
    const std::string& baseName,
    int gx,
    int gz,
    float gridStrideWorld,
    SpawnCatalogData& out)
{
    out = SpawnCatalogData{};
    std::string chosen;
    for (std::size_t i = 0; i < numSearchRoots; ++i) {
        if (!searchRoots[i])
            continue;
        std::string path = formatSpawnCatalogPathForGridCell(searchRoots[i], baseName, gx, gz);
        if (fileExists(path)) {
            chosen = std::move(path);
            break;
        }
    }
    if (chosen.empty())
        return true;

    std::string text;
    if (!readFile(chosen, text)) {
        std::cerr << "SpawnCatalog: could not read " << chosen << "\n";
        return false;
    }
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& ex) {
        std::cerr << "SpawnCatalog: JSON parse error in " << chosen << ": " << ex.what() << "\n";
        return false;
    }
    return loadSpawnCatalogFromParsedRoot(root, gx, gz, gridStrideWorld, true, out);
}

} // namespace spawn
