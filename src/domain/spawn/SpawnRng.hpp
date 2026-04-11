#pragma once

#include <cstdint>

namespace spawn {

/// Deterministic RNG for spawn rolls and factory-side attribute sampling (SplitMix64).
class SpawnRng {
public:
    explicit SpawnRng(uint64_t seed) noexcept
        : m_state(seed ? seed : 0x9E3779B97F4A7C15ULL)
    {
    }

    uint32_t nextU32() noexcept
    {
        uint64_t z = (m_state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return static_cast<uint32_t>((z ^ (z >> 31)) >> 32);
    }

    /// Uniform [0, 1).
    float nextU01() noexcept
    {
        return (nextU32() >> 8) * (1.f / 16777216.f);
    }

private:
    uint64_t m_state;
};

inline uint64_t hashSpawnId(std::string_view s) noexcept
{
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace spawn
