#include "imx/platform.h"
#include "imx/platform_details.h"
#include "imx/render.h"
#include <chrono>
#include <fmt/core.h>
#include <imgui.h>
#include <tracy/Tracy.hpp>
#include <cassert>

int main() {
  IMGUI_CHECKVERSION();
  int width = 800;
  int height = 600;



  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  auto* ctx = ImGui::CreateContext();
  assert(ctx && "Unable to create imgui context");
  ImGui::SetCurrentContext(ctx);
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
  io.DisplaySize = ImVec2(width, height);
  auto &style = ImGui::GetStyle();
  ImFont *fnt = io.Fonts->AddFontFromFileTTF(
      "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf", 24);
  io.FontDefault = fnt;
  ImFontConfig fontConfig;
  fontConfig.GlyphMinAdvanceX = 1.0f;
  fontConfig.SizePixels = 24.00;
  io.Fonts->AddFontDefault(&fontConfig);
  io.FontGlobalScale = 1.;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  fmt::print("ImGui initialized");

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
