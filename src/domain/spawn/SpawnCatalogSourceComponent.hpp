#pragma once

#include <string>

/// Marks entities created from `catalog.json` and stores the spawn row id.
struct SpawnCatalogSourceComponent {
    std::string spawnEntryId;
};
