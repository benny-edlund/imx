# Detect build type from the CMake cache
set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")

# Map CMake build types to Conan profiles
if("${BUILD_TYPE}" STREQUAL "Debug")
    set(CONAN_PROFILE "conan-debug")
elseif("${BUILD_TYPE}" STREQUAL "Release")
    set(CONAN_PROFILE "conan-optimized")
elseif("${BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set(CONAN_PROFILE "conan-sanitized")
else()
    set(CONAN_PROFILE "conan-default")
endif()

# Set the path for the marker file
set(MARKER_FILE "${CMAKE_BINARY_DIR}/ran_command.marker")
# Check if the marker file exists
if(NOT EXISTS "${MARKER_FILE}")
    # Run conan install command with the appropriate profile before configuring the project
    execute_process(
        COMMAND conan install . --profile "${CMAKE_CURRENT_LIST_DIR}/.conan2/dynamic" --build=missing --settings=build_type=${BUILD_TYPE}
        RESULT_VARIABLE CONAN_INSTALL_RESULT
        OUTPUT_VARIABLE CONAN_INSTALL_OUTPUT
    )
endif()

if(NOT "${CONAN_INSTALL_RESULT}" STREQUAL "0")
    message(FATAL_ERROR "Conan install failed: ${CONAN_INSTALL_OUTPUT}")
endif()

set( CONAN_TOOLCHAIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/generators/conan_toolchain.cmake")
message( "Conan toochain file generated at: ${CONAN_TOOLCHAIN_FILE}" )