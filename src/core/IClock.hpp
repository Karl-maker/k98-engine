#pragma once

class IClock
{
public:
    virtual ~IClock() = default;

    virtual double nowSeconds() const = 0;
    virtual void sleepSeconds(double seconds) const = 0;
};