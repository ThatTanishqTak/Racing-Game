# Vendor CMakeLists.txt

# Dear ImGui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGui")
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx12.cpp"
)

set(IMGUI_HEADERS
    "${IMGUI_DIR}/imconfig.h"
    "${IMGUI_DIR}/imgui.h"
    "${IMGUI_DIR}/imgui_internal.h"
    "${IMGUI_DIR}/backends/imgui_impl_win32.h"
    "${IMGUI_DIR}/backends/imgui_impl_dx12.h"
)

add_library(ImGui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})

# Consumers include <imgui.h> and <imgui_impl_*.h> directly
target_include_directories(ImGui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")

target_compile_features(ImGui PUBLIC cxx_std_23)
target_compile_options(ImGui PRIVATE /utf-8 /MP)

# WindowsInput owns the pads, so the Win32 backend must not poll XInput as well
target_compile_definitions(ImGui
    PUBLIC
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    PRIVATE
        IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
)

# The DX12 backend compiles its shaders with D3DCompile at runtime; the Win32 backend queries DWM for DPI
target_link_libraries(ImGui PUBLIC d3d12 dxgi d3dcompiler dwmapi)

set_target_properties(ImGui PROPERTIES
    FOLDER "Vendor"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_BASE}"
    CXX_EXTENSIONS OFF
)