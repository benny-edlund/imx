cmake_minimum_required(VERSION 3.16...3.23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ENABLE_DEVELOPER_MODE OFF CACHE BOOL "Enable 'developer mode'")
set(ENABLE_COVERAGE OFF CACHE BOOL "Enable 'coverage mode'")
set(OPT_WARNINGS_AS_ERRORS_DEVELOPER_DEFAULT TRUE)

# Set the project name and language
project(
  imx
  VERSION 1.0.0
  DESCRIPTION "A backend for ImGui built on blend2d & X11"
  HOMEPAGE_URL "%%https://github.com/benny-edlund/imx%%"
  LANGUAGES CXX )

# # # This variable is set by project() in CMake 3.21+
# string(
#   COMPARE EQUAL
#           "${CMAKE_SOURCE_DIR}"
#           "${PROJECT_SOURCE_DIR}"
#           PROJECT_IS_TOP_LEVEL)
# if(PROJECT_IS_TOP_LEVEL)
#   include(CTest)
#   if(CMAKE_COMPILER_IS_GNUCXX)
#     LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules")
#   endif()
# endif()

set(GIT_SHA "Unknown" CACHE STRING "SHA this build was generated from")
string( SUBSTRING "${GIT_SHA}" 0 8 GIT_SHORT_SHA)

add_subdirectory(lib)

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
