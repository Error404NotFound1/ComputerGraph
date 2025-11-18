#include "core/Timer.h"

namespace cg
{
    Timer::Timer()
        : m_last(std::chrono::steady_clock::now())
    {
    }

    double Timer::tick()
    {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> delta = now - m_last;
        m_last = now;
        return delta.count();
    }

    void Timer::reset()
    {
        m_last = std::chrono::steady_clock::now();
    }
} // namespace cg

