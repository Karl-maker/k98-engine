#pragma once
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include "IAsset.hpp"
#include "ImporterRegistry.hpp"

class AssetManager {
private:
    std::unordered_map<std::string, std::shared_ptr<IAsset>> cache;
    /// In-flight async loads — coalesced so each path decodes once; completed jobs are removed here.
    std::unordered_map<std::string, std::shared_future<std::shared_ptr<IAsset>>> m_inFlight;
    ImporterRegistry registry;
    std::mutex m_loadMutex;

    struct AsyncLoadJob {
        std::string path;
        std::promise<std::shared_ptr<IAsset>> promise;
    };

    std::queue<AsyncLoadJob> m_asyncQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::thread m_asyncWorker;
    bool m_asyncWorkerStop = false;

    static std::shared_future<std::shared_ptr<IAsset>> readyFuture(std::shared_ptr<IAsset> asset)
    {
        std::promise<std::shared_ptr<IAsset>> p;
        p.set_value(std::move(asset));
        return p.get_future().share();
    }

    /// `m_loadMutex` must be held. Path must not already be in `cache`.
    std::shared_ptr<IAsset> importNewAssetLocked(const std::string& path)
    {
        auto dot = path.find_last_of('.');
        std::string ext = path.substr(dot + 1);

        auto importer = registry.getImporter(ext);
        if (!importer) {
            std::cerr << "No importer for ." << ext << std::endl;
            return nullptr;
        }

        auto asset = importer->import(path);
        asset->id = path;

        cache[path] = asset;
        return asset;
    }

    void ensureAsyncWorkerStarted()
    {
        std::lock_guard<std::mutex> qk(m_queueMutex);
        if (!m_asyncWorker.joinable())
            m_asyncWorker = std::thread([this] { asyncWorkerLoop(); });
    }

    void asyncWorkerLoop()
    {
        for (;;) {
            AsyncLoadJob job;
            {
                std::unique_lock<std::mutex> lk(m_queueMutex);
                m_queueCv.wait(lk, [&] { return m_asyncWorkerStop || !m_asyncQueue.empty(); });
                if (m_asyncWorkerStop && m_asyncQueue.empty())
                    return;
                job = std::move(m_asyncQueue.front());
                m_asyncQueue.pop();
            }

            try {
                std::shared_ptr<IAsset> asset;
                {
                    std::lock_guard<std::mutex> lk(m_loadMutex);
                    if (cache.count(job.path))
                        asset = cache[job.path];
                    else
                        asset = importNewAssetLocked(job.path);
                    m_inFlight.erase(job.path);
                }
                job.promise.set_value(asset);
            } catch (...) {
                std::lock_guard<std::mutex> lk(m_loadMutex);
                m_inFlight.erase(job.path);
                job.promise.set_exception(std::current_exception());
            }
        }
    }

public:
    AssetManager() = default;

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    ~AssetManager()
    {
        {
            std::lock_guard<std::mutex> lk(m_queueMutex);
            m_asyncWorkerStop = true;
        }
        m_queueCv.notify_all();
        if (m_asyncWorker.joinable())
            m_asyncWorker.join();
    }

    void registerImporter(const std::string& ext, std::shared_ptr<IAssetImporter> importer)
    {
        registry.registerImporter(ext, importer);
    }

    /// Synchronous load. If the same path is already decoding on the background queue, waits for it (no double decode).
    std::shared_ptr<IAsset> load(const std::string& path)
    {
        std::unique_lock<std::mutex> lock(m_loadMutex);
        if (cache.count(path))
            return cache[path];

        auto infl = m_inFlight.find(path);
        if (infl != m_inFlight.end()) {
            std::shared_future<std::shared_ptr<IAsset>> f = infl->second;
            lock.unlock();
            return f.get();
        }

        return importNewAssetLocked(path);
    }

    void unload(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        cache.erase(path);
    }

    /// Queue CPU decode (glTF + textures) on a **single** background worker — one asset at a time so many queued
    /// requests do not spawn many parallel decoders. Same path is still coalesced to one decode. GPU upload is separate.
    std::shared_future<std::shared_ptr<IAsset>> loadSharedAsync(const std::string& path)
    {
        std::unique_lock<std::mutex> lock(m_loadMutex);
        auto cit = cache.find(path);
        if (cit != cache.end())
            return readyFuture(cit->second);

        auto infl = m_inFlight.find(path);
        if (infl != m_inFlight.end())
            return infl->second;

        AsyncLoadJob job;
        job.path = path;
        std::shared_future<std::shared_ptr<IAsset>> sf = job.promise.get_future().share();
        m_inFlight[path] = sf;
        lock.unlock();

        {
            std::lock_guard<std::mutex> qk(m_queueMutex);
            m_asyncQueue.push(std::move(job));
        }
        m_queueCv.notify_one();
        ensureAsyncWorkerStarted();
        return sf;
    }

    template<typename T>
    std::future<AssetHandle<T>> loadAsync(const std::string& path)
    {
        return std::async(std::launch::async, [this, path]() {
            AssetHandle<T> handle;
            handle.asset = std::static_pointer_cast<T>(loadSharedAsync(path).get());
            return handle;
        });
    }
};
