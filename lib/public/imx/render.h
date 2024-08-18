#pragma once

#include "imx/api.h"
#include <blend2d.h>
#include <imgui.h>
#include <string_view>

IMX_API bool imblend_initialize(std::string_view font_filename,
                                BLContextCreateInfo context_creation_info = {},
                                BLImageData shared_image_data = {});

IMX_API bool imblend_new_frame(ImVec4 clear_color = {0.F, 0.F, 0.F, 1.F},
                               BLImageData shared_image_data = {});

IMX_API bool
imblend_render(ImDrawData const *draw_data,
               BLContextFlushFlags flags = BL_CONTEXT_FLUSH_NO_FLAGS);

IMX_API BLImage &imblend_add_texture();
