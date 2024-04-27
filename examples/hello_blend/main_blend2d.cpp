#include "imx/render.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/keysym.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <functional>
#include <imgui.h>
#include <memory>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <tracy/Tracy.hpp>

#include <cassert>

// Function to find a 32-bit ARGB visual
Visual *find_argb_visual(Display *display, int screen) {
  XVisualInfo visualInfoTemplate{};
  visualInfoTemplate.screen = screen;
  visualInfoTemplate.depth = 32;
  visualInfoTemplate.c_class = TrueColor;

  int visualsMatched = 0;
  XVisualInfo *visualInfo = XGetVisualInfo(
      display, VisualScreenMask | VisualDepthMask | VisualClassMask,
      &visualInfoTemplate, &visualsMatched);

  Visual *visual = nullptr;
  if (visualsMatched > 0) {
    visual = visualInfo->visual;
  }

  XFree(visualInfo);
  return visual;
}

// no idea why this works but we need to use this size for our XImage
template <typename T> constexpr T sanitize_width(T x) {
  auto tmp = x / 2;
  return (tmp + tmp % 2) * 2;
}

struct Image {
  Image(Display *display, Visual *visual, int width, int height, int depth)
      : display_(display), visual_(visual), width_(sanitize_width(width)),
        height_(height), depth_(depth), stride_(width_ * 4) {
    memset(&info_, 0, sizeof(XShmSegmentInfo));
    info_.shmid =
        shmget(IPC_PRIVATE, width_ * height_ * (depth_ / sizeof(std::uint8_t)),
               IPC_CREAT | 0600);
    info_.shmaddr = (char *)shmat(info_.shmid, nullptr, IPC_CREAT | 0600);
    // shmdt(info_.shmaddr);
    info_.readOnly = False;

    // Attach the segment to the display
    if (!XShmAttach(display_, &info_)) {
      fmt::print("Failed to attach shared memory\n");
      memset(&info_, 0, sizeof(XShmSegmentInfo));
    }
    image_ = std::unique_ptr<XImage, void (*)(XImage *)>(
        XShmCreateImage(display_, visual_, depth_, ZPixmap, info_.shmaddr,
                        &info_, width_, height_),
        [](XImage *owned) { XDestroyImage(owned); });
    XSync(display_, 0);

    // Create an XImage using the shared memory
  }
  ~Image() {
    if (info_.shmaddr != 0) {
      XShmDetach(display_, &info_);
      //   XFreePixmap(display_, pixmap_);
      shmctl(info_.shmid, IPC_RMID, nullptr);
      XSync(display_, 0);
    }
  }
  int width() const { return width_; }
  int height() const { return height_; }
  int depth() const { return depth_; }
  int stride() const { return stride_; }
  void *data() { return (void *)info_.shmaddr; }
  void const *data() const { return (void *)info_.shmaddr; }
  XImage *image() const { return image_.get(); }

private:
  XShmSegmentInfo info_;
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

struct imx_window {
  Window window;
  unique_graphics_context gc;
  unique_image image;
  unique_input_context input_context;
};

struct imx_context {

  unique_display display;
  int screen = 0;
  Visual *visual = nullptr;
  Colormap colormap = 0;
  unique_input_method input_method;
  std::vector<imx_window> windows;

  imx_context()
      : display(XOpenDisplay(nullptr),
                [](Display *owned) { XCloseDisplay(owned); }),
        screen(DefaultScreen(display.get())),
        visual(find_argb_visual(display.get(), screen)),
        colormap(XCreateColormap(display.get(),
                                 RootWindow(display.get(), screen), visual,
                                 AllocNone)),
        input_method([](Display *display) {
          XSetLocaleModifiers("");
          return unique_input_method(XOpenIM(display, NULL, NULL, NULL),
                                     [](XIM owned) { XCloseIM(owned); });
        }(display.get())) {
    if (!visual) {
      fmt::print("No 32-bit ARGB visual found\n");
      std::terminate();
    }
    if (!input_method) {
      XSetLocaleModifiers("@im=none");
      input_method =
          unique_input_method(XOpenIM(display.get(), NULL, NULL, NULL),
                              [](XIM owned) { XCloseIM(owned); });
    }
  }
};

bool imx_platform_initialize() {
  static std::unique_ptr<imx_context> s_context;
  if (s_context) {
    fmt::print("ImX context already initialized\n");
    return false;
  }
  fmt::print("ImX initialized\n");
  s_context = std::make_unique<imx_context>();
  ImGui::GetIO().BackendPlatformUserData = s_context.get();
  return true;
}

bool imx_platform_create_window(std::uint32_t width, std::uint32_t height,
                                std::uint32_t depth = 32) {
  if (auto *context = reinterpret_cast<imx_context *>(
          ImGui::GetIO().BackendPlatformUserData)) {
    XSetWindowAttributes attrs;
    attrs.colormap = context->colormap;
    attrs.border_pixel = 0;
    attrs.background_pixel = 0xFF000000; // Transparent background 0x00000000

    Window window =
        XCreateWindow(context->display.get(),
                      RootWindow(context->display.get(), context->screen), 0, 0,
                      width, height, 0, depth, InputOutput, context->visual,
                      CWColormap | CWBorderPixel | CWBackPixel, &attrs);
    auto xic = XCreateIC(context->input_method.get(), XNInputStyle,
                         XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                         window, XNFocusWindow, window, NULL);

    XMapWindow(context->display.get(), window);
    XSelectInput(context->display.get(), window,
                 ExposureMask | PointerMotionMask | ButtonPressMask |
                     ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
                     FocusChangeMask | StructureNotifyMask);

    context->windows.push_back(imx_window{
        window,
        std::unique_ptr<_XGC, std::function<void(_XGC *)>>(
            XCreateGC(context->display.get(), window, 0, nullptr),
            [display_ = context->display.get()](_XGC *owned) {
              XFreeGC(display_, owned);
            }),
        std::make_unique<Image>(context->display.get(), context->visual, width,
                                height, depth),
        unique_input_context(xic, [](_XIC *owned) { XDestroyIC(owned); })});

    return true;
  }
  return false;
}

// Function to translate X11 key codes to ImGui key codes
ImGuiKey imx_platform_translate_key(XKeyEvent &event) {
  KeySym keysym = XLookupKeysym(&event, 0);
  if (event.state & ShiftMask) {
    keysym = XLookupKeysym(&event, 1);
  }

  switch (keysym) {
  case XK_BackSpace:
    return ImGuiKey_Backspace;
  case XK_Tab:
    return ImGuiKey_Tab;
  case XK_Linefeed:
    return ImGuiKey_Enter;
  case XK_Clear:
    return ImGuiKey_None;
  case XK_Return:
    return ImGuiKey_Enter;
  case XK_Pause:
    return ImGuiKey_Pause;
  case XK_Scroll_Lock:
    return ImGuiKey_ScrollLock;
  case XK_Sys_Req:
    return ImGuiKey_None;
  case XK_Escape:
    return ImGuiKey_Escape;
  case XK_Delete:
    return ImGuiKey_Delete;

  case XK_Home:
    return ImGuiKey_Home;
  case XK_Left:
    return ImGuiKey_LeftArrow;
  case XK_Up:
    return ImGuiKey_UpArrow;
  case XK_Right:
    return ImGuiKey_RightArrow;
  case XK_Down:
    return ImGuiKey_DownArrow;
  case XK_Page_Up:
    return ImGuiKey_PageUp;
  case XK_Page_Down:
    return ImGuiKey_PageDown;
  case XK_End:
    return ImGuiKey_End;
  case XK_Begin:
    return ImGuiKey_Home;

  case XK_Select:
    return ImGuiKey_None;
  case XK_Print:
    return ImGuiKey_PrintScreen;
  case XK_Execute:
    return ImGuiKey_None;
  case XK_Insert:
    return ImGuiKey_Insert;
  case XK_Undo:
    return ImGuiKey_None;
  case XK_Redo:
    return ImGuiKey_None;
  case XK_Menu:
    return ImGuiKey_Menu;
  case XK_Find:
    return ImGuiKey_None;
  case XK_Cancel:
    return ImGuiKey_None;
  case XK_Help:
    return ImGuiKey_None;
  case XK_Break:
    return ImGuiKey_None;
  case XK_Mode_switch:
    return ImGuiKey_None;
  case XK_Num_Lock:
    return ImGuiKey_NumLock;

  case XK_KP_Space:
    return ImGuiKey_Space;
  case XK_KP_Tab:
    return ImGuiKey_Tab;
  case XK_KP_Enter:
    return ImGuiKey_KeypadEnter;
  case XK_KP_F1:
    return ImGuiKey_F1;
  case XK_KP_F2:
    return ImGuiKey_F2;
  case XK_KP_F3:
    return ImGuiKey_F3;
  case XK_KP_F4:
    return ImGuiKey_F4;
  case XK_KP_Home:
    return ImGuiKey_Home;
  case XK_KP_Left:
    return ImGuiKey_LeftArrow;
  case XK_KP_Up:
    return ImGuiKey_UpArrow;
  case XK_KP_Right:
    return ImGuiKey_RightArrow;
  case XK_KP_Down:
    return ImGuiKey_DownArrow;
  case XK_KP_Page_Up:
    return ImGuiKey_PageUp;
  case XK_KP_Page_Down:
    return ImGuiKey_PageDown;
  case XK_KP_End:
    return ImGuiKey_End;
  case XK_KP_Begin:
    return ImGuiKey_Home;
  case XK_KP_Insert:
    return ImGuiKey_Insert;
  case XK_KP_Delete:
    return ImGuiKey_Delete;
  case XK_KP_Equal:
    return ImGuiKey_KeypadEqual;
  case XK_KP_Multiply:
    return ImGuiKey_KeypadMultiply;
  case XK_KP_Add:
    return ImGuiKey_KeypadAdd;
  case XK_KP_Separator:
    return ImGuiKey_Space;
  case XK_KP_Subtract:
    return ImGuiKey_KeypadSubtract;
  case XK_KP_Decimal:
    return ImGuiKey_KeypadDecimal;
  case XK_KP_Divide:
    return ImGuiKey_KeypadDivide;

  case XK_KP_0:
    return ImGuiKey_Keypad0;
  case XK_KP_1:
    return ImGuiKey_Keypad1;
  case XK_KP_2:
    return ImGuiKey_Keypad2;
  case XK_KP_3:
    return ImGuiKey_Keypad3;
  case XK_KP_4:
    return ImGuiKey_Keypad4;
  case XK_KP_5:
    return ImGuiKey_Keypad5;
  case XK_KP_6:
    return ImGuiKey_Keypad6;
  case XK_KP_7:
    return ImGuiKey_Keypad7;
  case XK_KP_8:
    return ImGuiKey_Keypad8;
  case XK_KP_9:
    return ImGuiKey_Keypad9;

  case XK_F1:
    return ImGuiKey_F1;
  case XK_F2:
    return ImGuiKey_F2;
  case XK_F3:
    return ImGuiKey_F3;
  case XK_F4:
    return ImGuiKey_F4;
  case XK_F5:
    return ImGuiKey_F5;
  case XK_F6:
    return ImGuiKey_F6;
  case XK_F7:
    return ImGuiKey_F7;
  case XK_F8:
    return ImGuiKey_F8;
  case XK_F9:
    return ImGuiKey_F9;
  case XK_F10:
    return ImGuiKey_F10;
  case XK_F11:
    return ImGuiKey_F11;
  case XK_F12:
    return ImGuiKey_F12;
  case XK_F13:
    return ImGuiKey_F13;
  case XK_F14:
    return ImGuiKey_F14;
  case XK_F15:
    return ImGuiKey_F15;
  case XK_F16:
    return ImGuiKey_F16;
  case XK_F17:
    return ImGuiKey_F17;
  case XK_F18:
    return ImGuiKey_F18;
  case XK_F19:
    return ImGuiKey_F19;
  case XK_F20:
    return ImGuiKey_F20;
  case XK_F21:
    return ImGuiKey_F21;
  case XK_F22:
    return ImGuiKey_F22;
  case XK_F23:
    return ImGuiKey_F23;
  case XK_F24:
    return ImGuiKey_F24;

  case XK_Shift_L:
    return ImGuiKey_LeftShift;
  case XK_Shift_R:
    return ImGuiKey_RightShift;
  case XK_Control_L:
    return ImGuiKey_LeftCtrl;
  case XK_Control_R:
    return ImGuiKey_RightCtrl;
  case XK_Caps_Lock:
    return ImGuiKey_CapsLock;

  // case XK_Meta_L: return ImGuiKey_
  // case XK_Meta_R: return ImGuiKey_
  case XK_Alt_L:
    return ImGuiKey_LeftAlt;
  case XK_Alt_R:
    return ImGuiKey_RightAlt;
  case XK_Super_L:
    return ImGuiKey_LeftSuper;
  case XK_Super_R:
    return ImGuiKey_RightSuper;
    // case XK_Hyper_L: return ImGuiKey_
    // case XK_Hyper_R: return ImGuiKey_

  case XK_space:
    return ImGuiKey_Space;
  // case XK_exclam: return ImGuiKey_;
  // case XK_quotedbl: return ImGuiKey_;
  // case XK_numbersign: return ImGuiKey_;
  // case XK_dollar: return ImGuiKey_;
  // case XK_percent: return ImGuiKey_;
  case XK_ampersand:
    return ImGuiKey_Apostrophe;
  // case XK_apostrophe: return ImGuiKey_;
  // case XK_quoteright: return ImGuiKey_;
  // case XK_parenleft: return ImGuiKey_;
  // case XK_parenright: return ImGuiKey_;
  // case XK_asterisk: return ImGuiKey_;
  // case XK_plus: return ImGuiKey_;
  case XK_comma:
    return ImGuiKey_Comma;
  case XK_minus:
    return ImGuiKey_Minus;
  case XK_period:
    return ImGuiKey_Period;
  case XK_slash:
    return ImGuiKey_Slash;
  case XK_0:
    return ImGuiKey_0;
  case XK_1:
    return ImGuiKey_1;
  case XK_2:
    return ImGuiKey_2;
  case XK_3:
    return ImGuiKey_3;
  case XK_4:
    return ImGuiKey_4;
  case XK_5:
    return ImGuiKey_5;
  case XK_6:
    return ImGuiKey_6;
  case XK_7:
    return ImGuiKey_7;
  case XK_8:
    return ImGuiKey_8;
  case XK_9:
    return ImGuiKey_9;
  // case XK_colon: return ImGuiKey_;
  case XK_semicolon:
    return ImGuiKey_Semicolon;
  // case XK_less: return ImGuiKey_;
  case XK_equal:
    return ImGuiKey_Equal;
  // case XK_greater: return ImGuiKey_;
  // case XK_question: return ImGuiKey_;
  // case XK_at: return ImGuiKey_;
  case XK_A:
  case XK_a:
    return ImGuiKey_A;
  case XK_B:
  case XK_b:
    return ImGuiKey_B;
  case XK_C:
  case XK_c:
    return ImGuiKey_C;
  case XK_D:
  case XK_d:
    return ImGuiKey_D;
  case XK_E:
  case XK_e:
    return ImGuiKey_E;
  case XK_F:
  case XK_f:
    return ImGuiKey_F;
  case XK_G:
  case XK_g:
    return ImGuiKey_G;
  case XK_H:
  case XK_h:
    return ImGuiKey_H;
  case XK_I:
  case XK_i:
    return ImGuiKey_I;
  case XK_J:
  case XK_j:
    return ImGuiKey_J;
  case XK_K:
  case XK_k:
    return ImGuiKey_K;
  case XK_L:
  case XK_l:
    return ImGuiKey_L;
  case XK_M:
  case XK_m:
    return ImGuiKey_M;
  case XK_N:
  case XK_n:
    return ImGuiKey_N;
  case XK_O:
  case XK_o:
    return ImGuiKey_O;
  case XK_P:
  case XK_p:
    return ImGuiKey_P;
  case XK_Q:
  case XK_q:
    return ImGuiKey_Q;
  case XK_R:
  case XK_r:
    return ImGuiKey_R;
  case XK_S:
  case XK_s:
    return ImGuiKey_S;
  case XK_T:
  case XK_t:
    return ImGuiKey_T;
  case XK_U:
  case XK_u:
    return ImGuiKey_U;
  case XK_V:
  case XK_v:
    return ImGuiKey_V;
  case XK_W:
  case XK_w:
    return ImGuiKey_W;
  case XK_X:
  case XK_x:
    return ImGuiKey_X;
  case XK_Y:
  case XK_y:
    return ImGuiKey_Y;
  case XK_Z:
  case XK_z:
    return ImGuiKey_Z;
  case XK_bracketleft:
    return ImGuiKey_LeftBracket;
  case XK_backslash:
    return ImGuiKey_Backslash;
  case XK_bracketright:
    return ImGuiKey_RightBracket;
  // case XK_asciicircum: return ImGuiKey_;
  // case XK_underscore: return ImGuiKey_;
  // case XK_grave: return ImGuiKey_;
  // case XK_quoteleft: return ImGuiKey_;
  // case XK_braceleft: return ImGuiKey_;
  // case XK_bar: return ImGuiKey_;
  // case XK_braceright: return ImGuiKey_;
  case XK_asciitilde:
    return ImGuiKey_GraveAccent;

  default:
    return ImGuiKey_None;
  }
}

void imx_platform_poll_events() {
  if (auto *context = reinterpret_cast<imx_context *>(
          ImGui::GetIO().BackendPlatformUserData)) {
    while (XPending(context->display.get()) > 0) {
      XEvent event;
      XNextEvent(context->display.get(), &event);
      switch (event.type) {
      case FocusIn: {
        auto &io = ImGui::GetIO();
        io.AddFocusEvent(true);
        break;
      }
      case FocusOut: {
        auto &io = ImGui::GetIO();
        io.AddFocusEvent(false);
        break;
      }

      case MotionNotify: {
        XMotionEvent motion_event = event.xmotion;
        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent((float)motion_event.x, (float)motion_event.y);
        break;
      }

      case ButtonPress: {
        XButtonPressedEvent press_event = event.xbutton;
        ImGuiIO &io = ImGui::GetIO();
        switch (press_event.button) {
        case Button1: {
          io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
          break;
        }
        case Button2: {
          io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
          break;
        }
        case Button3: {
          io.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
          break;
        }
        case Button4: {
          io.AddMouseWheelEvent(1, 1);
          break;
        }
        case Button5: {

          io.AddMouseWheelEvent(-1, -1);
          break;
        }
        default:
          break;
        }
        break;
      }

      case ButtonRelease: {
        XButtonPressedEvent press_event = event.xbutton;
        ImGuiIO &io = ImGui::GetIO();
        switch (press_event.button) {
        case Button1: {
          io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
          break;
        }
        case Button2: {
          io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
          break;
        }
        case Button3: {
          io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
          break;
        }
        default:
          break;
        }
        break;
      }

      case KeyPress: {
        XKeyEvent press_event = event.xkey;
        ImGuiIO &io = ImGui::GetIO();
        auto found =
            std::find_if(context->windows.begin(), context->windows.end(),
                         [&](auto const &handle) {
                           return handle.window == press_event.window;
                         });
        if (found != context->windows.end()) {
          imx_window &window = *found;
          static std::array<char, 256> buffer;
          KeySym key = 0;
          Status status;
          std::size_t count = Xutf8LookupString(
              window.input_context.get(), &event.xkey, buffer.data(),
              buffer.size() - 1, &key, &status);
          count = std::min(count, buffer.size() - 1);
          buffer.at(count) = '\0';
          if (count) {
            io.AddInputCharactersUTF8(buffer.data());
          }
          io.AddKeyEvent(imx_platform_translate_key(press_event), true);
        }
        break;
      }
      case KeyRelease: {
        fmt::print("key release\n");
        XKeyEvent press_event = event.xkey;
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(imx_platform_translate_key(press_event), false);
        break;
      }

      case ConfigureNotify: {
        XConfigureEvent configure_event = event.xconfigure;
        auto found =
            std::find_if(context->windows.begin(), context->windows.end(),
                         [&](auto const &handle) {
                           return handle.window == configure_event.window;
                         });
        if (found != context->windows.end()) {
          imx_window &window = *found;
          if (configure_event.width != window.image->width() ||
              configure_event.height != window.image->height()) {
            auto width = configure_event.width;
            auto height = configure_event.height;
            window.image.reset(new Image(context->display.get(),
                                         context->visual, width, height,
                                         window.image->depth()));
            ImGui::GetIO().DisplaySize = ImVec2(width, height);
          }
        }
        break;
      }
      case Expose: {
        XExposeEvent expose_event = event.xexpose;
        auto found =
            std::find_if(context->windows.begin(), context->windows.end(),
                         [&](auto const &handle) {
                           return handle.window == expose_event.window;
                         });
        if (found != context->windows.end()) {
          ZoneScopedN("X11 (render)");
          imx_window &window = *found;
          auto image_data = window.image->image();
          XShmPutImage(context->display.get(), window.window, window.gc.get(),
                       image_data, 0, 0, 0, 0, image_data->width,
                       image_data->height, 0);
        }
        break;
      }
      default:
        break;
      }
    }
  }
}

bool imx_platform_expose_again() {
  if (auto *context = reinterpret_cast<imx_context *>(
          ImGui::GetIO().BackendPlatformUserData)) {
    for (auto const &handle : context->windows) {
      XExposeEvent exposeEvent = {Expose,
                                  0,
                                  True,
                                  context->display.get(),
                                  handle.window,
                                  0,
                                  0,
                                  handle.image->width(),
                                  handle.image->height(),
                                  0};
      XSendEvent(context->display.get(), handle.window, False, ExposureMask,
                 (XEvent *)&exposeEvent);
    }
    return true;
  }
  return false;
}

int main() {
  IMGUI_CHECKVERSION();
  int width = 800;
  int height = 600;

  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2(width, height);

  imx_platform_initialize();
  imx_platform_create_window(width, height);
  imblend_initialize("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf");
  fmt::print("initialized\n");
  bool show_demo_window = false;
  bool show_another_window = false;
  ImVec4 clear_color{0.F, 0.F, 0.F, 1.F};
  BLImage &icon = imblend_add_texture();
  if (icon.readFromFile("/home/benny/projects/imgui_blend_backend/icon.png") !=
      BL_SUCCESS) {
    fmt::print("Failed to load icon\n");
    exit(-1);
  }

  using target_rate = std::chrono::duration<double, std::ratio<1, 60>>;
  auto stamp = std::chrono::steady_clock::now();
  auto deadline = stamp + target_rate(1);
  while (true) {
    imx_platform_poll_events();
    if (deadline > std::chrono::steady_clock::now()) {
      continue;
    }
    if (auto *context = reinterpret_cast<imx_context *>(
            ImGui::GetIO().BackendPlatformUserData)) {
      auto &image = *context->windows.front().image;
      if (!imblend_new_frame(clear_color,
                             BLImageData{image.data(), image.stride(),
                                         BLSizeI(image.width(), image.height()),
                                         BL_FORMAT_PRGB32})) {
        continue;
      }
      deadline += target_rate(1);

      static double fps = 0.F;
      static double ren = 0.F;
      using rate = std::chrono::duration<double>;
      stamp = std::chrono::steady_clock::now();
      static auto previous = std::chrono::steady_clock::now();
      io.DeltaTime = std::chrono::duration_cast<rate>(
                         std::chrono::steady_clock::now() - previous)
                         .count();
      fps = 1. / io.DeltaTime;
      previous = stamp;

      ImGui::NewFrame();
      if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
      }
      // 2. Show a simple window that we create ourselves. We use a
      // Begin/End pair to create a named window.
      {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat(
            "float", &f, 0.0f,
            1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3(
            "clear color",
            (float *)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button")) {

          counter++;
        }
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS) render(%.1f)",
                    1000.0f / io.Framerate, io.Framerate, fps);
        ImGui::Image(reinterpret_cast<ImTextureID>(&icon),
                     ImVec2(icon.size().w / 2.F, icon.size().h / 2.F));
        ImGui::End();
      }

      // 3. Show another simple window.
      if (show_another_window) {
        ImGui::Begin("Another Window",
                     &show_another_window); // Pass a pointer to our bool
                                            // variable (the window will have
                                            // a closing button that will
                                            // clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
          show_another_window = false;
        ImGui::End();
      }
      ImGui::Render();
      if (!imblend_render(ImGui::GetDrawData(), BL_CONTEXT_FLUSH_SYNC)) {
        fmt::print("Imblend render failed\n");
      }

      FrameMark;
      imx_platform_expose_again();
    }
  }
  ImGui::DestroyContext();
  return 0;
}
