#pragma once

#include <chrono>

namespace cg
{
    class Timer
    {
    public:
        Timer();
        double tick();

    private:
        std::chrono::steady_clock::time_point m_last;
    };
} // namespace cg

