#pragma once

#include "StateMachineConfig.hpp"

#include <string>

class StateMachine
{
public:
    StateMachine() = default;

    void initialize(Entity owner, const StateMachineConfig& config);

    void update(double dt);

    void handleEvent(StateEventType event);

    bool canTransitionTo(const std::string& state) const;
    bool transitionTo(const std::string& state);
    bool forceTransitionTo(const std::string& state);

    const std::string& getCurrentState() const;
    double getTimeInState() const;

private:
    void changeState(const std::string& newState);

private:
    Entity m_owner = 0;
    StateMachineConfig m_config;

    std::string m_currentState;
    double m_timeInState = 0.0;
};