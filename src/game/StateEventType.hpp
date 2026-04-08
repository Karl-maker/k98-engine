#pragma once

enum class StateEventType
{
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Stop,
    UserDetected,

    /// Locomotion tutorial / third-person run states
    SprintPress,
    SprintRelease,
    Jump,
    Landed,
};
