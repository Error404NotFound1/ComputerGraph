#include "util/FileSystem.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cg
{
    std::string readTextFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::runtime_error("Failed to open file: " + path);
        }

        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }

    std::vector<char> readBinaryFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open file: " + path);
        }

        return std::vector<char>((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    }
} // namespace cg

