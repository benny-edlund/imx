#include "blend2d.h"
#include "imx/context.hpp"
#include "imx/imx.hpp"
#include <X11/Xlib.h>
#include <blend2d/api.h>
#include <blend2d/context.h>
#include <cassert>
#include <chrono>
#include <fmt/core.h>
#include <imgui.h>
#include <thread>
#include <tracy/Tracy.hpp>

int main() {
  std::string s_ttf_font = "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf";

  IMGUI_CHECKVERSION();
  auto *ctx = ImGui::CreateContext();
  assert(ctx && "Unable to create imgui context");

  BLContextCreateInfo info;
  info.threadCount = 4;

  imx::initialize(s_ttf_font, ImVec4{0.F, 0.F, 0.F, 1.F}, info);

  bool show_demo_window = false;
  bool show_another_window = false;
  ImVec4 clear_color{0.F, 0.F, 0.F, 1.F};
  BLImage &icon = imx::add_texture();
  char const *const texture = "examples/hello_blend/blend2d_logo.png";
  if (icon.readFromFile(texture) != BL_SUCCESS) {
    fmt::print("Failed to load icon from {}\n", texture);
    _exit(-1);
  }

  ImGui::StyleColorsDark();

  int width = 800;
  int height = 600;
  imx::create_window(width, height);

  using target_rate = std::chrono::duration<double, std::ratio<1, 60>>;
  auto stamp = std::chrono::steady_clock::now();
  auto deadline = stamp + target_rate(1);
  while (true) {
    if (deadline > std::chrono::steady_clock::now()) {
      continue;
    }
    deadline += target_rate(1);
    if (imx::poll_events()) {

      static double fps = 0.F;
      static double ren = 0.F;
      using rate = std::chrono::duration<double>;
      stamp = std::chrono::steady_clock::now();
      static auto previous = std::chrono::steady_clock::now();
      ImGui::GetIO().DeltaTime =
          std::chrono::duration_cast<rate>(std::chrono::steady_clock::now() -
                                           previous)
              .count();
      fps = 1. / ImGui::GetIO().DeltaTime;
      previous = stamp;

      ImGui::NewFrame();
      if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
      }
      // 2. Show a simple window that we create ourselves. We use a
      // Begin/End pair to create a named window.
      {
        static float f = 0.0F;
        static int counter = 0;

        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat(
            "float", &f, 0.0F,
            1.0F); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit4(
            "clear color",
            (float *)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button")) {

          counter++;
        }
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS) render(%.1f)",
                    1000.0F / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate, fps);
        ImGui::Image(static_cast<ImTextureID>(&icon),
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
      if (!imx::draw_frame(ImGui::GetDrawData(), clear_color)) {
        fmt::print("Failed to draw frame\n");
      }
    }
    std::this_thread::sleep_until(deadline);
  }
  ImGui::DestroyContext();
  return 0;
}
