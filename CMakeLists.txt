cmake_minimum_required(VERSION 3.20)
project(BMUSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── PCAN-Basic SDK (optional) ───────────────────────────────────────────────
# Download from https://www.peak-system.com/PCAN-Basic.239.0.html
# Extract the ZIP and set PCAN_SDK_DIR to the root folder (contains Include/ and Win64/).
set(PCAN_SDK_DIR "" CACHE PATH "Path to PCAN-Basic SDK root (must contain Include/PCANBasic.h)")

# ── ImGui (Win32 + DirectX 11 backend, docking branch) ──────────────────────
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

# ── ImPlot ───────────────────────────────────────────────────────────────────
set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot)
add_library(implot STATIC
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)
target_include_directories(implot PUBLIC ${IMPLOT_DIR})
target_link_libraries(implot PUBLIC imgui)

# ── dbcppp ───────────────────────────────────────────────────────────────────
set(build_tests    OFF CACHE BOOL "" FORCE)
set(build_examples OFF CACHE BOOL "" FORCE)
set(build_tools    OFF CACHE BOOL "" FORCE)
set(build_kcd      OFF CACHE BOOL "" FORCE)

# dbcppp hardcodes add_library(dbcppp SHARED ...) but its Export.h never gets
# DBCPPP_EXPORT defined when building, making every symbol dllimport.
# Intercept add_library to force a static build instead.
macro(add_library name)
    set(_bmu_args ${ARGN})
    if("${name}" STREQUAL "dbcppp")
        list(REMOVE_ITEM _bmu_args SHARED MODULE)
        _add_library(${name} STATIC ${_bmu_args})
    else()
        _add_library(${name} ${_bmu_args})
    endif()
endmacro()

add_subdirectory(third_party/dbcppp)

# Define DBCPPP_EXPORT so that DBCPPP_API expands to dllexport rather than
# dllimport. On a static lib dllexport is harmless; without it the build fails.
target_compile_definitions(dbcppp PUBLIC DBCPPP_EXPORT)

# MinGW + GCC 15 workaround: on Windows the filesystem is case-insensitive, so
# #include <signal.h> (issued deep inside GCC's gthr-default.h → pthread.h)
# resolves to dbcppp's include/dbcppp/Signal.h instead of the system header.
# That causes a circular include that breaks shared_ptr_base.h / concurrence.h.
# Fix: replace INCLUDE_DIRECTORIES with an explicit list that omits include/dbcppp/
# (dbcppp's own headers use relative/quoted includes, so they don't need it;
# consumers use #include <dbcppp/Signal.h> which only needs the include/ parent.)
set_property(TARGET dbcppp PROPERTY INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbcppp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbcppp/third-party/boost"
)
set_property(TARGET dbcppp PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dbcppp/include"
)

# ── PCAN-Basic availability check ────────────────────────────────────────────
set(BMU_PCAN_AVAILABLE OFF)
if(PCAN_SDK_DIR)
    if(EXISTS "${PCAN_SDK_DIR}/Include/PCANBasic.h")
        set(BMU_PCAN_AVAILABLE ON)
        message(STATUS "PCAN-Basic SDK found: ${PCAN_SDK_DIR}")
    else()
        message(WARNING "PCAN_SDK_DIR set but PCANBasic.h not found at ${PCAN_SDK_DIR}/Include/PCANBasic.h")
    endif()
else()
    message(STATUS "PCAN_SDK_DIR not set — PCAN hardware disabled. Virtual interface only.")
endif()

# ── Application ──────────────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.cpp
    src/App.cpp
    src/can/CanBus.cpp
    src/can/VirtualInterface.cpp
    src/dbc/DbcModel.cpp
    src/ui/BusMonitorPanel.cpp
    src/ui/SystemPanel.cpp
    src/ui/PlotPanel.cpp
)

if(BMU_PCAN_AVAILABLE)
    list(APPEND APP_SOURCES src/can/PcanInterface.cpp)
endif()

add_executable(BMUSimulator WIN32 ${APP_SOURCES})

target_include_directories(BMUSimulator PRIVATE src)

target_link_libraries(BMUSimulator PRIVATE
    imgui
    implot
    dbcppp
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    comdlg32
)

target_compile_definitions(BMUSimulator PRIVATE
    $<$<BOOL:${BMU_PCAN_AVAILABLE}>:BMU_PCAN_AVAILABLE>
)

if(BMU_PCAN_AVAILABLE)
    target_include_directories(BMUSimulator PRIVATE "${PCAN_SDK_DIR}/Include")
    target_link_directories(BMUSimulator PRIVATE "${PCAN_SDK_DIR}/Win64")
    target_link_libraries(BMUSimulator PRIVATE PCANBasic)
endif()
