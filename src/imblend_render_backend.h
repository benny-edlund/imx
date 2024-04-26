#pragma once

#include <blend2d.h>
#include <imgui.h>
#include <string_view>

#ifdef _WIN32
#define IMGUI_BLEND_BACKEND_EXPORT __declspec(dllexport)
#else
#define IMGUI_BLEND_BACKEND_EXPORT
#endif

IMGUI_BLEND_BACKEND_EXPORT bool
imblend_initialize(std::string_view font_filename,
                   BLContextCreateInfo context_creation_info = {},
                   BLImageData shared_image_data = {});

bool imblend_new_frame(ImVec4 clear_color = {0.F, 0.F, 0.F, 1.F},
                       BLImageData shared_image_data = {});

IMGUI_BLEND_BACKEND_EXPORT bool
imblend_render(ImDrawData const *draw_data,
               BLContextFlushFlags flags = BL_CONTEXT_FLUSH_NO_FLAGS);

IMGUI_BLEND_BACKEND_EXPORT BLImage &imblend_add_texture();
