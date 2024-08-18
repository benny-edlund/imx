#pragma once

#include "imx/api.h"
#include <blend2d.h>
#include <cstdint>
#include <imgui.h>
#include <string_view>

namespace imx {

static const std::uint32_t IMX_24BIT_DEPTH = 32;
static const std::uint32_t IMX_32BIT_DEPTH = 32;
static const ImVec4 IMX_NO_COLOR = {-1, -1, -1, -1};

IMX_API bool initialize(std::string_view font_filename,
                        ImVec4 clear_color = {0.F, 0.F, 0.F, 1.F},
                        BLContextCreateInfo context_creation_info = {},
                        BLImageData shared_image_data = {});
IMX_API bool create_window(std::uint32_t width, std::uint32_t height,
                           std::uint32_t depth = IMX_32BIT_DEPTH);
IMX_API void poll_events();
IMX_API bool begin_frame();
IMX_API bool
render_frame(ImDrawData const *draw_data, ImVec4 clear_color = IMX_NO_COLOR,
             BLContextFlushFlags flags = BL_CONTEXT_FLUSH_NO_FLAGS);

} // namespace imx