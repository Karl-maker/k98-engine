#pragma once

// -----------------------------------------------------------------------------
// Components.hpp — convenience bundle for the minimal gameplay set used by early
// demos (movement + HP). Prefer including specific headers from `components/`
// in new code so dependencies stay explicit.
//
// Every type you addComponent<> must be registered once on Registry:
//   registry.registerComponent<Position>();
//   registry.registerComponent<Velocity>();
//   registry.registerComponent<Health>();
// -----------------------------------------------------------------------------

#include "../components/Position.hpp"
#include "../components/Velocity.hpp"
#include "../components/Health.hpp"
