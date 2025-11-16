#pragma once

#include <glm/glm.hpp>

namespace cg
{
    struct InputState
    {
        bool forward{false};
        bool backward{false};
        bool left{false};
        bool right{false};
        bool up{false};
        bool down{false};
        bool freeLook{false};
        bool boost{false};
        bool cursorInitialized{false};
        glm::vec2 lastCursor{};
        glm::vec2 cursorDelta{};
        float scrollDelta{0.0f};
    };
} // namespace cg

