#include "StateMachine.hpp"

#include <stdexcept>

void StateMachine::initialize(Entity owner, const StateMachineConfig& config)
{
    m_owner = owner;
    m_config = config;
    m_currentState = config.initialState;
    m_timeInState = 0.0;

    if (m_config.states.find(m_currentState) == m_config.states.end())
    {
        throw std::runtime_error("Initial state not found");
    }

    auto& state = m_config.states[m_currentState];
    if (state.onEnter)
        state.onEnter(m_owner);
}

void StateMachine::update(double dt)
{
    m_timeInState += dt;

    auto& state = m_config.states[m_currentState];

    if (state.onUpdate)
        state.onUpdate(m_owner, dt);
}

void StateMachine::handleEvent(StateEventType event)
{
    auto& state = m_config.states[m_currentState];

    for (const auto& t : state.eventTransitions)
    {
        if (t.event == event)
        {
            if (!t.guard || t.guard(m_owner))
            {
                changeState(t.toState);
                return;
            }
        }
    }
}

bool StateMachine::canTransitionTo(const std::string& state) const
{
    const auto& current = m_config.states.at(m_currentState);

    for (const auto& t : current.eventTransitions)
    {
        if (t.toState == state)
            return true;
    }

    return false;
}

bool StateMachine::transitionTo(const std::string& state)
{
    if (!canTransitionTo(state))
        return false;

    changeState(state);
    return true;
}

bool StateMachine::forceTransitionTo(const std::string& state)
{
    if (m_config.states.find(state) == m_config.states.end())
        return false;

    changeState(state);
    return true;
}

void StateMachine::changeState(const std::string& newState)
{
    auto& current = m_config.states[m_currentState];

    if (current.onExit)
        current.onExit(m_owner);

    m_currentState = newState;
    m_timeInState = 0.0;

    auto& next = m_config.states[newState];

    if (next.onEnter)
        next.onEnter(m_owner);
}

const std::string& StateMachine::getCurrentState() const
{
    return m_currentState;
}

double StateMachine::getTimeInState() const
{
    return m_timeInState;
}