# Root CMakeLists.txt

cmake_minimum_required(VERSION 3.30)
project(RacingGame LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "RacingGame requires Windows and MSVC or clang-cl")
endif()

# Global settings
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Convert CMake architecture name to more readable architecture name
if(DEFINED CMAKE_VS_PLATFORM_NAME)
    string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" ARCH_NAME)
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_NAME)
endif()

if(ARCH_NAME MATCHES "amd64|x86_64|x64")
    set(ARCH_NAME "x86_64")
elseif(ARCH_NAME MATCHES "i[3-6]86|win32|x86")
    set(ARCH_NAME "x86_32")
elseif(ARCH_NAME MATCHES "aarch64|arm64")
    set(ARCH_NAME "arm64")
endif()

# Output directory
set(OUTPUT_BASE "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${ARCH_NAME}/${PROJECT_NAME}")

# Copy assets
add_custom_target(CopyAssets
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/Assets" "${OUTPUT_BASE}/Assets"
    VERBATIM
)

set_target_properties(CopyAssets PROPERTIES FOLDER "Utility")

# Subdirectories, Vendor first
add_subdirectory(Vendor)
add_subdirectory(Powertrain)
add_subdirectory(Powertrain-Sandbox)

# Visual Studio startup project
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Sandbox)