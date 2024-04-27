cmake_minimum_required(VERSION 3.15)
project(imgui_blend_backend CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(imgui)
find_package(blend2d)
find_package(fmt)
find_package(GSL)
find_package(Tracy)
find_package(glfw3)

add_executable(
        imgui_blend_backend 
        src/imblend_render_backend.cpp
        src/main_blend2d.cpp )
target_compile_definitions(imgui_blend_backend PRIVATE "TRACY_ENABLE")
target_include_directories(imgui_blend_backend PRIVATE "~/projects/tracy/public")
target_link_libraries(imgui_blend_backend imgui::imgui blend2d::blend2d X11 Xext fmt::fmt tracy )

include(GenerateExportHeader)
generate_export_header(
  imx
  BASE_NAME IMX
  EXPORT_FILE_NAME imx/api.h
  EXPORT_MACRO_NAME IMX_API
  NO_EXPORT_MACRO_NAME IMX_HIDDEN
)


add_executable(
        imgui_glfw_backend 
        src/main_glfw.cpp  
        src/backends/imgui_impl_glfw.cpp 
        src/backends/imgui_impl_opengl3.cpp 
        )
target_compile_definitions(imgui_glfw_backend PRIVATE "TRACY_ENABLE")
target_include_directories(imgui_glfw_backend PRIVATE "backends")
target_include_directories(imgui_glfw_backend PRIVATE "~/projects/tracy/public")
target_link_libraries(imgui_glfw_backend imgui::imgui fmt::fmt glfw opengl::opengl tracy )


install(TARGETS imgui_blend_backend DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
