#include <kf/game/audio.h>
#include <kf/game/resources.h>
#include <kf/game/graphics.h>

#include <kf/lib/overlay.h>
#include <kf/game/player.h>
#include <kf/game/save.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/game/game.h>

enum {
    FRAME_PACER_INTERVAL_TICKS = 3,
    ENDING_MASTER_FADE_STEP_Q8 = 0x80
};

static std::uint64_t frame_pacer_last_tick;

KfOverlayResultWord game_next_overlay_mode;

void automap_publish(void);

void game_main_loop(void)
{
    // A language chosen during the opening takes effect before anything loads.
    kf::language_apply_pending();
    game_use_language_labels();
    memset((void *)&game_graphics_runtime, 0, sizeof game_graphics_runtime);
    memset((void *)&actor_state, 0, sizeof actor_state);
    memset((void *)&map_object_state, 0, sizeof map_object_state);
    memset((void *)&effect_state, 0, sizeof(KfEffectState));
    memset((void *)map_runtime_state.events, 0, sizeof map_runtime_state.events);
    memset((void *)&player_state, 0, sizeof(KfPlayerState));
    memory_set_allocation_mode(memory_arena, KF_MEMORY_CREATE_ARENA);
    audio_initialize();
    display_initialize();
    item_load_database();
    actor_pool_clear();
    map_object_pool_clear();
    effect_pool_reset();
    map_event_timers_reset();
    common_resources_load();
    game_initialize_session();
    // KF_TEST_GIVE=<item id>,...: adds items for automated menu tests.
    if (const char *give = std::getenv("KF_TEST_GIVE")) {
        for (const char *cursor = give; *cursor;) {
            const int id = std::atoi(cursor);
            if (id >= 0 && id < KF_ITEM_COUNT)
                item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][id]++;
            const char *comma = std::strchr(cursor, ',');
            if (!comma)
                break;
            cursor = comma + 1;
        }
    }
    memory_set_allocation_mode(memory_arena, KF_MEMORY_REBASE_ARENA);
    map_load_floor_wrapper();
    frame_pacer_last_tick = kf::host_clock_tick();
    player_warp_shimmer_at_player(KF_WARP_SHIMMER_SHRINK_REMOVE);
    game_next_overlay_mode = KF_OVERLAY_MODE_NONE;
    for (;;) {
        // Between updates no menu or dialogue holds text, so a language change
        // requested from the F1 overlay can be applied here.
        game_apply_language();
        player_update();
        if (game_next_overlay_mode != KF_OVERLAY_MODE_NONE) {
            break;
        }
        player_update_transform_snapshot(&player_position_snapshot, &player_rotation_snapshot);
        audio_set_listener_transform(audio_state, &player_position_snapshot, &player_rotation_snapshot);
        actor_set_player_transform(&player_position_snapshot, &player_rotation_snapshot);
        actor_pool_update();
        map_object_pool_update();
        effect_pool_sweep();
        map_event_pool_update();
        render_frame(&player_position_snapshot, &player_rotation_snapshot);
        automap_publish();
        player_state.allow_near_actor_spawn = KF_ACTOR_NEAR_SPAWN_FORBIDDEN;
        if (player_current_map_attribute()
            == KF_MAP_ATTRIBUTE_WARP) {
            if (!map_cells_equal(player_state.previous_map_cell, player_state.motion_state.map_cell)) {
                if (player_warp_trigger_update() != 0) {
                    game_next_overlay_mode = KF_OVERLAY_MODE_ENDING;
                    player_warp_shimmer_at_player(KF_WARP_SHIMMER_GROW_KEEP);
                    display_play_transition();
                    audio_stop_sequence_master_fade(ENDING_MASTER_FADE_STEP_Q8);
                    break;
                }
                player_state.previous_map_cell.x = player_state.motion_state.map_cell.x;
                player_state.previous_map_cell.z = player_state.motion_state.map_cell.z;
            }
        } else {
            player_state.previous_map_cell.z = KF_MAP_CELL_COORD_INVALID;
            player_state.previous_map_cell.x = KF_MAP_CELL_COORD_INVALID;
        }
    }
    game_shutdown();
}

void game_shutdown(void)
{
    audio_close_vab(audio_state);
}

void frame_pacer_wait(void)
{
    kf::host_wait_until_tick(frame_pacer_last_tick + FRAME_PACER_INTERVAL_TICKS);
    frame_pacer_last_tick = kf::host_clock_tick();
}

void game_reset_module_state(void)
{
    kf::restore_initial_value<frame_pacer_last_tick>();
    kf::restore_initial_value<game_next_overlay_mode>();
}
