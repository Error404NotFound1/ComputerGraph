#pragma once

#include <iostream>
#include <string_view>

namespace cg
{
    enum class LogLevel
    {
        Info,
        Warn,
        Error
    };

    inline void log(LogLevel level, std::string_view message)
    {
        const char* prefix = "";
        switch (level)
        {
        case LogLevel::Info: prefix = "[Info] "; break;
        case LogLevel::Warn: prefix = "[Warn] "; break;
        case LogLevel::Error: prefix = "[Error] "; break;
        }

        std::ostream& stream = level == LogLevel::Error ? std::cerr : std::cout;
        stream << prefix << message << std::endl;
    }
} // namespace cg

