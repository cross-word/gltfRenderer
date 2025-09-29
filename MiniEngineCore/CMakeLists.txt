add_executable(MiniEngineCore WIN32
    MiniWindow.cpp
    MiniTimer.cpp
    AgilitySDK.cpp
    stdafx.cpp
)

set_target_properties(MiniEngineCore PROPERTIES
    OUTPUT_NAME MiniEngineCore
)

find_path(D3DX12_INCLUDE_DIR d3dx12.h HINTS ${CMAKE_SOURCE_DIR}/external/d3dx12)
if(NOT D3DX12_INCLUDE_DIR)
  message(FATAL_ERROR "d3dx12.h not found")
endif()

if(MSVC)
    target_compile_options(MiniEngineCore PRIVATE /W4 /permissive- /wd4100 /wd4201)
else()
    target_compile_options(MiniEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_precompile_headers(MiniEngineCore PRIVATE stdafx.h)

target_include_directories(MiniEngineCore
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${D3DX12_INCLUDE_DIR}
)

target_compile_definitions(MiniEngineCore
    PRIVATE
        UNICODE
        _UNICODE
)

target_link_libraries(MiniEngineCore
    PRIVATE
        RenderDX12
        d2d1
        shell32
        shcore
)

file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.inl"
)
target_sources(MiniEngineCore PRIVATE ${CORE_HEADERS})
source_group("Header Files" FILES ${CORE_HEADERS})