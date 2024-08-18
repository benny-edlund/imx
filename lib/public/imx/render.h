#pragma once

#include "imx/api.h"
#include <blend2d.h>
#include <imgui.h>
#include <string_view>

namespace imx {

IMX_API bool initialize_renderer(std::string_view font_filename,
                                 ImVec4 clear_color = {0.F, 0.F, 0.F, 1.F},
                                 BLContextCreateInfo context_creation_info = {},
                                 BLImageData shared_image_data = {});

IMX_API bool begin_frame();

IMX_API bool
render_frame(ImDrawData const *draw_data,
             BLContextFlushFlags flags = BL_CONTEXT_FLUSH_NO_FLAGS);

IMX_API BLImage &add_texture();

} // namespace imx