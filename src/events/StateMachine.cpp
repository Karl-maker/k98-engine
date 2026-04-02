#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

struct StateTransition
{
    std::string toState;

    // Guard condition
    std::function<bool()> guard;
};

struct StateDefinition
{
    std::string name;

    std::function<void()> onEnter;
    std::function<void(double)> onUpdate;
    std::function<void()> onExit;

    std::vector<StateTransition> transitions;
};

class StateMachine
{
public:
    void setInitialState(const std::string& state);
    void addState(const StateDefinition& state);

    void update(double dt);

    const std::string& getState() const;
    double getTimeInState() const;

    // Explicit transition
    bool canTransition(const std::string& to) const;
    bool forceTransition(const std::string& to);

    // Event hook
    std::function<void(const std::string&, const std::string&)> onStateChanged;

private:
    void changeState(const std::string& newState);

private:
    std::unordered_map<std::string, StateDefinition> m_states;

    std::string m_currentState;
    double m_timeInState = 0.0;
};