# Engine CMakeLists.txt

set(TARGET_NAME Powertrain)

# Collect the source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

# Collect the public header files
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp"
)

# Collect the private header files
file(GLOB_RECURSE INTERNALS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Internal/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Internal/*.hpp"
)

# Collect the shaders
file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.hlsli"
)

# Create the static library
add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS} ${INTERNALS})

# IDE source layout
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Include" PREFIX "Include" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Internal" PREFIX "Internal" FILES ${INTERNALS})

# Include directories, only Include is visible to sandbox
target_include_directories(${TARGET_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Internal"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
)

# C++ standard
target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)

# Warnings
target_compile_options(${TARGET_NAME} PRIVATE
    /W4
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /MP
)

# Link libraries
target_link_libraries(${TARGET_NAME}
    PRIVATE
        d3d12
        dxgi
        dxguid
        user32
        xinput
        ImGui
)

# Compiler definitions
target_compile_definitions(${TARGET_NAME}
    PUBLIC
        $<$<CONFIG:Debug>:PT_DEBUG>
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
)

# Window specific options
target_link_options(${TARGET_NAME}
    INTERFACE
        $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
        $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
)

# Engine properties
set_target_properties(${TARGET_NAME} PROPERTIES 
    # Set output directory
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_BASE}"

    # Set C++ standard
    CXX_EXTENSIONS OFF
)

# Shader build step
file(GLOB DXC_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
list(SORT DXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
if(DXC_CANDIDATES)
    list(GET DXC_CANDIDATES 0 DXC_DEFAULT)
else()
    set(DXC_DEFAULT "DXC_EXECUTABLE-NOTFOUND")
endif()
set(DXC_EXECUTABLE "${DXC_DEFAULT}" CACHE FILEPATH "Path to dxc.exe")
if(NOT EXISTS "${DXC_EXECUTABLE}")
    message(FATAL_ERROR "dxc.exe not found in any Windows SDK; set DXC_EXECUTABLE")
endif()

file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.hlsli")
set(SHADER_OUTPUTS)

# One entry point per call
function(powertrain_compile_shader SOURCE PROFILE ENTRY)
    get_filename_component(SHADER_NAME "${SOURCE}" NAME_WE)
    set(SHADER_OUTPUT "${OUTPUT_BASE}/Shaders/${SHADER_NAME}.${ENTRY}.cso")

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_BASE}/Shaders"
        COMMAND "${DXC_EXECUTABLE}" -T ${PROFILE} -E ${ENTRY} -HV 2021 -WX
                "$<IF:$<CONFIG:Debug>,-Od;-Zi;-Qembed_debug,-O3>"
                -I "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
                -Fo "${SHADER_OUTPUT}" "${SOURCE}"
        DEPENDS "${SOURCE}" ${SHADER_INCLUDES}
        COMMENT "Compiling ${SHADER_NAME}.hlsl (${ENTRY})"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} "${SHADER_OUTPUT}" PARENT_SCOPE)
endfunction()

powertrain_compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/DebugLine.hlsl" vs_6_0 VSMain)
powertrain_compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/DebugLine.hlsl" ps_6_0 PSMain)
powertrain_compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Forward.hlsl" vs_6_6 VSMain)
powertrain_compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Forward.hlsl" ps_6_6 PSMain)

add_custom_target(PowertrainShaders DEPENDS ${SHADER_OUTPUTS} SOURCES ${SHADERS})
set_target_properties(PowertrainShaders PROPERTIES FOLDER "Utility")
add_dependencies(${TARGET_NAME} PowertrainShaders)