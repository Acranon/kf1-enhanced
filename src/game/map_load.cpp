#include <kf/platform/prelude.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/lib/map.h>
#include <kf/lib/map_data.h>
#include <kf/lib/random.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr std::array<u8, 4> floor5_boss_death_cleanup_definitions = {0, 2, 3, 4};

enum {
    MAP_RESTORE_POSITION_RANDOM_BITS = 15
};

bool map_saved_link_valid(KfMapObjectOperation operation, const KfMapObjectLink &link)
{
    const KfObjectId *items = nullptr;
    switch (operation) {
    case KF_MAP_OBJECT_OP_HINGED_CONTAINER: items = link.hinged_container.item_ids; break;
    case KF_MAP_OBJECT_OP_ITEM_CONTAINER: items = link.item_ids; break;
    case KF_MAP_OBJECT_OP_HINGED_DOOR:
    case KF_MAP_OBJECT_OP_HINGED_DOOR_PARTNER:
        return link.fields.action_parameter.object_index == KF_MAP_OBJECT_PARAMETER_NONE
            || link.fields.action_parameter.object_index < KF_MAP_OBJECT_CAPACITY;
    case KF_MAP_OBJECT_OP_COPY_REGION:
        return link.fields.action_parameter.copy_region == KF_MAP_COPY_REGION_NONE
            || kf_enum_encode<u8>(link.fields.action_parameter.copy_region) < KF_MAP_COPY_REGION_COUNT;
    case KF_MAP_OBJECT_OP_RELEASE_ORBIT_OR_SHORT_SWING:
    case KF_MAP_OBJECT_OP_RELEASE_LONG_SWING:
    case KF_MAP_OBJECT_OP_EFFECT_SWITCH:
        return link.fields.action_parameter.effect_index < KF_EFFECT_CAPACITY;
    default: break;
    }
    if (items)
        for (unsigned i = 0; i < KF_MAP_CONTAINER_ITEM_COUNT; ++i)
            if (items[i] != KF_OBJECT_NONE && kf_enum_encode<u8>(items[i]) >= KF_ITEM_COUNT)
                return false;
    return true;
}

void map_restore_floor_state(void)
{
    u8 *in;
    KfMapEvent *event;
    KfMapObject *object;
    s32 i;
    s32 index;

    const auto floor = kf_enum_encode<u8>(player_state.progress_state.current_floor);
    if (floor < 1 || floor > KF_MAP_SAVED_FLOOR_COUNT)
        kf::host_fail("Invalid restored floor");
    in = map_runtime_state.world_state.floors[floor - 1].records.data();
    if (*in++ == 1) {
        event = map_runtime_state.events.data();
        for (i = 0; i < KF_MAP_EVENT_CAPACITY; i++, event++) {
            event->state = kf_enum_decode<KfMapEventState>(*in++);
            event->dialogue.stage_limit = *in++;
            event->dialogue.stage = *in++;
            event->dialogue.page = *in++;
            const u8 last_page = *in++;
            const auto stage = event->dialogue.stage;
            if (stage > KF_DIALOGUE_STAGE_COUNT)
                kf::host_fail("Invalid restored dialogue stage");
            if (stage)
                event->dialogue_pages.last_page[stage - 1] = last_page;
            event->dialogue.page_delay = *in++;
            event->unknown_0d = *in++;
        }

        i = *in++;
        if (--i != -1) {
            KfActor *actors = actor_state.actors.data();

            do {
                index = *in++;
                actors[index].lifecycle = kf_enum_decode<KfActorLifecycle>(*in++);
            } while (--i != -1);
        }

        const s32 cross_index = floor == 1 ? map_floor1_cross_index() : -1;
        object = &map_object_state.objects[0];
        for (i = 0; i < KF_MAP_OBJECT_CAPACITY; i++, object++) {
            const auto saved_id = kf_enum_decode<KfObjectId>(*in++);
            if (i < KF_MAP_OBJECT_EFFECT_FIRST && saved_id != KF_OBJECT_NONE && saved_id != object->object_id) {
                const bool fountain = object->object_id == KF_MAP_OBJECT_DRY_FOUNTAIN
                    && saved_id == KF_MAP_OBJECT_FILLED_FOUNTAIN;
                const bool cross = i == cross_index && saved_id == KF_MAP_OBJECT_BROKEN_STONE_CROSS
                    && map_floor_script(KF_FLOOR_1).floor1.actor_activation_stage == KF_MAP_TRIGGER_COMPLETE;
                if (object->object_id == KF_OBJECT_NONE || (!fountain && !cross))
                    kf::host_fail("Saved object identity is incompatible with the loaded floor");
            }
            object->object_id = saved_id;
        }

        i = *in++;
        while (--i != -1) {
            index = *in++;
            object = &map_object_state.objects[index];
            KfMapObjectLink restored;
            std::memcpy(&restored, in, sizeof restored);
            in += sizeof restored;
            // Floor construction has already rebuilt effects. Their pool indexes
            // are transient references, not persistent game state.
            switch (object->action) {
            case KF_MAP_OBJECT_OP_RELEASE_ORBIT_OR_SHORT_SWING:
            case KF_MAP_OBJECT_OP_RELEASE_LONG_SWING:
            case KF_MAP_OBJECT_OP_EFFECT_SWITCH:
                restored.fields.action_parameter.effect_index = object->link.fields.action_parameter.effect_index;
                break;
            default: break;
            }
            object->link = restored;
        }

        object = &map_object_state.objects[KF_MAP_OBJECT_GOLD_DROP_FIRST];
        for (i = 0; i < KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY; i++, object++) {
            object->cell_x = *in++;
            object->cell_z = *in++;
            object->position.vx =
                object->cell_x * KF_MAP_TILE_SIZE + ((kf::random_next() * KF_MAP_TILE_SIZE) >> MAP_RESTORE_POSITION_RANDOM_BITS);
            object->position.vz =
                object->cell_z * KF_MAP_TILE_SIZE + ((kf::random_next() * KF_MAP_TILE_SIZE) >> MAP_RESTORE_POSITION_RANDOM_BITS);
            object->position.vy =
                -(map_floor_height_grid.cells[object->cell_z][object->cell_x] * KF_MAP_HEIGHT_STEP);
            object->rotation.angles.z = 0;
            object->rotation.angles.y = 0;
            object->rotation.angles.x = 0;
            object->link.gold_amount = *in++;
            object->link.gold_amount |= *in++ << 8;
            object->link.fields.spawn.sequence = 0;
            object->link.fields.vertical_velocity = 0;
        }

        object = &map_object_state.objects[KF_MAP_OBJECT_DEFINITION_DROP_FIRST];
        for (i = 0; i < 2 * KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY; i++, object++) {
            object->cell_x = *in++;
            object->cell_z = *in++;
            object->position.vx =
                object->cell_x * KF_MAP_TILE_SIZE + ((kf::random_next() * KF_MAP_TILE_SIZE) >> MAP_RESTORE_POSITION_RANDOM_BITS);
            object->position.vz =
                object->cell_z * KF_MAP_TILE_SIZE + ((kf::random_next() * KF_MAP_TILE_SIZE) >> MAP_RESTORE_POSITION_RANDOM_BITS);
            object->position.vy =
                -(map_floor_height_grid.cells[object->cell_z][object->cell_x] * KF_MAP_HEIGHT_STEP);
            if (object->object_id < KF_MAP_DROP_TIP_ID_END) {
                object->rotation.angles.x = KF_ANGLE_QUARTER_TURN;
            } else if (object->object_id < KF_MAP_DROP_SPIN_ID_END) {
                object->rotation.angles.x = 0;
            }
            object->rotation.angles.z = 0;
            object->rotation.angles.y = *in++ << KF_MAP_SAVED_YAW_SHIFT;
            object->link.gold_amount = 0;
            object->link.fields.spawn.sequence = 0;
            object->link.fields.vertical_velocity = 0;
        }
    }

    // Saved IDs can change the interpretation of links even when the optional
    // link record is absent. Validate the resulting objects, not just records.
    for (const auto &restored : map_object_state.objects) {
        if (restored.object_id == KF_OBJECT_NONE)
            continue;
        if (!map_saved_link_valid(restored.action, restored.link) ||
                !map_saved_link_valid(map_object_state.definitions.entries[
                    kf_enum_encode<u8>(restored.object_id)].behavior_type, restored.link))
            kf::host_fail("Saved object link is incompatible with the loaded floor");
    }

    switch (player_state.progress_state.current_floor) {
    case KF_FLOOR_1:
        if (map_floor_script(KF_FLOOR_1).floor1.passage_opened == KF_MAP_SCRIPT_SET) {
            map_apply_copy_region(KF_MAP_COPY_FLOOR1_PASSAGE);
        }
        if (map_floor_script(KF_FLOOR_1).floor1.actor_activation_stage != KF_MAP_TRIGGER_COMPLETE) {
            index = actor_pool_find_at_tile(KF_FLOOR1_TRIGGER_ACTOR_TILE_X, KF_FLOOR1_TRIGGER_ACTOR_TILE_Z);
            if (index != -1) {
                actor_state.actors[index].lifecycle = KF_ACTOR_LIFECYCLE_DISABLED;
            }
        }
        if (map_floor_script(KF_FLOOR_5).floor5.weapon_transformed == KF_MAP_SCRIPT_SET) {
            map_object_pool_clear_link(KF_MAP_LINK_WEAPON_TRANSFORM_DOORS);
        }
        break;
    case KF_FLOOR_2:
        if (map_floor_script(KF_FLOOR_5).floor5.weapon_transformed == KF_MAP_SCRIPT_SET) {
            map_object_pool_clear_link(KF_MAP_LINK_WEAPON_TRANSFORM_DOORS);
        }
        if (player_state.progress_state.highest_floor >= KF_FLOOR_3) {
            map_runtime_state.events[KF_FLOOR2_DEPARTING_EVENT].state = KF_MAP_EVENT_DISABLED;
        }
        break;
    case KF_FLOOR_3:
        if (map_floor_script(KF_FLOOR_5).floor5.weapon_transformed == KF_MAP_SCRIPT_SET) {
            map_object_pool_clear_link(KF_MAP_LINK_WEAPON_TRANSFORM_DOORS);
        }
        if (map_floor_script(KF_FLOOR_3).floor3.revealed_piece_count == KF_MAP_FLOOR3_REQUIRED_REVEALS) {
            map_apply_copy_region(KF_MAP_COPY_FLOOR3_REVEAL_FIRST);
            map_apply_copy_region(KF_MAP_COPY_FLOOR3_REVEAL_SECOND);
        }
        break;
    case KF_FLOOR_4:
        break;
    case KF_FLOOR_5:
        if (map_floor_script(KF_FLOOR_5).floor5.character_arrived == KF_MAP_SCRIPT_SET) {
            map_runtime_state.events[KF_FLOOR5_WEAPON_TRANSFORM_EVENT].state = KF_MAP_EVENT_ACTIVE;
        }
        if (map_floor_script(KF_FLOOR_5).floor5.boss_encounter_started == KF_MAP_SCRIPT_UNSET) {
            actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_MELEE] = KF_ANIMATION_CLIP_NONE;
            actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_EFFECT0] = KF_ANIMATION_CLIP_NONE;
            actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_EFFECT1] = KF_ANIMATION_CLIP_NONE;
            actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_EFFECT2] = KF_ANIMATION_CLIP_NONE;
            actor_state.definitions.entries[KF_FLOOR5_BOSS_DEFINITION].action_animations[KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK] = KF_ANIMATION_CLIP_NONE;
        } else {
            map_apply_copy_region(KF_MAP_COPY_FLOOR5_BOSS_ENCOUNTER);
        }
        if (item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_DRAGON_SWORD)] != 0 || item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_MOONLIGHT_SWORD)] != 0
                || map_floor_script(KF_FLOOR_5).floor5.weapon_transformed == KF_MAP_SCRIPT_SET) {
            map_object_pool_clear_link(KF_MAP_LINK_FLOOR5_SWORD_DOOR);
        }
        if (map_floor_script(KF_FLOOR_5).floor5.boss_defeat != KF_MAP_SCRIPT_UNSET) {
            map_object_pool_trigger_link(KF_MAP_LINK_BOSS_EMITTERS);
            actor_pool_begin_death_by_definition(floor5_boss_death_cleanup_definitions[0]);
            actor_pool_begin_death_by_definition(floor5_boss_death_cleanup_definitions[1]);
            actor_pool_begin_death_by_definition(floor5_boss_death_cleanup_definitions[2]);
            actor_pool_begin_death_by_definition(floor5_boss_death_cleanup_definitions[3]);
        }
        break;
    default:
        break;
    }
}

void map_refresh_dialogue_stages(void)
{
    KfMapEvent *event = map_runtime_state.events.data();
    u16 index = KF_MAP_EVENT_CAPACITY - 1;

    do {
        if (event->state == KF_MAP_EVENT_ACTIVE) {
            map_event_refresh_dialogue_stage(event);
        }
        event++;
    } while (index-- != 0);
}

void map_load_floor(void)
{
    map_resources_load(player_state.progress_state.current_floor,
                       player_state.map_variant);
    map_restore_floor_state();
    map_refresh_dialogue_stages();
    render_prepare_actor_textures(player_state.progress_state.current_floor);
    game_graphics_runtime.hud_model_color_matrix = color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_WHITE)];
}

void map_load_floor_wrapper(void)
{
    map_load_floor();
}
