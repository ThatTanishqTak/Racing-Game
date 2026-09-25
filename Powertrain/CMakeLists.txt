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