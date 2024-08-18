include(GenerateExportHeader)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

find_package(blend2d)
find_package(Tracy)
find_package(imgui)
find_package(fmt)

set(HEADER_LIST 
	${CMAKE_CURRENT_BINARY_DIR}/imx/api.h
	${CMAKE_CURRENT_SOURCE_DIR}/public/imx/render.h
	${CMAKE_CURRENT_SOURCE_DIR}/public/imx/platform.h
	${CMAKE_CURRENT_SOURCE_DIR}/public/imx/platform_details.h
)

# # Static library
# add_library(imx_static render.cpp "${HEADER_LIST}")
# target_link_libraries(imgui::imgui blend2d::blend2d tracy )
# target_include_directories(
#   imx_static 
#   PUBLIC  
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public> 
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
# )
# target_include_directories(imx_static PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
# target_compile_definitions( imx_static PUBLIC imx_STATIC_DEFINE )

# Shared library
add_library(imx SHARED render.cpp platform.cpp "${HEADER_LIST}")
target_link_libraries( imx blend2d::blend2d Tracy::TracyClient imgui::imgui fmt::fmt )
target_include_directories(
  imx 
  PUBLIC  
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public> 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  )
  target_include_directories(imx PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
  target_compile_definitions( imx PUBLIC imx_DEFINE )

generate_export_header(
  imx
  BASE_NAME IMX
  EXPORT_FILE_NAME imx/api.h
  EXPORT_MACRO_NAME IMX_API
  NO_EXPORT_MACRO_NAME IMX_HIDDEN
)

# install(TARGETS imx imx_static
install(TARGETS imx
EXPORT imx_targets
LIBRARY DESTINATION lib
ARCHIVE DESTINATION lib
RUNTIME DESTINATION bin
INCLUDES DESTINATION include
)

install(DIRECTORY public/imx DESTINATION include FILES_MATCHING PATTERN "*.h")
install(FILES ${PROJECT_BINARY_DIR}/lib/imx/api.h DESTINATION include/imx)

include(CMakePackageConfigHelpers)
write_basic_package_version_file( imxConfigVersion.cmake VERSION "${imx_VERSION}" COMPATIBILITY AnyNewerVersion )
export(EXPORT imx_targets FILE "${CMAKE_CURRENT_BINARY_DIR}/imxTargets.cmake"  NAMESPACE imx:: )
configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/imxConfig.cmake" INSTALL_DESTINATION lib/cmake/imx)
install(EXPORT imx_targets FILE imxTargets.cmake DESTINATION lib/cmake/imx NAMESPACE imx:: )
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/imxConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/imxConfigVersion.cmake"
  DESTINATION lib/cmake/imx 
)

if ( CMAKE_COMPILER_IS_GNUCXX AND ENABLE_COVERAGE )
  target_compile_options(imx PRIVATE -g -coverage )
  # target_compile_options(imx_static PRIVATE -g -coverage )
endif()

if ( ENABLE_DEVELOPER_MODE )
  target_compile_options(imx PRIVATE -fsanitize=address,undefined,leak )
  target_link_options(imx PRIVATE -fsanitize=address,undefined,leak )
endif()

if(MSVC)
  set_target_properties(imx PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$(VC_ExecutablePath_x64);%PATH%")
endif()
