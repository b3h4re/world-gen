#pragma once

#include "game/input/input_state.hpp"

#include <vulkan/vulkan.h>

namespace lve {

class InputReader {
public:
    virtual ~InputReader() = default;

    virtual AppInputState readInputState(VkExtent2D extent) = 0;
};

} // namespace lve
