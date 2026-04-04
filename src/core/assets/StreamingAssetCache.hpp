#pragma once

#include "AssetManager.hpp"
#include "IAsset.hpp"
#include "../../utils/ProximityUtils.hpp"
#include "../../math/Vec3.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/// Proximity + view-scored LRU cache for non-skeleton assets (textures, buffers, etc.).
class StreamingAssetCache {
public:
    explicit StreamingAssetCache(AssetManager& assets)
        : m_assets(assets) {}

    void setMemoryBudgetBytes(std::size_t bytes) { m_budgetBytes = bytes; }
    void setViewerPosition(const Vec3& p) { m_viewer = p; }

    /// Higher score = more important to keep resident.
    float scoreForDistance(float distance) const {
        return 1.0f / (1.0f + distance);
    }

    bool isLikelyVisible(const Vec3& objectCenter, const Vec3& /*cameraForward*/) const {
        (void)objectCenter;
        // Placeholder until frustum culling: treat all as visible when in front heuristic.
        return true;
    }

    template<typename T>
    std::shared_ptr<T> acquire(const std::string& path, const Vec3& objectWorldCenter, const Vec3& cameraForward) {
        std::lock_guard<std::mutex> lock(m_mutex);
        touchLocked(path);

        float dist = ProximityUtils::distance(m_viewer, objectWorldCenter);
        float viewScore = isLikelyVisible(objectWorldCenter, cameraForward) ? 1.0f : 0.25f;
        float priority = scoreForDistance(dist) * viewScore;

        std::shared_ptr<IAsset> cached = m_assets.load(path);
        auto cast = std::dynamic_pointer_cast<T>(cached);
        if (!cast)
            return nullptr;

        Entry& e = m_entries[path];
        e.bytes = estimateBytes(*cast);
        e.lastPriority = priority;
        e.asset = cached;

        evictIfNeededLocked();
        return cast;
    }

    void touch(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        touchLocked(path);
    }

    void evict(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.erase(path);
        auto it = std::find(m_lru.begin(), m_lru.end(), path);
        if (it != m_lru.end())
            m_lru.erase(it);
    }

private:
    struct Entry {
        std::shared_ptr<IAsset> asset;
        std::size_t bytes = 0;
        float lastPriority = 0.f;
    };

    void touchLocked(const std::string& path) {
        auto it = std::find(m_lru.begin(), m_lru.end(), path);
        if (it != m_lru.end())
            m_lru.erase(it);
        m_lru.push_back(path);
    }

    template<typename T>
    static std::size_t estimateBytes(const T& asset) {
        (void)asset;
        return 1024;
    }

    void evictIfNeededLocked() {
        std::size_t total = 0;
        for (const auto& kv : m_entries)
            total += kv.second.bytes;

        while (total > m_budgetBytes && !m_lru.empty()) {
            std::string victim = m_lru.front();
            m_lru.pop_front();
            auto it = m_entries.find(victim);
            if (it != m_entries.end()) {
                total -= it->second.bytes;
                m_entries.erase(it);
                m_assets.unload(victim);
            }
        }
    }

    AssetManager& m_assets;
    Vec3 m_viewer{};
    std::size_t m_budgetBytes = 64u * 1024u * 1024u;
    std::unordered_map<std::string, Entry> m_entries;
    std::deque<std::string> m_lru;
    std::mutex m_mutex;
};
