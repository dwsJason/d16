cmake_minimum_required(VERSION 3.16)
project(d16 LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find SDL2 and SDL2_image via pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

# --- Find OpenGL ---
find_package(OpenGL REQUIRED)

# --- Application source files ---
set(APP_SOURCES
    source/main.cpp
    source/imagedoc.cpp
    source/sdl_helpers.cpp
    source/toolbar.cpp
    source/paldoc.cpp
    source/icon.cpp
    source/lzb.cpp
    source/xu_line.cpp
    source/compiled.cpp
    source/256_file.cpp
    source/anm_file.cpp
    source/blit_file.cpp
    source/c2_file.cpp
    source/fan_file.cpp
    source/flc_file.cpp
    source/gsdx_file.cpp
    source/gsla_file.cpp
    source/obj_file.cpp
    source/common/cursor.cpp
    source/common/limage.cpp
    source/common/log.cpp
)

# --- ImGui sources ---
set(IMGUI_SOURCES
    libs/imgui/imgui.cpp
    libs/imgui/imgui_demo.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/examples/imgui_impl_opengl3.cpp
    libs/imgui/examples/imgui_impl_sdl.cpp
)

# --- GL3W (OpenGL loader) ---
set(GL3W_SOURCES
    libs/imgui/examples/libs/gl3w/GL/gl3w.c
)

# --- libimagequant ---
set(IMAGEQUANT_SOURCES
    libs/libimagequant-msvc/blur.c
    libs/libimagequant-msvc/kmeans.c
    libs/libimagequant-msvc/libimagequant.c
    libs/libimagequant-msvc/mediancut.c
    libs/libimagequant-msvc/mempool.c
    libs/libimagequant-msvc/nearest.c
    libs/libimagequant-msvc/pam.c
)

# --- giflib ---
set(GIFLIB_SOURCES
    libs/giflib-5.2.2/dgif_lib.c
    libs/giflib-5.2.2/egif_lib.c
    libs/giflib-5.2.2/getarg.c
    libs/giflib-5.2.2/gifalloc.c
    libs/giflib-5.2.2/gif_err.c
    libs/giflib-5.2.2/gif_hash.c
    libs/giflib-5.2.2/openbsd-reallocarray.c
)

# --- lzsa ---
set(LZSA_SOURCES
    libs/lzsa/src/dictionary.c
    libs/lzsa/src/expand_block_v1.c
    libs/lzsa/src/expand_block_v2.c
    libs/lzsa/src/expand_context.c
    libs/lzsa/src/expand_inmem.c
    libs/lzsa/src/expand_streaming.c
    libs/lzsa/src/frame.c
    libs/lzsa/src/matchfinder.c
    libs/lzsa/src/shrink_block_v1.c
    libs/lzsa/src/shrink_block_v2.c
    libs/lzsa/src/shrink_context.c
    libs/lzsa/src/shrink_inmem.c
    libs/lzsa/src/shrink_streaming.c
    libs/lzsa/src/stream.c
    libs/lzsa/src/libdivsufsort/lib/divsufsort.c
    libs/lzsa/src/libdivsufsort/lib/divsufsort_utils.c
    libs/lzsa/src/libdivsufsort/lib/sssort.c
    libs/lzsa/src/libdivsufsort/lib/trsort.c
)

# --- lz4 ---
set(LZ4_SOURCES
    libs/lz4-dev/lib/lz4.c
    libs/lz4-dev/lib/lz4frame.c
    libs/lz4-dev/lib/lz4hc.c
    libs/lz4-dev/lib/xxhash.c
)

# --- flic ---
set(FLIC_SOURCES
    libs/flic/decoder.cpp
    libs/flic/encoder.cpp
    libs/flic/stdio.cpp
)

# --- Native File Dialog (platform-specific) ---
if(APPLE)
    set(NFD_SOURCES libs/nativefiledialog-extended/src/nfd_cocoa.m)
elseif(WIN32)
    set(NFD_SOURCES libs/nativefiledialog-extended/src/nfd_win.cpp)
else()
    set(NFD_SOURCES libs/nativefiledialog-extended/src/nfd_portal.cpp)
endif()

# --- Combine all sources ---
add_executable(d16
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
    ${GL3W_SOURCES}
    ${IMAGEQUANT_SOURCES}
    ${GIFLIB_SOURCES}
    ${LZSA_SOURCES}
    ${LZ4_SOURCES}
    ${FLIC_SOURCES}
    ${NFD_SOURCES}
)

# --- Include directories ---
target_include_directories(d16 PRIVATE
    source
    source/common
    source/include
    libs/avir
    libs/flic
    libs/giflib-5.2.2
    libs/imgui
    libs/imgui/examples
    libs/imgui/examples/libs/gl3w
    libs/libimagequant-msvc
    libs/lz4-dev/lib
    libs/lzsa/src
    libs/lzsa/src/libdivsufsort/include
    libs/lzsa/src/libdivsufsort/lib
    libs/nativefiledialog-extended/src/include
    libs/vectormath
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

# --- Compiler definitions ---
target_compile_definitions(d16 PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GL3W
)

if(WIN32)
    target_compile_definitions(d16 PRIVATE
        GUID_WINDOWS=1
        WIN32=1
        _CRT_SECURE_NO_WARNINGS
    )
    target_include_directories(d16 PRIVATE libs/dirent)
endif()

# --- Library directories (needed for Homebrew on macOS) ---
target_link_directories(d16 PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_IMAGE_LIBRARY_DIRS}
)

# --- Link libraries ---
target_link_libraries(d16 PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    OpenGL::GL
)

# --- Platform-specific link flags ---
if(APPLE)
    target_link_libraries(d16 PRIVATE
        "-framework AppKit"
        "-framework UniformTypeIdentifiers"
    )
endif()

# --- Compiler flags ---
target_compile_options(d16 PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_IMAGE_CFLAGS_OTHER}
)

# Suppress warnings in third-party library code
set_source_files_properties(
    ${IMAGEQUANT_SOURCES}
    ${GIFLIB_SOURCES}
    ${LZSA_SOURCES}
    ${LZ4_SOURCES}
    ${GL3W_SOURCES}
    PROPERTIES COMPILE_FLAGS "-w"
)
