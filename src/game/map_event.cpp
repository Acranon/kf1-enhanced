#include <kf/platform/prelude.h>
#include <kf/game/asset.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/lib/codec.h>
#include <kf/lib/map.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>

#include <array>

void map_event_set_current(KfMapEvent *event)
{
    map_runtime_state.current_event = event;
}

void map_event_refresh_dialogue_stage(KfMapEvent *event)
{
    if (event->dialogue.stage_limit <= event->dialogue.stage)
        return;
    const u8 highest_floor = kf_enum_encode<u8>(player_state.progress_state.highest_floor);
    const u8 stage = highest_floor < event->dialogue.stage_limit
        ? highest_floor : event->dialogue.stage_limit;
    if (event->dialogue.stage == stage)
        return;
    event->dialogue.stage = stage;
    event->dialogue.page = KF_DIALOGUE_FIRST_PAGE;
    event->dialogue.page_delay = 0;
}

void map_event_advance_animation_blocking(KfMapEvent *event, u16 target, s16 step)
{
    const auto input_context = kf::host_set_input_context(kf::InputContext::Scripted);
    while (event->animation_phase < target) {
        event->animation_phase += step;
        render_frame(NULL, NULL);
    }
    event->animation_phase = target;
    render_frame(NULL, NULL);
    kf::host_set_input_context(input_context);
}

void map_event_pool_load(KfResourceChunk chunk)
{
    std::array<KfEventPlacementData, KF_MAP_EVENT_CAPACITY> decoded {};
    std::size_t count;
    if (kf_event_placements_decode({chunk.data, chunk.size},
            {KF_MAP_COLUMNS, KF_ASSET_WEAPON - KF_ASSET_MAP_EVENT_FIRST, KF_MAP_TILE_SIZE}, decoded, count) != KF_CODEC_OK)
        kf::host_fail("Invalid map event placements.");
    for (std::size_t i = 0; i < std::size(map_runtime_state.events); ++i) {
        auto &event = map_runtime_state.events[i];
        if (i >= count) {
            event.state = KF_MAP_EVENT_FREE;
        } else {
            const auto *definitions = &decoded[i];
            event.state = kf_enum_decode<KfMapEventState>(definitions->state);
            event.character_id = kf_enum_decode<KfCharacterId>(definitions->character_id);
            event.model_index = definitions->model_index;
            std::copy(std::begin(definitions->dialogue_pages), std::end(definitions->dialogue_pages),
                std::begin(event.dialogue_pages.last_page));
            event.dialogue.stage_limit = definitions->dialogue_stage_limit;
            event.unknown_0c = definitions->unknown_0b;
            event.unknown_0d = definitions->unknown_0c;
            event.behavior = kf_enum_decode<KfMapEventBehavior>(definitions->behavior);
            event.home_x = definitions->cell_x * KF_MAP_TILE_SIZE + definitions->position_x_offset;
            event.reference_position.vx = event.home_x;
            event.home_z = definitions->cell_z * KF_MAP_TILE_SIZE + definitions->position_z_offset;
            event.reference_position.vz = event.home_z;
            event.cell_x = definitions->cell_x;
            event.cell_z = definitions->cell_z;
            event.radius = definitions->radius;
            event.reference_position.vy =
                -(map_floor_height_grid.cells[event.cell_z][event.cell_x] * KF_MAP_HEIGHT_STEP);
            event.rotation.vy = definitions->initial_rotation;
            event.rotation.vz = 0;
            event.rotation.vx = 0;
            event.dialogue.page = KF_DIALOGUE_FIRST_PAGE;
            event.dialogue.stage = KF_DIALOGUE_FIRST_STAGE;
            event.dialogue.page_delay = 0;
            event.animation_clip = KF_ANIMATION_CLIP_FIRST;
            event.animation_phase = 0;
            event.rotation_target = 0;
            event.collision_turn_pending = KF_MAP_EVENT_COLLISION_TURN_NONE;
            collision_adjust_cell_occupancy(event.cell_x, event.cell_z, 1);
        }
    }
}

s32 map_event_distance_to_point(
    const KfMapEvent *event, s32 point_x, s32 point_z, s32 max_distance)
{
    s32 delta_x = event->reference_position.vx - point_x;
    s32 delta_z;
    s32 distance;

    if (delta_x >= -max_distance && delta_x <= max_distance) {
        delta_z = event->reference_position.vz - point_z;
        if (delta_z >= -max_distance && delta_z <= max_distance) {
            distance = fixed_vector2_length(delta_x, delta_z);
            if (distance <= max_distance) {
                return distance;
            }
        }
    }
    return -1;
}

KfMapEvent *map_event_pool_find_target_in_cone(
    const VECTOR *origin,
    s16 facing,
    s32 max_distance,
    s32 angle_tolerance,
    s32 *distance_out)
{
    KfMapEvent *found = NULL;
    s16 best_angle = KF_CONE_SEARCH_INITIAL_ANGLE_ERROR;
    s32 found_distance = 0;
    s32 distance;
    s16 angle;
    s16 folded;

    for (auto &event : map_runtime_state.events) {
        if (event.state != KF_MAP_EVENT_ACTIVE) {
            continue;
        }
        distance = map_event_distance_to_point(&event, origin->vx, origin->vz, max_distance);
        if (distance == -1) {
            continue;
        }
        angle = vector_xz_to_angle(
            event.reference_position.vx - origin->vx, origin->vz - event.reference_position.vz) - facing;
        folded = angle_error_magnitude(angle);
        if (angle_tolerance < folded) {
            continue;
        }
        if (folded < best_angle) {
            best_angle = folded;
            found = &event;
            found_distance = distance;
        }
    }
    *distance_out = found_distance;
    return found;
}

s32 map_event_pool_find_overlap(s32 point_x, s32 point_z, s32 radius_padding)
{
    KfMapEvent *event = map_runtime_state.events.data();
    s16 index = 0;

    do {
        if (event->state == KF_MAP_EVENT_ACTIVE
            && map_event_distance_to_point(
                   event, point_x, point_z, event->radius + radius_padding) != -1) {
            return index;
        }
        index++;
        event++;
    } while (index < KF_MAP_EVENT_CAPACITY);
    return -1;
}
