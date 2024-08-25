#pragma once

#include "imx/api.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <blend2d.h>
#include <functional>
#include <imgui.h>
#include <limits>
#include <memory>

namespace imx {

struct IMX_API Image {
  IMX_API Image(Display *display, Visual *visual, int width, int height,
                int depth);
  IMX_API ~Image();
  IMX_API Image(Image const &) = delete;
  IMX_API Image &operator=(Image const &) = delete;
  IMX_API Image(Image &&) noexcept = delete;
  IMX_API Image &operator=(Image &&) noexcept = delete;

  [[nodiscard]] IMX_API int width() const;
  [[nodiscard]] IMX_API int height() const;
  [[nodiscard]] IMX_API int depth() const;
  [[nodiscard]] IMX_API int stride() const;
  [[nodiscard]] IMX_API void *data();
  [[nodiscard]] IMX_API void const *data() const;
  [[nodiscard]] IMX_API XImage *image() const;

private:
  XShmSegmentInfo info_{};
  Display *display_ = nullptr;
  Visual *visual_ = nullptr;
  std::unique_ptr<XImage, void (*)(XImage *)> image_{nullptr, [](auto *) {}};

  int width_ = 0;
  int height_ = 0;
  int depth_ = 0;
  int stride_ = 0;
};

using unique_graphics_context =
    std::unique_ptr<_XGC, std::function<void(_XGC *)>>;
using unique_image = std::unique_ptr<Image>;
using unique_input_context = std::unique_ptr<_XIC, std::function<void(_XIC *)>>;
using unique_display = std::unique_ptr<Display, std::function<void(Display *)>>;
using unique_input_method = std::unique_ptr<_XIM, std::function<void(_XIM *)>>;

struct IMX_API imx_window {
  Window window;
  unique_graphics_context gc;
  unique_image image;
  unique_input_context input_context;
  std::array<int, 2> size_updates{std::numeric_limits<int>::max(),
                                  std::numeric_limits<int>::max()};
};

struct IMX_API imx_context {

  unique_display display;
  int screen = 0;
  Visual *visual = nullptr;
  Colormap colormap = 0;
  unique_input_method input_method;
  std::vector<imx_window> windows;

  imx_context();
};

IMX_API ImGuiKey translate_key(XKeyEvent &event);
IMX_API BLImage &add_texture();
IMX_API bool begin_frame();
IMX_API bool end_frame(BLContextFlushFlags flags = BL_CONTEXT_FLUSH_NO_FLAGS);
IMX_API bool enqueue_expose();

} // namespace imx
