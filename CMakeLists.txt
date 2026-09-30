cmake_minimum_required(VERSION 3.25)
project(kings_field VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions> $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
option(KF_SANITIZE "Enable address and undefined behavior sanitizers" OFF)
if(KF_SANITIZE AND NOT EMSCRIPTEN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

find_program(CARGO_EXECUTABLE cargo REQUIRED)
set(KF_RUST_DIR "${CMAKE_BINARY_DIR}/rust")
set(KF_RUST_ARGS --offline --locked --release --manifest-path ${CMAKE_SOURCE_DIR}/codec-bridge/Cargo.toml)
if(EMSCRIPTEN)
  list(APPEND KF_RUST_ARGS --target wasm32-unknown-emscripten -Zbuild-std=core,compiler_builtins)
  # Cargo 1.95's pinned source-root override avoids mutating the Nix sysroot.
  set(KF_RUST_ENV "__CARGO_TESTS_ONLY_SRC_ROOT=$ENV{KF_RUST_SOURCE}")
  set(KF_CODEC_LIBRARY "${KF_RUST_DIR}/wasm32-unknown-emscripten/release/libkf_codec_bridge.a")
else()
  set(KF_CODEC_LIBRARY "${KF_RUST_DIR}/release/libkf_codec_bridge.a")
endif()
file(GLOB_RECURSE KF_CODEC_SOURCES CONFIGURE_DEPENDS codecs/src/*.rs codec-bridge/src/*.rs)
add_custom_command(OUTPUT "${KF_CODEC_LIBRARY}"
  COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${KF_RUST_DIR} RUSTC_BOOTSTRAP=1
    RUSTFLAGS=-Cpanic=abort ${KF_RUST_ENV} ${CARGO_EXECUTABLE} build ${KF_RUST_ARGS}
  DEPENDS ${KF_CODEC_SOURCES} codecs/Cargo.toml codec-bridge/Cargo.toml codec-bridge/Cargo.lock
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
add_custom_target(kf_codecs_build DEPENDS "${KF_CODEC_LIBRARY}")
add_library(kf_codecs STATIC IMPORTED GLOBAL)
set_target_properties(kf_codecs PROPERTIES IMPORTED_LOCATION "${KF_CODEC_LIBRARY}")
add_dependencies(kf_codecs kf_codecs_build)

include(cmake/OriginalSources.cmake)
add_library(kf_support STATIC src/platform/files.cpp src/platform/disc.cpp src/platform/assets.cpp src/platform/languages.cpp src/platform/translation.cpp src/platform/controls.cpp src/lib/fixed_math.cpp src/lib/random.cpp)
target_include_directories(kf_support PUBLIC include)
target_link_libraries(kf_support PUBLIC kf_codecs)
target_compile_options(kf_support PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(EMSCRIPTEN)
  if(NOT DEFINED ENV{KF_SDL_SOURCE})
    message(FATAL_ERROR "Build in nix develop: KF_SDL_SOURCE must identify the pinned SDL3 source")
  endif()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  add_subdirectory("$ENV{KF_SDL_SOURCE}" "${CMAKE_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)
elseif(WIN32)
  # Windows: the SDL3 MinGW development package is downloaded unless SDL3 is
  # already on CMAKE_PREFIX_PATH.
  find_package(SDL3 CONFIG QUIET)
  if(NOT SDL3_FOUND)
    set(KF_SDL3_VERSION 3.4.16)
    set(KF_SDL3_ROOT "${CMAKE_BINARY_DIR}/SDL3-${KF_SDL3_VERSION}")
    if(NOT EXISTS "${KF_SDL3_ROOT}")
      set(KF_SDL3_ARCHIVE "${CMAKE_BINARY_DIR}/SDL3-devel-${KF_SDL3_VERSION}-mingw.tar.gz")
      message(STATUS "Downloading SDL3 ${KF_SDL3_VERSION} (MinGW)")
      file(DOWNLOAD "https://github.com/libsdl-org/SDL/releases/download/release-${KF_SDL3_VERSION}/SDL3-devel-${KF_SDL3_VERSION}-mingw.tar.gz"
        "${KF_SDL3_ARCHIVE}" STATUS KF_SDL3_STATUS TLS_VERIFY ON)
      list(GET KF_SDL3_STATUS 0 KF_SDL3_CODE)
      if(NOT KF_SDL3_CODE EQUAL 0)
        message(FATAL_ERROR "Cannot download SDL3 (${KF_SDL3_STATUS}). Set CMAKE_PREFIX_PATH to an SDL3 MinGW package instead.")
      endif()
      file(ARCHIVE_EXTRACT INPUT "${KF_SDL3_ARCHIVE}" DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
    find_package(SDL3 CONFIG REQUIRED PATHS "${KF_SDL3_ROOT}/x86_64-w64-mingw32" NO_DEFAULT_PATH)
  endif()
  # Windows has no system libGLESv2: GLES 3.0 is resolved through SDL (vendor/glshim).
  add_library(kf_glshim STATIC vendor/glshim/glshim.c)
  target_include_directories(kf_glshim PUBLIC vendor/gles)
  target_compile_definitions(kf_glshim PUBLIC KHRONOS_STATIC)
  target_link_libraries(kf_glshim PUBLIC SDL3::SDL3)
  set(KF_GLES_LIBRARY kf_glshim)
else()
  find_package(SDL3 CONFIG REQUIRED)
  find_library(KF_GLES_LIBRARY GLESv2 REQUIRED)
endif()
if(NOT EMSCRIPTEN)
  # PC extensions: overlay (Dear ImGui), console, settings and Lua mods.
  set(KF_IMGUI ${CMAKE_SOURCE_DIR}/vendor/imgui)
  file(GLOB KF_LUA_SOURCES ${CMAKE_SOURCE_DIR}/vendor/lua/l*.c)
  list(FILTER KF_LUA_SOURCES EXCLUDE REGEX "/(lua|luac|ltests|onelua)\\.c$")
  add_library(kf_ext STATIC
    src/ext/console.cpp src/ext/settings.cpp src/ext/mods.cpp src/ext/overlay.cpp src/ext/automap.cpp
    ${KF_IMGUI}/imgui.cpp ${KF_IMGUI}/imgui_draw.cpp ${KF_IMGUI}/imgui_tables.cpp ${KF_IMGUI}/imgui_widgets.cpp
    ${KF_IMGUI}/backends/imgui_impl_sdl3.cpp ${KF_IMGUI}/backends/imgui_impl_opengl3.cpp
    ${KF_LUA_SOURCES} vendor/cjson/cJSON.c)
  target_include_directories(kf_ext PUBLIC include ${KF_IMGUI} vendor/lua vendor/cjson)
  target_compile_definitions(kf_ext PUBLIC KF_EXT=1 PRIVATE IMGUI_IMPL_OPENGL_ES3)
  target_link_libraries(kf_ext PUBLIC SDL3::SDL3 ${KF_GLES_LIBRARY})
endif()
add_library(kf_host STATIC src/platform/host.cpp src/platform/saves.cpp src/platform/automap.cpp src/audio/sound.cpp src/renderer/renderer.cpp src/renderer/textures.cpp src/renderer/lighting.cpp src/renderer/projection.cpp)
target_include_directories(kf_host PUBLIC include)
target_link_libraries(kf_host PUBLIC kf_support SDL3::SDL3)
if(WIN32)
  target_link_libraries(kf_host PUBLIC kf_glshim)
endif()
if(TARGET kf_ext)
  target_link_libraries(kf_host PUBLIC kf_ext)
endif()
target_compile_options(kf_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(kings-field src/platform/entry.cpp
  $<TARGET_OBJECTS:kf_game> $<TARGET_OBJECTS:kf_opening> $<TARGET_OBJECTS:kf_common>)
target_link_libraries(kings-field PRIVATE kf_host)
target_compile_options(kings-field PRIVATE -Wall -Wextra)
if(EMSCRIPTEN)
  target_link_options(kings-field PRIVATE
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=262144
    -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=536870912
    -sENVIRONMENT=web -sEXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1
    "-sEXPORTED_RUNTIME_METHODS=['ccall','FS','callMain']"
    "--shell-file=${CMAKE_SOURCE_DIR}/web/shell.html")
  set_target_properties(kings-field PROPERTIES SUFFIX .html)
  set_property(TARGET kings-field APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/shell.html")
else()
  target_link_libraries(kings-field PRIVATE ${KF_GLES_LIBRARY})
endif()
if(WIN32)
  # Static C++ runtime: the game folder needs only SDL3.dll beside the exe.
  target_link_options(kings-field PRIVATE -static)
  # A windowed app (no console window); output goes to kf.log and the F1 console.
  option(KF_WINDOWED "Build without a console window" ON)
  if(KF_WINDOWED)
    target_link_options(kings-field PRIVATE -mwindows)
  endif()
  add_custom_command(TARGET kings-field POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:kings-field>)
endif()
install(TARGETS kings-field RUNTIME DESTINATION bin)
