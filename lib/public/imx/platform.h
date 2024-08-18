#pragma once

#include "imx/api.h"
#include <cstdint>

namespace imx {

static const std::uint32_t IMX_24BIT_DEPTH = 32;
static const std::uint32_t IMX_32BIT_DEPTH = 32;

IMX_API bool initialize_platform();
IMX_API bool create_window(std::uint32_t width, std::uint32_t height,
                           std::uint32_t depth = IMX_32BIT_DEPTH);
IMX_API void poll_events();

} // namespace imx