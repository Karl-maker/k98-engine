#pragma once

class IGame
{
public:
    virtual ~IGame() = default;

    virtual void onStart() = 0;
    virtual void onInput() = 0;
    virtual void onUpdate(double fixedDeltaTime) = 0;
    virtual void onRender(double alpha) = 0;
    virtual void onStop() = 0;

    virtual bool shouldClose() const = 0;
};