#pragma once

#include <string>
#include <vector>

namespace cg
{
    std::string readTextFile(const std::string& path);
    std::vector<char> readBinaryFile(const std::string& path);
} // namespace cg

