cmake_minimum_required(VERSION 3.16...3.23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ENABLE_DEVELOPER_MODE OFF CACHE BOOL "Enable 'developer mode'")
set(ENABLE_COVERAGE OFF CACHE BOOL "Enable 'coverage mode'")
set(OPT_WARNINGS_AS_ERRORS_DEVELOPER_DEFAULT TRUE)

include( dependencies.cmake )
set( CMAKE_TOOLCHAIN_FILE ${CONAN_TOOLCHAIN_FILE} )

# Set the project name and language
project(
  imx
  VERSION 1.0.0
  DESCRIPTION "A backend for ImGui built on blend2d & X11"
  HOMEPAGE_URL "%%https://github.com/benny-edlund/imx%%"
  LANGUAGES CXX )

add_subdirectory(lib)
add_subdirectory(examples)

# Adding the tests:
option(ENABLE_TESTING "Enable the tests" ${PROJECT_IS_TOP_LEVEL})
if(ENABLE_TESTING)
  enable_testing()
   add_subdirectory(test)
endif()

if(CMAKE_SKIP_INSTALL_RULES)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${imx_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${imx_VERSION_MINOR}")
include(CPack)
