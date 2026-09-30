#include <kf/platform/prelude.h>
#include <kf/cutscene/audio.h>
#include <kf/cutscene/controller.h>
#include <kf/cutscene/opening_helpers.h>
#include <kf/cutscene/opening_scenes.h>
#include <kf/cutscene/playback.h>
#include <kf/cutscene/render.h>
#include <kf/cutscene/resources.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/memory.h>
#include <kf/lib/null.h>
#include <kf/lib/resource_file.h>
#include <kf/lib/resources.h>
#include <kf/platform/input.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

std::array<char, KF_OPENING_INITIAL_TIM_PATH_BYTES> opening_initial_tim_path = {"B0/L0."};

static void opening_load_skip_assets(void)
{
    u8 *tim_data;
    std::size_t tim_size;
    memory_allocation_reset(cutscene_memory_arena);
    resource_file_load_allocated(cutscene_memory_arena, &tim_data, "B0/MIX3.", &tim_size);
    tim_upload_images(tim_data, tim_size);
    memory_release_last(cutscene_memory_arena);
    audio_stop_sequence(KF_AUDIO_STOP_FADE);
}

void opening_run(Cutscene scene)
{
    u8 *tim_data;
    std::size_t tim_size;
    KfOpeningInputAction advance_action;
    KfOpeningInputAction skip_action;

    open_graphics_runtime = {};
    opening_entity_state = {};
    memory_set_allocation_mode(cutscene_memory_arena, KF_MEMORY_CREATE_ARENA);
    cutscene_audio_initialize();
    cutscene_display_initialize(scene);
    opening_entity_pool_reset();
    memory_set_allocation_mode(cutscene_memory_arena, KF_MEMORY_REBASE_ARENA);

    switch (scene) {
    case Cutscene::Intro:
        if (resource_file_load_into(
                (void *)open_graphics_runtime.display_state.asset_load_buffer,
                open_graphics_runtime.display_state.asset_load_capacity,
                opening_initial_tim_path.data(), &tim_size) != KF_RESOURCE_LOADED) {
            resource_file_fail(opening_initial_tim_path.data());
        }
        advance_action = KF_OPENING_INPUT_ADVANCE;
        tim_upload_images(open_graphics_runtime.display_state.asset_load_buffer, tim_size);
        skip_action = KF_OPENING_INPUT_SKIP;
        opening_fade_in();
        resource_file_load_allocated(cutscene_memory_arena, &tim_data, "B0/MIX0.", &tim_size);
        tim_upload_images(tim_data, tim_size);
        memory_release_last(cutscene_memory_arena);
        opening_input_action = KF_OPENING_INPUT_NONE;

        for (;;) {
            opening_scene0_run();
            if (opening_input_action != advance_action &&
                opening_input_action == skip_action) {
                opening_load_skip_assets();
                break;
            }

            opening_input_action = KF_OPENING_INPUT_NONE;
            opening_scene1_run();
            if (opening_input_action != advance_action) {
                if (opening_input_action == skip_action) {
                    opening_load_skip_assets();
                    break;
                }
                continue;
            }

            audio_stop_sequence(KF_AUDIO_STOP_FADE);
            opening_input_action = KF_OPENING_INPUT_NONE;
            opening_scene3_run();

            audio_stop_sequence(KF_AUDIO_STOP_FADE);
            break;
        }
        break;

    case Cutscene::Ending:
        opening_ending_scene_run();
        opening_ending_scroll_run();
        break;
    }

    opening_fade_in();
    audio_close_vab(cutscene_audio_state);
}

void opening_controller_reset_module_state(void)
{
    kf::restore_initial_value<opening_initial_tim_path>();
}
