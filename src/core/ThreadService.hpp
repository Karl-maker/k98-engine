#pragma once

#include "ecs/Entity.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

// =============================================================================
// ThreadService — worker pool for background jobs (e.g. asset decode). The game
// loop (input → update → render) stays on the MAIN thread: OpenGL/GLFW require
// rendering on the main thread on macOS; ECS Registry is not thread-safe for
// concurrent writes.
//
// Usage:
//   ThreadService ts;
//   ts.configure({});   // optional: set worker count
//   ts.start();
//   auto fut = ts.submit([]{ return loadModel(); });
//   // ... poll fut on main thread ...
//   ts.shutdown();     // in game onStop
//
// parallelRange / parallelForEntityIndices: split work across workers; **join
// completes before returning** — safe to touch Registry again after the call
// only if each chunk’s work is isolated (documented in method comments).
// =============================================================================

struct ThreadServiceConfig {
    /// Dedicated worker threads (default: max(1, hardware_concurrency - 2) to
    /// leave headroom for main thread + OS). Set 0 to use default.
    std::uint32_t workerCount = 0;

    /// When entity count exceeds this, parallelRange can shard work (optional).
    std::uint32_t entityParallelThreshold = 256;

    /// Minimum entities per parallel chunk (avoid tiny tasks).
    std::uint32_t minEntitiesPerChunk = 64;
};

class ThreadService {
public:
    ThreadService() = default;
    ~ThreadService();

    ThreadService(const ThreadService&) = delete;
    ThreadService& operator=(const ThreadService&) = delete;

    void configure(const ThreadServiceConfig& cfg);
    const ThreadServiceConfig& config() const { return m_config; }

    /// Spawn worker threads. Idempotent if already started.
    void start();

    /// Stop workers, drain or abandon pending jobs (waits for running jobs).
    void shutdown();

    bool isRunning() const { return m_running; }

    std::uint32_t workerCount() const { return m_workerCount; }

    /// Submit work to the pool. Returns a future; main thread should wait/poll.
    template<typename F>
    auto submit(F&& fn) -> std::future<std::invoke_result_t<std::decay_t<F>>> {
        using R = std::invoke_result_t<std::decay_t<F>>;
        std::packaged_task<R()> task(std::forward<F>(fn));
        std::future<R> fut = task.get_future();
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_jobs.push([t = std::make_shared<std::packaged_task<R()>>(std::move(task))]() { (*t)(); });
        }
        m_cv.notify_one();
        return fut;
    }

    /// Split [0, totalItems) into up to `segments` chunks; run `fn(start, end)` per
    /// chunk on the pool and **block until all finish**. Do not mutate Registry
    /// from chunks unless chunks touch disjoint data or you use external sync.
    void parallelRange(std::size_t totalItems, std::size_t segments, const std::function<void(std::size_t, std::size_t)>& fn);

    /// Split `entities` into contiguous chunks; run `fn(chunk)` per chunk on pool.
    /// Registry is not concurrent-write-safe — only use for disjoint data or read-only work.
    void parallelEntityChunks(const std::vector<Entity>& entities, const std::function<void(const std::vector<Entity>&)>& fn);

private:
    void workerLoop();

    ThreadServiceConfig m_config{};
    std::uint32_t m_workerCount = 1;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_jobs;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};
