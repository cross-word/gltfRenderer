find_package(directxtex CONFIG REQUIRED)          # Microsoft::DirectXTex
find_package(directxtk12 CONFIG QUIET)            # Microsoft::DirectXTK12
find_package(winpixevent CONFIG QUIET)     # Microsoft::WinPixEventRuntime
find_package(imgui CONFIG REQUIRED)               # imgui::imgui

set(IMGUI_BACKENDS_DIR ${CMAKE_SOURCE_DIR}/external/imgui_backends)

if(NOT EXISTS "${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp")
  message(STATUS "ImGui backends not found in repo. Fetching from upstream...")
  include(FetchContent)
  FetchContent_Declare(imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
  )
  FetchContent_Populate(imgui_src)
  set(IMGUI_BACKENDS_DIR ${imgui_src_SOURCE_DIR}/backends)
endif()

find_path(D3DX12_INCLUDE_DIR d3dx12.h HINTS ${CMAKE_SOURCE_DIR}/external/d3dx12)
if(NOT D3DX12_INCLUDE_DIR)
  message(FATAL_ERROR "d3dx12.h not found")
endif()

add_library(RenderDX12 SHARED
    D3DCamera.cpp
    D3DTimer.cpp
    D3DUtil.cpp
    DX12CommandList.cpp
    DX12ConstantManager.cpp
    DX12DDSManager.cpp
    DX12DescriptorHeap.cpp
    DX12Device.cpp
    DX12FrameBuffer.cpp
    DX12FrameResource.cpp
    DX12PSO.cpp
    DX12RenderGeometry.cpp
    DX12Resource.cpp
    DX12RootSignature.cpp
    DX12ShadowManager.cpp
    DX12SwapChain.cpp
    DX12TextureManager.cpp
    DX12View.cpp
    RenderDX12.cpp
    stdafx.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_dx12.cpp
)

set_target_properties(RenderDX12 PROPERTIES
    OUTPUT_NAME RenderDX12
)

target_include_directories(RenderDX12
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR}/FileLoader
          ${IMGUI_BACKENDS_DIR}
          ${D3DX12_INCLUDE_DIR}
          ${RenderDX12_PIX_INCLUDE}
)

target_link_libraries(RenderDX12
  PRIVATE
    FileLoader
    imgui::imgui
    Microsoft::DirectXTex
    $<$<TARGET_EXISTS:Microsoft::DirectXTK12>:Microsoft::DirectXTK12>
    $<$<TARGET_EXISTS:Microsoft::WinPixEventRuntime>:Microsoft::WinPixEventRuntime>
    d3d12 dxgi d3dcompiler dxguid user32 gdi32 shell32 imm32 shcore
)

if(MSVC)
  target_compile_options(RenderDX12 PRIVATE /W4 /permissive- /wd4100 /wd4201 /wd4324)
else()
  target_compile_options(RenderDX12 PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(RenderDX12 PRIVATE _BUILDING_RENDERDX12 UNICODE _UNICODE)
target_precompile_headers(RenderDX12 PRIVATE stdafx.h)

file(GLOB_RECURSE RENDERDX12_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.inl"
)
target_sources(RenderDX12 PRIVATE ${RENDERDX12_HEADERS})
source_group("Header Files" FILES ${RENDERDX12_HEADERS})