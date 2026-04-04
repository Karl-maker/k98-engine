# State machines and events in this project

This document describes how the `StateMachine` in `game-engine/src/statemachine` works, how **events** are modeled, and how to integrate both with the ECS (`Registry`, `StateMachineComponent`).

---

## 1. Mental model

- A **state machine** owns a **current state** (a string name), a **configuration** (`StateMachineConfig`) that lists all states and transitions, and an **owner entity id** (`Entity`) passed into callbacks.
- **Events** are not a separate publish/subscribe bus in this codebase. They are **typed labels** (`enum class StateEventType`) that you pass into `StateMachine::handleEvent(...)`. The machine looks at **only the current state’s** `eventTransitions` and may change state if a transition matches.
- **Per-frame behavior** is optional: each `StateDefinition` can supply `onEnter`, `onUpdate`, and `onExit` callbacks.

So: **“Subscribing” to an event** means **your game code decides when to call `handleEvent`**, usually from `onUpdate`, `onInput`, or another system after you detect a condition (input, AI, timers, collisions, etc.).

---

## 2. Files and types

| Piece | Location | Role |
|--------|------------|------|
| `StateEventType` | `game-engine/src/statemachine/StateMachineTypes.hpp` | Enum of event kinds the FSM knows about. |
| `EventTransition` | same | Links `(event → target state)` with optional **guard**. |
| `StateDefinition` | same | One state: name, `onEnter` / `onUpdate` / `onExit`, and `eventTransitions`. |
| `StateMachineConfig` | `StateMachineConfig.hpp` | `initialState` + `states` map (`std::unordered_map<std::string, StateDefinition>`). |
| `StateMachine` | `StateMachine.hpp` / `StateMachine.cpp` | Runtime engine: `initialize`, `update`, `handleEvent`, transitions. |
| `StateMachineComponent` | `game-engine/src/components/StateMachineComponent.hpp` | ECS wrapper holding a `StateMachine machine`. |

### 2.1 Callback type aliases

From `StateMachineTypes.hpp`:

- `StateGuard` → `std::function<bool(Entity)>` — if set, transition runs only when guard returns `true`.
- `StateEnterAction` → `void(Entity)`
- `StateUpdateAction` → `void(Entity, double dt)`
- `StateExitAction` → `void(Entity)`

### 2.2 Built-in `StateEventType` values

```cpp
enum class StateEventType
{
    UserDetected,
    MoveInput,
    StopInput
};
```

To add new events, extend this enum and recompile everything that uses it. Then reference the new enumerator in `EventTransition` entries.

---

## 3. Lifecycle

### 3.1 `initialize(Entity owner, const StateMachineConfig& config)`

- Stores `owner` and copies `config`.
- Sets current state to `config.initialState` (must exist in `config.states` or **throws** `std::runtime_error("Initial state not found")`).
- Calls **`onEnter`** for the initial state, if non-null.

### 3.2 `update(double dt)`

- Increments **time in current state** (`m_timeInState`).
- Calls **`onUpdate`** for the **current** state, if non-null: `onUpdate(owner, dt)`.

Call this once per entity per tick from your game loop (see `PositionDemo::onUpdate`).

### 3.3 `handleEvent(StateEventType event)`

Implementation (simplified behavior):

1. Look up the **current** `StateDefinition`.
2. Walk `eventTransitions` **in order**.
3. For each transition where `t.event == event`:
   - If `t.guard` is null **or** `t.guard(owner)` is `true`, then **`changeState(t.toState)`** and **return** (first match wins).

So **order of entries in `eventTransitions` matters** if multiple rows could match the same event (unusual, but possible with different guards).

---

## 4. Changing state: three APIs

| Method | Behavior |
|--------|----------|
| `handleEvent(StateEventType)` | Event-driven: uses **only** transitions listed under the **current** state. Respects **guards**. |
| `transitionTo(const std::string& state)` | Only succeeds if `canTransitionTo(state)` is true (see below). |
| `forceTransitionTo(const std::string& state)` | Jumps to `state` if it exists in `config.states`; **does not** require an event transition. Still runs exit/enter. |

### 4.1 What `canTransitionTo` actually checks

`canTransitionTo(target)` returns `true` if **any** `EventTransition` on the **current** state has `toState == target`. It does **not** check event type or guards. It is a loose “is there some declared edge toward this state?” helper.

### 4.2 `changeState` (private, but important)

On every successful transition:

1. `onExit(owner)` on the old state (if set).
2. `m_currentState = newState`, `m_timeInState = 0`.
3. `onEnter(owner)` on the new state (if set).

---

## 5. How this ties to “events” and “subscribe”

There is **no** central `EventBus::subscribe(...)` in this engine. The pattern is:

1. **Define** transitions in config: “when in `Idle`, event `MoveInput` goes to `Move`”.
2. **Dispatch** by calling `machine.handleEvent(StateEventType::MoveInput)` when your game decides that happened.

That call site is effectively your **subscription**: e.g. “when player velocity indicates movement, dispatch `MoveInput`”.

### 5.1 Example: player Idle ↔ Move (from `PositionDemo.cpp`)

Game logic derives **moving vs idle** from velocity and a short grace timer, then dispatches:

```cpp
if (moving && sm.getCurrentState() != "Move")
{
    sm.handleEvent(StateEventType::MoveInput);
}
else if (!moving && sm.getCurrentState() != "Idle")
{
    sm.handleEvent(StateEventType::StopInput);
}
```

State definitions (abbreviated):

```cpp
config.states["Idle"] = {
    "Idle", nullptr, nullptr, nullptr,
    { { StateEventType::MoveInput, "Move", nullptr } }
};

config.states["Move"] = {
    "Move", nullptr, nullptr, nullptr,
    { { StateEventType::StopInput, "Idle", nullptr } }
};
```

Here, **you** “subscribe” movement to the FSM by running that `if` block in `onUpdate`.

### 5.2 Example: enemy `UserDetected` → `Flee` with `onEnter` side effect

```cpp
config.states["Idle"] = {
    "Idle", nullptr, nullptr, nullptr,
    { { StateEventType::UserDetected, "Flee", nullptr } }
};

config.states["Flee"] = {
    "Flee",
    [&](Entity e) {
        m_registry.getComponent<Velocity>(e).x = -2;
    },
    nullptr,
    nullptr,
    {}
};
```

To actually transition, something in the game must call:

```cpp
sm.handleEvent(StateEventType::UserDetected);
```

(The sample demo may not call this for enemies yet; it illustrates how you would wire detection.)

---

## 6. Guards (conditional transitions)

`EventTransition` has an optional `StateGuard guard`.

```cpp
{ StateEventType::MoveInput, "Move", [](Entity e) {
    return /* e.g. enough stamina, not stunned, etc. */;
}}
```

If `guard` is non-null and returns `false`, that transition is skipped and the next matching `EventTransition` for the same event is considered. If none succeed, the state does not change.

---

## 7. ECS usage pattern

1. `registerComponent<StateMachineComponent>()` on the registry.
2. `addComponent<StateMachineComponent>(entity, {})` — default-constructs the embedded `StateMachine`.
3. `auto& sm = registry.getComponent<StateMachineComponent>(entity).machine;`
4. Build a `StateMachineConfig`, then `sm.initialize(entity, config)`.

The `Entity` passed to `initialize` is the same id you get from `createEntity()`; callbacks receive it so you can look up `Position`, `Velocity`, etc.

---

## 8. Full minimal example (new AI states)

```cpp
#include "statemachine/StateMachine.hpp"
#include "statemachine/StateMachineTypes.hpp"
#include "statemachine/StateMachineConfig.hpp"

void setupPatrolAi(Entity e, Registry& registry)
{
    StateMachineConfig config;
    config.initialState = "Patrol";

    config.states["Patrol"] = {
        "Patrol",
        nullptr,
        [](Entity ent, double dt) { /* wander */ },
        nullptr,
        {
            { StateEventType::UserDetected, "Chase", [](Entity) { return true; } },
        }
    };

    config.states["Chase"] = {
        "Chase",
        nullptr,
        [](Entity ent, double dt) { /* run toward player */ },
        nullptr,
        {
            { StateEventType::StopInput, "Patrol", nullptr }, // reuse or add a new enum
        }
    };

    registry.addComponent<StateMachineComponent>(e, {});
    registry.getComponent<StateMachineComponent>(e).machine.initialize(e, config);
}
```

When your visibility system spots the player:

```cpp
sm.handleEvent(StateEventType::UserDetected);
```

---

## 9. Optional patterns if you want real “subscribe” semantics

Not in the repo today, but common extensions:

1. **Queue**: push `StateEventType` (and maybe `Entity`) into a `std::vector` in `onInput`/`onUpdate`, then drain the queue and call `handleEvent` for each (ordering and batching control).
2. **Thin dispatcher**: a struct holding `StateMachine*` and a method `void post(StateEventType e) { machine->handleEvent(e); }` used by subsystems.
3. **Many event types**: keep the enum manageable or switch to `std::string` / `uint32_t` ids (would require changing `StateMachineTypes.hpp` and `EventTransition`).

---

## 10. Pitfalls

- **First matching transition wins** for a given `handleEvent` call; structure `eventTransitions` accordingly.
- **`transitionTo` / `canTransitionTo`** are tied to how transitions are **declared**, not to runtime guards only—read the implementation in `StateMachine.cpp` before relying on them for strict graph validation.
- **`forceTransitionTo`** skips the “is there an event edge?” check; use when loading levels, cutscenes, or cheats.
- **Adding event enum values** is a compile-time contract: all `switch` statements in your code should handle new cases or use `default` where appropriate.

---

## 11. Quick API reference

| API | Purpose |
|-----|---------|
| `initialize(owner, config)` | Reset machine to `initialState`, run first `onEnter`. |
| `update(dt)` | Advance time in state; run `onUpdate`. |
| `handleEvent(event)` | Try event transitions from current state (guards, first match). |
| `getCurrentState()` | `const std::string&` state name. |
| `getTimeInState()` | Seconds since last transition. |
| `canTransitionTo(state)` | Whether any outgoing **event** transition targets `state` (see §4.1). |
| `transitionTo(state)` | `changeState` only if `canTransitionTo`. |
| `forceTransitionTo(state)` | `changeState` if state exists in config. |

This matches the implementation in `game-engine/src/statemachine/StateMachine.cpp` as of this document.
