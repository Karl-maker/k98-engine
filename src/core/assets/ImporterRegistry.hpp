#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "importers/IAssetImporter.hpp"

class ImporterRegistry {
private:
    std::unordered_map<std::string, std::shared_ptr<IAssetImporter>> importers;

public:
    void registerImporter(const std::string& extension,
                          std::shared_ptr<IAssetImporter> importer) {
        importers[extension] = importer;
    }

    std::shared_ptr<IAssetImporter> getImporter(const std::string& extension) {
        if (importers.count(extension)) {
            return importers[extension];
        }
        return nullptr;
    }
};