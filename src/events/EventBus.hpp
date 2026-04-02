#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>

class EventBus
{
public:
    template<typename Event>
    using Handler = std::function<void(const Event&)>;

    template<typename Event>
    void subscribe(Handler<Event> handler)
    {
        auto& handlers = m_handlers[typeid(Event)];
        handlers.push_back(
            [handler](const void* e)
            {
                handler(*static_cast<const Event*>(e));
            });
    }

    template<typename Event>
    void emit(const Event& event)
    {
        auto it = m_handlers.find(typeid(Event));
        if (it == m_handlers.end()) return;

        for (auto& handler : it->second)
        {
            handler(&event);
        }
    }

private:
    std::unordered_map<std::type_index,
        std::vector<std::function<void(const void*)>>> m_handlers;
};