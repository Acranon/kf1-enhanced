cmake_minimum_required(VERSION 3.25)
project(kings_field VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
if(EMSCRIPTEN)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
  add_link_options(-fexceptions)
endif()
option(KF_SANITIZE "Enable address and undefined behavior sanitizers" OFF)
if(KF_SANITIZE AND NOT EMSCRIPTEN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(kf_codecs STATIC codecs/tim.cpp codecs/audio.cpp codecs/resources.cpp)
target_include_directories(kf_codecs PUBLIC include)
target_compile_options(kf_codecs PRIVATE -Wall -Wextra -Wpedantic -Werror)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
# The Nix dev shell and package supply a delta derived from the translator's PPF
# release; otherwise a local english-v1.kfdelta in the build directory is used.
if(DEFINED ENV{KF_ENGLISH_PATCH} AND NOT "$ENV{KF_ENGLISH_PATCH}" STREQUAL "")
  set(KF_ENGLISH_PATCH_DEFAULT "$ENV{KF_ENGLISH_PATCH}")
else()
  set(KF_ENGLISH_PATCH_DEFAULT "${CMAKE_BINARY_DIR}/english-v1.kfdelta")
endif()
set(KF_ENGLISH_PATCH "${KF_ENGLISH_PATCH_DEFAULT}" CACHE FILEPATH
    "English translation delta to embed (optional; never distribute builds containing it)")
# Checked on every build, so a payload added after configuring is picked up;
# the generated file is rewritten only when its content changes.
add_custom_target(kf_english_patch
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/english_patch.py" embed
    --input "${KF_ENGLISH_PATCH}" --optional --output "${CMAKE_BINARY_DIR}/generated/english_patch.inc"
  BYPRODUCTS "${CMAKE_BINARY_DIR}/generated/english_patch.inc" VERBATIM)
set(KF_GAME_SOURCES
  src/game/display_play_transition.cpp
  src/game/player_warp_shimmer_at_player.cpp
  src/game/game.cpp
  src/game/equipment.cpp
  src/game/player_death.cpp
  src/game/player_core.cpp
  src/game/player_use_item.cpp
  src/game/player_death_fade.cpp
  src/game/lighting_presets.cpp
  src/game/player_fire_defense.cpp
  src/game/player_update.cpp
  src/game/collision_grid.cpp
  src/game/collision.cpp
  src/game/resource_file.cpp
  src/game/resources.cpp
  src/game/render.cpp
  src/game/render_enqueuers.cpp
  src/game/render_sprite.cpp
  src/game/render_map_cells.cpp
  src/game/entity_model_render.cpp
  src/game/entity_render.cpp
  src/game/map_event_render.cpp
  src/game/render_scene.cpp
  src/game/geometry_render.cpp
  src/game/render_frame.cpp
  src/game/asset_registry.cpp
  src/game/animation_cache.cpp
  src/game/item.cpp
  src/game/menu.cpp
  src/game/menu_list.cpp
  src/game/menu_map_viewer.cpp
  src/game/menu_panels.cpp
  src/game/menu_select.cpp
  src/game/menu_runtime.cpp
  src/game/save_system.cpp
  src/game/automap_feed.cpp
  src/game/actor.cpp
  src/game/actor_behavior.cpp
  src/game/actor_pool.cpp
  src/game/map_object_pool.cpp
  src/game/map_object.cpp
  src/game/audio.cpp
  src/game/camera_path.cpp
  src/game/map_event.cpp
  src/game/lighting.cpp
  src/game/map_scripts.cpp
  src/game/map_events.cpp
  src/game/map_load.cpp
  src/game/player_warp.cpp
  src/game/menu_enter_mode.cpp
  src/game/effect_pool.cpp
  src/game/effect_map_collision.cpp
  src/game/effect_update.cpp
  src/game/effect_dispatch.cpp
  src/game/magic.cpp
  src/game/debug_text.cpp
  src/game/session.cpp
  src/cutscene/opening_render.cpp
  src/cutscene/opening_helpers.cpp
  src/cutscene/camera_path.cpp
  src/cutscene/opening_scenes.cpp
  src/cutscene/opening_fade.cpp
  src/cutscene/opening_controller.cpp
  src/cutscene/resources.cpp
  src/cutscene/render_init.cpp
  src/cutscene/render.cpp
  src/cutscene/render_tmd.cpp
  src/cutscene/render_unlit.cpp
  src/cutscene/render_map.cpp
  src/cutscene/render_sprite.cpp
  src/cutscene/render_map_cells.cpp
  src/cutscene/entity_render.cpp
  src/cutscene/opening_scene0_render.cpp
  src/cutscene/opening_render_entities.cpp
  src/cutscene/opening_entity_pool.cpp
  src/cutscene/audio.cpp
  src/cutscene/audio_spatial.cpp
  src/cutscene/playback.cpp
  src/lib/sprite_add.cpp
  src/lib/matrix_rotation.cpp
  src/lib/vector_math.cpp
  src/lib/camera_path.cpp
  src/lib/memory.cpp
  src/lib/matrix_lerp.cpp
  src/lib/audio.cpp
  src/lib/display.cpp
  src/lib/tmd.cpp
  src/lib/floor_item.cpp
  src/lib/format.cpp
  src/lib/resource_file.cpp
  src/lib/resources.cpp
  src/lib/debug_matrix.cpp
  src/lib/render_view.cpp
  src/lib/render_face.cpp
)
set_source_files_properties(${KF_GAME_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-Wall;-Wextra;-fwrapv;-fno-strict-aliasing")
add_library(kf_support STATIC src/platform/files.cpp src/platform/disc.cpp src/platform/assets.cpp src/platform/language.cpp src/platform/translation.cpp src/platform/translation_ppf.cpp src/platform/controls.cpp src/lib/fixed_math.cpp src/lib/random.cpp)
target_sources(kf_support PRIVATE "${CMAKE_BINARY_DIR}/generated/english_patch.inc")
target_include_directories(kf_support PUBLIC include PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(kf_support PUBLIC kf_codecs)
add_dependencies(kf_support kf_english_patch)
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
add_library(kf_host STATIC src/platform/host.cpp src/platform/saves.cpp src/platform/language_runtime.cpp src/platform/automap.cpp src/audio/sound.cpp src/renderer/renderer.cpp src/renderer/textures.cpp src/renderer/lighting.cpp src/renderer/projection.cpp)
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
  ${KF_GAME_SOURCES})
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
