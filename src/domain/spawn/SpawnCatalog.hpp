#pragma once

#include "SpawnTypes.hpp"

#include <cstddef>
#include <string>

namespace spawn {

bool loadSpawnCatalogFromFile(const std::string& path, SpawnCatalogData& out);

/// Builds `{directory}/{baseName}_(gx,gz).json` (directory may omit trailing slash).
std::string formatSpawnCatalogPathForGridCell(const std::string& directory, const std::string& baseName, int gx, int gz);

/// Loads the catalog for terrain/map grid cell `(gx, gz)`. Uses the same world stride as terrain chunks
/// (`chunkSize * scale`) so indices match `TerrainChunkSystem` chunk indices.
/// Entries may use `"position"` (world) or `"localPosition"` (relative to chunk corner `(gx*stride, 0, gz*stride)`).
/// Tries each path in `searchRoots` until a file exists; if none exist, clears `out` and returns true.
/// Returns false only on JSON read/parse errors for an existing file.
bool loadSpawnCatalogForGridCell(
    const char* const* searchRoots,
    std::size_t numSearchRoots,
    const std::string& baseName,
    int gx,
    int gz,
    float gridStrideWorld,
    SpawnCatalogData& out);

/// Merges spawn entries from `(centerGx + dx, centerGz + dz)` for all `dx,dz` in `[-neighborRadius, neighborRadius]`.
/// The center cell is merged first (its `worldSeed` wins). Duplicate `id`s from neighbor files are skipped so
/// spawns in adjacent chunk catalogs are visible while the player stands in a neighboring cell.
/// Returns false if any existing catalog file fails to parse.
bool loadSpawnCatalogMergedNeighborhood(
    const char* const* searchRoots,
    std::size_t numSearchRoots,
    const std::string& baseName,
    int centerGx,
    int centerGz,
    float gridStrideWorld,
    int neighborRadius,
    SpawnCatalogData& out);

} // namespace spawn
