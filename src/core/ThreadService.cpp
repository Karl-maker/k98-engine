#include "ThreadService.hpp"

#include <algorithm>

ThreadService::~ThreadService()
{
    shutdown();
}

void ThreadService::configure(const ThreadServiceConfig& cfg)
{
    m_config = cfg;
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0)
        hw = 2;

    if (cfg.workerCount == 0) {
        // Main thread: input + ECS + OpenGL. Pool: streaming decode and optional parallel shards.
        const unsigned reserved = 2u;
        if (hw > reserved)
            m_workerCount = static_cast<std::uint32_t>(hw - reserved);
        else
            m_workerCount = 1u;
    } else {
        m_workerCount = cfg.workerCount;
    }
    m_workerCount = std::max<std::uint32_t>(1u, m_workerCount);
}

void ThreadService::start()
{
    if (m_running)
        return;

    m_stop = false;
    m_workers.clear();
    m_workers.reserve(m_workerCount);

    for (std::uint32_t i = 0; i < m_workerCount; ++i) {
        m_workers.emplace_back([this] { workerLoop(); });
    }
    m_running = true;
}

void ThreadService::shutdown()
{
    if (!m_running)
        return;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();

    for (auto& t : m_workers) {
        if (t.joinable())
            t.join();
    }
    m_workers.clear();

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        while (!m_jobs.empty())
            m_jobs.pop();
    }

    m_running = false;
    m_stop = false;
}

void ThreadService::workerLoop()
{
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this] { return m_stop || !m_jobs.empty(); });
            if (m_stop && m_jobs.empty())
                return;
            if (!m_jobs.empty()) {
                job = std::move(m_jobs.front());
                m_jobs.pop();
            }
        }
        if (job)
            job();
    }
}

void ThreadService::parallelRange(std::size_t totalItems, std::size_t segments,
    const std::function<void(std::size_t, std::size_t)>& fn)
{
    if (totalItems == 0 || !fn)
        return;

    if (!m_running || m_workerCount <= 1 || segments <= 1) {
        fn(0, totalItems);
        return;
    }

    segments = std::min(segments, totalItems);
    const std::size_t chunk = (totalItems + segments - 1) / segments;
    std::vector<std::future<void>> futs;
    futs.reserve(segments);

    for (std::size_t s = 0; s < segments; ++s) {
        const std::size_t start = s * chunk;
        if (start >= totalItems)
            break;
        const std::size_t end = std::min(start + chunk, totalItems);
        futs.push_back(submit([fn, start, end]() { fn(start, end); }));
    }
    for (auto& f : futs)
        f.wait();
}

void ThreadService::parallelEntityChunks(const std::vector<Entity>& entities,
    const std::function<void(const std::vector<Entity>&)>& fn)
{
    if (entities.empty() || !fn)
        return;

    const std::uint32_t thresh = m_config.entityParallelThreshold;
    const std::uint32_t minChunk = m_config.minEntitiesPerChunk;

    if (entities.size() < thresh || !m_running || m_workerCount <= 1) {
        fn(entities);
        return;
    }

    std::size_t numChunks = (entities.size() + static_cast<std::size_t>(minChunk) - 1) / static_cast<std::size_t>(minChunk);
    numChunks = std::min<std::size_t>(numChunks, static_cast<std::size_t>(m_workerCount));
    numChunks = std::max<std::size_t>(1, numChunks);

    const std::size_t per = (entities.size() + numChunks - 1) / numChunks;
    std::vector<std::future<void>> futs;
    futs.reserve(numChunks);

    for (std::size_t i = 0; i < numChunks; ++i) {
        const std::size_t a = i * per;
        if (a >= entities.size())
            break;
        const std::size_t b = std::min(a + per, entities.size());
        std::vector<Entity> chunk(entities.begin() + static_cast<std::ptrdiff_t>(a),
            entities.begin() + static_cast<std::ptrdiff_t>(b));
        futs.push_back(submit([fn, chunk = std::move(chunk)]() mutable { fn(chunk); }));
    }
    for (auto& f : futs)
        f.wait();
}
