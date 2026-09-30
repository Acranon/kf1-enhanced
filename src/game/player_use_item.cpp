#include <kf/game/audio.h>
#include <kf/lib/null.h>
#include <kf/lib/bool.h>

#include <kf/game/player.h>
#include <kf/game/game.h>
static constexpr unsigned enemy_image_number_offset = 7;
static constexpr unsigned enemy_image_path_capacity = 14, person_image_path_capacity = 15;
static constexpr unsigned person_image_number_offset = 8;

enum {
    PLAYER_KEY_UNLOCK_VOLUME = 110,
    PLAYER_ILLUSION_STAFF_TIMER_RELOAD = 1000,
    PLAYER_MIRROR_TARGET_DISTANCE = 6000,
    PLAYER_MIRROR_ANGLE_TOLERANCE = KF_ANGLE_FULL_TURN / 12,
    PLAYER_HARP_PROGRESS_PER_UPDATE = 150,
    PLAYER_HARP_CELL_STAGGER = 800,
    PLAYER_HARP_FLOOR2_FIRST_SEGMENT = 0,
    PLAYER_HARP_FLOOR2_SEGMENT_COUNT = 4,
    PLAYER_HARP_FLOOR2_SWEEP_UPDATES = 43,
    PLAYER_HARP_FLOOR2_HOLD_COUNTDOWN = 70,
    PLAYER_HARP_FLOOR3_FIRST_SEGMENT = 4,
    PLAYER_HARP_FLOOR3_SEGMENT_COUNT = 1,
    PLAYER_HARP_FLOOR3_SWEEP_UPDATES = 88,
    PLAYER_HARP_FLOOR3_HOLD_COUNTDOWN = 270
};

char enemy_info_image_path_template[enemy_image_path_capacity] = "ENE0/EI00.TIM";

char person_image_path_template[person_image_path_capacity] = "PRSN/PER00.TIM";

void actor_show_info_image(const KfActor *actor)
{
    render_frame(NULL, NULL);
    render_frame(NULL, NULL);
    enemy_info_image_path_template[3] = '0' + kf_enum_encode<u8>(player_state.progress_state.current_floor);
    enemy_info_image_path_template[enemy_image_number_offset] = '0' + actor->definition_id / 10;
    enemy_info_image_path_template[enemy_image_number_offset + 1] = '0' + actor->definition_id % 10;
    screen_show_image_until_input(enemy_info_image_path_template);
}

void map_event_show_person_image(const KfMapEvent *event)
{
    render_frame(NULL, NULL);
    render_frame(NULL, NULL);
    person_image_path_template[person_image_number_offset] = '0' + kf_enum_encode<u8>(event->character_id) / 10;
    person_image_path_template[person_image_number_offset + 1] = '0' + kf_enum_encode<u8>(event->character_id) % 10;
    screen_show_image_until_input(person_image_path_template);
}

void player_use_item(KfObjectId item_id)
{
    KfMapObject *object;
    KfActor *actor;
    KfMapEvent *event;
    KfEffectRecord *record;
    s32 distance;
    s32 index;
    s16 slot;
    KfBool8 used = false;

    const auto reach = vector_yaw_probe_xz(player_state.camera_position,
        player_state.camera_rotation.vy, MAP_INTERACTION_PROBE_DISTANCE);
    index = 0;
    switch (item_id) {
    case KF_ITEM_KEY_OF_THE_DEAD:
    case KF_ITEM_RAITO_FAMILY_KEY:
    case KF_ITEM_DUNGEON_KEY:
    case KF_ITEM_SORCERER_KEY:
        for (;;) {
            index = map_object_pool_find_interaction_from(
                index, reach.x, reach.z, MAP_INTERACTION_RADIUS_PADDING);
            if (index == -1) {
                break;
            }
            object = &map_object_state.objects[index];
            switch (object->object_id) {
            case KF_MAP_OBJECT_BEVELED_WOODEN_LID:
            case KF_MAP_OBJECT_FLAT_WOODEN_LID:
            case KF_MAP_OBJECT_STONE_CONTAINER_LID:
            case KF_MAP_OBJECT_GRAVESTONE:
            case KF_MAP_OBJECT_LIFTING_GATE:
            case KF_MAP_OBJECT_PORTCULLIS:
            case KF_MAP_OBJECT_HINGED_DOOR:
            case KF_MAP_OBJECT_HINGED_DOOR_PARTNER:
            case KF_MAP_OBJECT_TALL_HINGED_DOOR:
            case KF_MAP_OBJECT_TALL_HINGED_DOOR_PARTNER:
                if (object->link.fields.link_id == KF_MAP_LINK_NONE) {
                    notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
                } else if (object->object_id != KF_MAP_OBJECT_GRAVESTONE
                           || angle_within_tolerance(
                               player_state.camera_rotation.vy, KF_ANGLE_HALF_TURN - object->rotation.angles.y, MAP_DOOR_FACING_TOLERANCE)) {
                    used = true;
                    if (object->link.fields.link_id == kf_enum_encode<u8>(item_id)) {
                        object->link.fields.link_id = KF_MAP_LINK_NONE;
                        sound_ref_play(audio_playback(), &gameplay_sound_refs[KF_GAMEPLAY_SOUND_KEY_UNLOCK], PLAYER_KEY_UNLOCK_VOLUME);
                        if (object->object_id == KF_MAP_OBJECT_GRAVESTONE) {
                            sound_ref_play(audio_playback(), &gameplay_sound_refs[KF_GAMEPLAY_SOUND_STONE_PASSAGE], KF_AUDIO_MAX_VOLUME);
                        }
                    } else {
                        notify_enqueue(KF_NOTIFICATION_KEY_DOES_NOT_FIT);
                    }
                }
                break;
            }
            index++;
        }
        // Retail falls through into the seal-stone search here, which can read an
        // invalid object (upstream sushi-shi #47).
        break;

    case KF_ITEM_DRAGON_CHALICE:
    case KF_ITEM_WATER_SEAL_STONE:
    case KF_ITEM_EARTH_SEAL_STONE:
    case KF_ITEM_FIRE_SEAL_STONE:
    case KF_ITEM_WIND_SEAL_STONE:
        for (;;) {
            index = map_object_pool_find_interaction_from(
                index, reach.x, reach.z, MAP_INTERACTION_RADIUS_PADDING);
            if (index == -1) {
                break;
            }
            object = &map_object_state.objects[index];
            if (object->object_id == kf_enum_decode<KfObjectId>(kf_enum_encode<u8>(item_id))) {
                if (object->link.fields.link_id == KF_MAP_LINK_NONE) {
                    notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
                } else {
                    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(object->object_id)] = 0;
                    used = true;
                    map_object_pool_trigger_link(object->link.fields.link_id);
                    object->link.fields.link_id = KF_MAP_LINK_NONE;
                }
            }
            index++;
        }
        break;
    case KF_ITEM_HARP:
        record = effect_state.records;
        for (slot = KF_EFFECT_CAPACITY - 1; slot != -1; slot--, record++) {
            if (record->type == KF_EFFECT_SLOT_FREE) {
                continue;
            }
            if (record->kind == KF_EFFECT_KIND_FLOOR_DEFORMATION) {
                notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
                return;
            }
        }
        if (player_state.progress_state.current_floor == KF_FLOOR_2) {
            effect_pool_spawn_typed(
                PLAYER_HARP_FLOOR2_FIRST_SEGMENT, PLAYER_HARP_FLOOR2_SEGMENT_COUNT,
                PLAYER_HARP_PROGRESS_PER_UPDATE, PLAYER_HARP_CELL_STAGGER,
                PLAYER_HARP_FLOOR2_SWEEP_UPDATES, PLAYER_HARP_FLOOR2_HOLD_COUNTDOWN);
        } else if (player_state.progress_state.current_floor == KF_FLOOR_3) {
            effect_pool_spawn_typed(
                PLAYER_HARP_FLOOR3_FIRST_SEGMENT, PLAYER_HARP_FLOOR3_SEGMENT_COUNT,
                PLAYER_HARP_PROGRESS_PER_UPDATE, PLAYER_HARP_CELL_STAGGER,
                PLAYER_HARP_FLOOR3_SWEEP_UPDATES, PLAYER_HARP_FLOOR3_HOLD_COUNTDOWN);
        } else {
            break;
        }
        sound_ref_play(audio_playback(), &gameplay_sound_refs[KF_GAMEPLAY_SOUND_HARP], KF_AUDIO_MAX_VOLUME);
        used = true;
        break;
    case KF_ITEM_MEDICINAL_HERB:
    case KF_ITEM_ANTIDOTE_HERB:
    case KF_ITEM_RECOVERY_MEDICINE:
    case KF_ITEM_DRAGON_KING_GRASS_LEAF:
    case KF_ITEM_DRAGON_KING_GRASS_FRUIT:
        used = true;
        break;
    case KF_ITEM_GREEN_DRAGON_STAFF:
        player_warp_to_floor_entry();
        return;
    case KF_ITEM_ILLUSION_STAFF:
        player_state.illusion_staff_timer = PLAYER_ILLUSION_STAFF_TIMER_RELOAD;
        if (item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_ILLUSION_STAFF)] != 0) {
            item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_ILLUSION_STAFF)]--;
        }
        return;
    case KF_ITEM_MIRROR_OF_TRUTH:
        actor = actor_pool_find_target_in_cone(
            &player_state.camera_position,
            player_state.camera_rotation.vy,
            PLAYER_MIRROR_TARGET_DISTANCE,
            PLAYER_MIRROR_ANGLE_TOLERANCE,
            &distance);
        if (actor != NULL) {
            actor_show_info_image(actor);
            return;
        }
        event = map_event_pool_find_target_in_cone(
            &player_state.camera_position,
            player_state.camera_rotation.vy,
            PLAYER_MIRROR_TARGET_DISTANCE,
            PLAYER_MIRROR_ANGLE_TOLERANCE,
            &distance);
        if (event == NULL) {
            break;
        }
        map_event_show_person_image(event);
        return;
    case KF_ITEM_VERDITE:
        player_state.magic_training += KF_PLAYER_TRAINING_POINTS_PER_GAIN;
        used = true;
        player_increment_magic_training();
        break;
    }
    if (!used) {
        notify_enqueue(KF_NOTIFICATION_NOTHING_HAPPENS);
    }
}

void player_use_item_reset_module_state(void)
{
    kf::restore_initial_value<enemy_info_image_path_template>();
    kf::restore_initial_value<person_image_path_template>();
}
