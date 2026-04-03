#pragma once
#include <cstdint>

using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = static_cast<Entity>(-1);

constexpr Entity MAX_ENTITIES = 5000;
constexpr std::size_t MAX_COMPONENTS = 32;