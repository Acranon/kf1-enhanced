#include <kf/platform/prelude.h>
#include <kf/game/audio.h>
#include <kf/game/game.h>
#include <kf/lib/map.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>
#include <kf/lib/random.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum {
    MAP_DOOR_INTERACTION_LOCAL_Z = 550,
    MAP_GOLD_DROP_SCATTER_RADIUS = 600,
    MAP_GOLD_DROP_INITIAL_VELOCITY_Y = -120,
    MAP_EFFECT_SPAWN_SEQUENCE_MODULUS = 0x10000,
    MAP_SWING_DOOR_YAW_STEP = KF_ANGLE_QUARTER_TURN / kf_enum_encode<u16>(KF_MAP_OBJECT_SWING_OPEN_END),
    MAP_LIFT_DOOR_Y_STEP = 60,
    MAP_DROP_TIP_GRAVITY = 20,
    MAP_DROP_TIP_INITIAL_ANGULAR_VELOCITY = 16,
    MAP_DROP_TIP_ANGULAR_ACCELERATION = 16,
    MAP_DROP_SPIN_Y_STEP = 20,
    MAP_DROP_SPIN_YAW_STEP = 256,
    MAP_DROP_BOUNCE_PITCH_STEP = 160,
    MAP_DROP_BOUNCE_GRAVITY = 30,
    MAP_DROP_BOUNCE_STOP_VELOCITY = 120,
    MAP_EMITTER_PLAYER_RANGE = 30000,
    MAP_EMITTER_COUNTDOWN_RANDOM_SHIFT = 12,
    MAP_EMITTER_COUNTDOWN_BASE = 10,
    MAP_BOSS_EMITTER_COUNTDOWN_BASE = 20,
    MAP_EMITTER_VELOCITY_SHIFT = 10,
    MAP_FIRE_BALL_EMITTER_VELOCITY_SHIFT = 7,
    MAP_FIRE_BALL_EMITTER_Y_OFFSET = -1400,
    MAP_WIND_CUTTER_EMITTER_Y_OFFSET = 600,
    MAP_BOSS_EMITTER_X_OFFSET = 1100,
    MAP_BOSS_EMITTER_Z_OFFSET = 1000,
    MAP_BOSS_EMITTER_Y_OFFSET = -1000,
    MAP_BOSS_EMITTER_SOUND_RANDOM_LIMIT = (kf::random_max + 1) / 8,
    MAP_EFFECT_SWITCH_PHASE_STEP = 128,
    MAP_REVEAL_LIFT = KF_MAP_OBJECT_REVEAL_DEPTH
        + (kf_enum_encode<u16>(KF_MAP_OBJECT_REVEAL_SETTLE_END) - 1) * KF_MAP_OBJECT_REVEAL_SETTLE_STEP,
    MAP_RESTORE_POINT_YAW_STEP = 8
};

static constexpr u32 MAP_EMITTER_VELOCITY_NUMERATOR = 175u;
static constexpr u32 MAP_FIRE_BALL_EMITTER_VELOCITY_NUMERATOR = 25u;
static constexpr u32 MAP_BOSS_EMITTER_VELOCITY_NUMERATOR = 225u;

std::array<SoundRef, KF_GAMEPLAY_SOUND_COUNT> gameplay_sound_refs = {
    SoundRef{9, 0, 72},
    SoundRef{10, 0, 77},
    SoundRef{11, 0, 60},
    SoundRef{9, 0, 80},
    SoundRef{21, 0, 48},
    SoundRef{12, 0, 83},
    SoundRef{19, 4, 103},
    SoundRef{9, 0, 67},
    SoundRef{20, 0, 48},
    SoundRef{8, 0, 47},
    SoundRef{60, 0, 44},
    SoundRef{27, 0, 65},
    SoundRef{15, 0, 71}
};

s32 map_object_pool_find_interaction_from(s32 start_index, s32 point_x, s32 point_z, s32 radius_padding)
{
    KfMapObject *object = &map_object_state.objects[start_index];
    s16 index = start_index;
    KfMapObjectDefinition *definition;
    SVECTOR offset;
    VECTOR point;
    MATRIX matrix;

    for (; index < KF_MAP_OBJECT_CAPACITY; index++, object++) {
        if (object->object_id == KF_OBJECT_NONE) {
            continue;
        }
        definition = &map_object_state.definitions.entries[kf_enum_encode<u8>(object->object_id)];
        if (definition->behavior_type == KF_MAP_OBJECT_OP_HINGED_DOOR) {
            offset = {-KF_MAP_TILE_SIZE, 0, MAP_DOOR_INTERACTION_LOCAL_Z};
            matrix_set_rotation_y(object->rotation.angles.y, &matrix);
            point = kf::matrix_apply_rotation(matrix, offset);
            point.vx += point_x;
            point.vz += point_z;
            if (map_object_distance_to_point(
                    object, point.vx, point.vz, definition->interaction_radius + radius_padding)
                != -1) {
                return index;
            }
        } else if (definition->behavior_type == KF_MAP_OBJECT_OP_HINGED_DOOR_PARTNER) {
            offset = {KF_MAP_TILE_SIZE, 0, MAP_DOOR_INTERACTION_LOCAL_Z};
            matrix_set_rotation_y(object->rotation.angles.y, &matrix);
            point = kf::matrix_apply_rotation(matrix, offset);
            point.vx += point_x;
            point.vz += point_z;
            if (map_object_distance_to_point(
                    object, point.vx, point.vz, definition->interaction_radius + radius_padding)
                != -1) {
                return index;
            }
        } else if (map_object_distance_to_point(
                       object, point_x, point_z, definition->interaction_radius + radius_padding)
                   != -1) {
            return index;
        }
    }
    return -1;
}

void map_object_start_action_if_idle(KfMapObject *object, KfMapObjectOperation action)
{
    if (object->action == KF_MAP_OBJECT_OP_NONE) {
        object->action = action;
        object->action_timer = KF_MAP_OBJECT_PROGRESS_INIT;
    }
}

KfMapObject *map_object_effect_pool_acquire(u16 first_index, u16 count, u16 sequence)
{
    KfMapObject *object = &map_object_state.objects[first_index];
    KfMapObject *oldest = NULL;
    s32 oldest_age = 0;
    s32 age;

    do {
        if (object->object_id == KF_OBJECT_NONE) {
            return object;
        }
        age = sequence - object->link.fields.spawn.sequence;
        if (age < 0) {
            age += MAP_EFFECT_SPAWN_SEQUENCE_MODULUS;
        }
        if (oldest_age < age) {
            oldest = object;
            oldest_age = age;
        }
        object++;
    } while (--count != 0);
    return oldest;
}

void map_object_spawn_drop(KfMapObjectDropSource drop_source, KfObjectId object_id, const VECTOR *position, s32 y_offset)
{
    bool within_drop_range;
    u16 *sequence;
    u16 first_index;
    KfMapObject *object;

    if (drop_source == KF_MAP_OBJECT_DROP_FROM_PLACEMENT) {
        sequence = &map_object_state.placement_drop_sequence;
        first_index = KF_MAP_OBJECT_PLACEMENT_DROP_FIRST;
    } else {
        first_index = KF_MAP_OBJECT_DEFINITION_DROP_FIRST;
        sequence = &map_object_state.definition_drop_sequence;
    }
    object = map_object_effect_pool_acquire(first_index, KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY, *sequence);
    object->link.fields.spawn.sequence = (*sequence)++;
    object->object_id = object_id;
    object->position = {position->vx, y_offset + position->vy, position->vz};
    object->cell_x = object->position.vx / KF_MAP_TILE_SIZE;
    object->cell_z = object->position.vz / KF_MAP_TILE_SIZE;
    object->rotation.angles.z = 0;
    object->rotation.angles.x = 0;
    within_drop_range = object_id < KF_MAP_DROP_BOUNCE_ID_END;
    object->rotation.angles.y = kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
    object->action = KF_MAP_OBJECT_OP_NONE;
    if (object_id < KF_MAP_DROP_TIP_ID_END) {
        map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_FALL_AND_TIP);
    } else if (object_id < KF_MAP_DROP_SPIN_ID_END) {
        map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_FALL_AND_SPIN);
    } else if (within_drop_range) {
        map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_BOUNCE);
    }
    object->link.fields.vertical_velocity = 0;
}

void map_object_spawn_gold_drop(u16 gold_amount, const VECTOR *position, s32 y_offset)
{
    KfMapObject *object;
    u16 *sequence;
    u16 angle;

    sequence = &map_object_state.gold_drop_sequence;
    object = map_object_effect_pool_acquire(KF_MAP_OBJECT_GOLD_DROP_FIRST, KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY, *sequence);
    object->link.fields.spawn.sequence = (*sequence)++;
    object->object_id = KF_ITEM_GOLD_COIN;

    object->link.gold_amount = gold_amount;
    angle = (u32)kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
    object->position = {
        ((kf::angle_sine(angle) * MAP_GOLD_DROP_SCATTER_RADIUS) >> KF_FIXED12_BITS) + position->vx,
        y_offset + position->vy,
        ((kf::angle_cosine(angle) * MAP_GOLD_DROP_SCATTER_RADIUS) >> KF_FIXED12_BITS) + position->vz};
    object->cell_x = object->position.vx / KF_MAP_TILE_SIZE;
    object->cell_z = object->position.vz / KF_MAP_TILE_SIZE;
    object->rotation.angles.z = 0;
    object->rotation.angles.x = 0;
    object->rotation.angles.y = kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
    object->action = KF_MAP_OBJECT_OP_NONE;
    map_object_start_action_if_idle(object, KF_MAP_OBJECT_OP_BOUNCE);
    object->link.fields.vertical_velocity = MAP_GOLD_DROP_INITIAL_VELOCITY_Y;
}

void map_object_pool_trigger_link(u8 link_id)
{
    for (auto &object : map_object_state.objects) {
        switch (object.action) {
        case KF_MAP_OBJECT_OP_RESTORE_POINT:
        case KF_MAP_OBJECT_OP_PROJECTILE_EMITTER:
        case KF_MAP_OBJECT_OP_RELEASE_ORBIT_OR_SHORT_SWING:
        case KF_MAP_OBJECT_OP_RELEASE_LONG_SWING:
            if (object.link.fields.link_id == link_id) {
                object.link.fields.link_id = KF_MAP_LINK_NONE;
            }
            break;
        default:
            if (map_object_state.definitions.entries[kf_enum_encode<u8>(object.object_id)].behavior_type < KF_MAP_OBJECT_OP_LINK_TRIGGER_END
                && !(object.link.fields.link_id < KF_MAP_LINK_REUSABLE_FIRST) && object.link.fields.link_id == link_id) {
                map_object_start_action_if_idle(
                    &object, map_object_state.definitions.entries[kf_enum_encode<u8>(object.object_id)].behavior_type);
            }
            break;
        }
    }
}

void map_object_pool_clear_link(u8 link_id)
{
    KfMapObjectDefinition *definitions = map_object_state.definitions.entries;

    for (auto &object : map_object_state.objects) {
        if ((definitions[kf_enum_encode<u8>(object.object_id)].behavior_type < KF_MAP_OBJECT_OP_LINK_CLEAR_LAST
             || definitions[kf_enum_encode<u8>(object.object_id)].behavior_type == KF_MAP_OBJECT_OP_LINK_CLEAR_LAST)
            && object.link.fields.link_id == link_id) {
            object.link.fields.link_id = KF_MAP_LINK_NONE;
        }
    }
}

void map_object_pool_update(void)
{
    KfMapObject *pair;
    KfEffectRecord *record;
    u8 *counter;
    SVECTOR direction;
    VECTOR point;
    KfMapObjectProgress timer;
    KfMapObjectProgress elapsed;
    s32 floor;
    s32 tilt;

    for (auto &slot : map_object_state.objects) {
        KfMapObject *object = &slot;
        if (object->action == KF_MAP_OBJECT_OP_NONE) {
            continue;
        }
        switch (object->action) {
        default:
            // Unlisted operations have no per-frame action here.
            break;
        case KF_MAP_OBJECT_OP_HINGED_DOOR:
            timer = object->action_timer;
            if (object->link.fields.action_parameter.object_index != KF_MAP_OBJECT_PARAMETER_NONE) {
                pair = &map_object_state.objects[object->link.fields.action_parameter.object_index];
            } else {
                pair = NULL;
            }
            object->action_timer++;
            if (timer < KF_MAP_OBJECT_SWING_OPEN_END) {
                object->rotation.angles.y += MAP_SWING_DOOR_YAW_STEP;
                if (pair != NULL) {
                    pair->rotation.angles.y -= MAP_SWING_DOOR_YAW_STEP;
                }
                if (timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                    if (object->object_id == KF_MAP_OBJECT_HINGED_DOOR) {
                        audio_play_spatial_default_range(
                            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_HINGED_DOOR], &object->position, KF_AUDIO_MAX_VOLUME);
                    } else {
                        audio_play_spatial_default_range(
                            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_STONE_PASSAGE], &object->position, KF_AUDIO_MAX_VOLUME);
                    }
                }
                if (timer == KF_MAP_OBJECT_SWING_OPEN_LAST) {
                    map_object_mark_collision_edge(object, KF_MAP_CELL_FLOOR, object->rotation.angles.y - KF_ANGLE_QUARTER_TURN);
                    object->action_timer = KF_MAP_OBJECT_DOOR_HOLD_FIRST;
                }
            } else if (timer >= KF_MAP_OBJECT_DOOR_CLOSE_FIRST) {
                if (timer >= KF_MAP_OBJECT_SWING_CLOSE_END) {
                    object->action = KF_MAP_OBJECT_OP_NONE;
                    break;
                }
                if (timer == KF_MAP_OBJECT_DOOR_CLOSE_FIRST) {
                    if (map_object_probe_door_closing(object, object->rotation.angles.y - KF_ANGLE_QUARTER_TURN).kind != KfCollisionKind::None) {
                        object->action_timer = KF_MAP_OBJECT_DOOR_CLOSE_FIRST;
                        break;
                    }
                    map_object_mark_collision_edge(object, KF_MAP_CELL_BLOCKED, object->rotation.angles.y - KF_ANGLE_QUARTER_TURN);
                    if (object->object_id == KF_MAP_OBJECT_HINGED_DOOR) {
                        audio_play_spatial_default_range(
                            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_HINGED_DOOR], &object->position, KF_AUDIO_MAX_VOLUME);
                    } else {
                        audio_play_spatial_default_range(
                            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_STONE_PASSAGE], &object->position, KF_AUDIO_MAX_VOLUME);
                    }
                }
                object->rotation.angles.y -= MAP_SWING_DOOR_YAW_STEP;
                if (pair != NULL) {
                    pair->rotation.angles.y += MAP_SWING_DOOR_YAW_STEP;
                }
            }
            break;
        case KF_MAP_OBJECT_OP_LIFT_DOOR:
            elapsed = object->action_timer++;
            if (elapsed < KF_MAP_OBJECT_LIFT_OPEN_END) {
                object->position.vy -= MAP_LIFT_DOOR_Y_STEP;
                if (elapsed == KF_MAP_OBJECT_PROGRESS_INIT) {
                    audio_play_spatial_default_range(
                        &gameplay_sound_refs[KF_GAMEPLAY_SOUND_LIFT_DOOR], &object->position, KF_AUDIO_MAX_VOLUME);
                }
                if (elapsed == KF_MAP_OBJECT_LIFT_OPEN_LAST) {
                    map_object_mark_collision_edge(object, KF_MAP_CELL_FLOOR, object->rotation.angles.y);
                    object->action_timer = KF_MAP_OBJECT_DOOR_HOLD_FIRST;
                }
            } else if (elapsed >= KF_MAP_OBJECT_DOOR_CLOSE_FIRST) {
                if (elapsed >= KF_MAP_OBJECT_LIFT_CLOSE_END) {
                    object->action = KF_MAP_OBJECT_OP_NONE;
                    break;
                }
                if (elapsed == KF_MAP_OBJECT_DOOR_CLOSE_FIRST) {
                    if (map_object_probe_door_closing(object, object->rotation.angles.y).kind != KfCollisionKind::None) {
                        object->action_timer = KF_MAP_OBJECT_DOOR_CLOSE_FIRST;
                        break;
                    }
                    map_object_mark_collision_edge(object, KF_MAP_CELL_BLOCKED, object->rotation.angles.y);
                    audio_play_spatial_default_range(
                        &gameplay_sound_refs[KF_GAMEPLAY_SOUND_LIFT_DOOR], &object->position, KF_AUDIO_MAX_VOLUME);
                }
                object->position.vy += MAP_LIFT_DOOR_Y_STEP;
            }
            break;
        case KF_MAP_OBJECT_OP_FALL_AND_TIP:
            if (object->action_timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                s32 floor_steps = map_floor_height_grid.cells[object->cell_z][object->cell_x];

                object->position.vy += object->link.fields.vertical_velocity;
                object->link.fields.vertical_velocity += MAP_DROP_TIP_GRAVITY;
                if (object->position.vy < -(floor_steps * KF_MAP_HEIGHT_STEP)) {
                    break;
                }
                object->position.vy = -(floor_steps * KF_MAP_HEIGHT_STEP);
                object->link.fields.vertical_velocity = MAP_DROP_TIP_INITIAL_ANGULAR_VELOCITY;
                object->action_timer = KF_MAP_OBJECT_PROGRESS_RUNNING;
            } else {
                object->rotation.angles.x += object->link.fields.vertical_velocity;
                object->link.fields.vertical_velocity += MAP_DROP_TIP_ANGULAR_ACCELERATION;
                if (object->rotation.angles.x >= KF_ANGLE_QUARTER_TURN) {
                    object->rotation.angles.x = KF_ANGLE_QUARTER_TURN;
                    object->action = KF_MAP_OBJECT_OP_NONE;
                }
            }
            break;
        case KF_MAP_OBJECT_OP_FALL_AND_SPIN: {
            s32 floor_steps = map_floor_height_grid.cells[object->cell_z][object->cell_x];

            object->position.vy += MAP_DROP_SPIN_Y_STEP;
            object->rotation.angles.y = (object->rotation.angles.y + MAP_DROP_SPIN_YAW_STEP) & KF_ANGLE_WRAP_MASK;
            if (object->position.vy < -(floor_steps * KF_MAP_HEIGHT_STEP)) {
                break;
            }
            object->position.vy = -(floor_steps * KF_MAP_HEIGHT_STEP);
            object->action_timer = KF_MAP_OBJECT_PROGRESS_RUNNING;
            object->action = KF_MAP_OBJECT_OP_NONE;
            break;
        }
        case KF_MAP_OBJECT_OP_BOUNCE: {
            s32 floor_steps = map_floor_height_grid.cells[object->cell_z][object->cell_x];

            object->position.vy += object->link.fields.vertical_velocity;
            floor = -(floor_steps * KF_MAP_HEIGHT_STEP);
            tilt = object->rotation.angles.x;
            if (object->action_timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                object->rotation.angles.x = (tilt + MAP_DROP_BOUNCE_PITCH_STEP) & KF_ANGLE_WRAP_MASK;
            } else {
                object->rotation.angles.x = (tilt - MAP_DROP_BOUNCE_PITCH_STEP) & KF_ANGLE_WRAP_MASK;
            }
            object->link.fields.vertical_velocity += MAP_DROP_BOUNCE_GRAVITY;
            if (object->position.vy < floor) {
                break;
            }
            object->position.vy = floor;
            if (object->link.fields.vertical_velocity < MAP_DROP_BOUNCE_STOP_VELOCITY) {
                object->rotation.angles.x = 0;
                object->action = KF_MAP_OBJECT_OP_NONE;
                break;
            }
            object->link.fields.vertical_velocity = -(object->link.fields.vertical_velocity >> 1);
            object->action_timer = map_object_toggle_progress(object->action_timer);
            break;
        }
        case KF_MAP_OBJECT_OP_PROJECTILE_EMITTER:
            if (object->link.fields.link_id == KF_MAP_LINK_NONE) {
                break;
            }
            if (object->action_timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                if (player_distance_to_point(
                        object->position.vx, KF_COLLISION_IGNORE_HEIGHT, object->position.vz, MAP_EMITTER_PLAYER_RANGE, 0)
                    == -1) {
                    break;
                }
                switch (object->object_id) {
                default:
                    // Only the listed emitter IDs spawn projectiles.
                    break;
            case KF_MAP_OBJECT_PROJECTILE_EMITTER:
                direction.vy = 0;
                direction.vx = (kf::angle_sine(object->rotation.angles.y) * MAP_EMITTER_VELOCITY_NUMERATOR) >> MAP_EMITTER_VELOCITY_SHIFT;
                direction.vz = (-kf::angle_cosine(object->rotation.angles.y) * MAP_EMITTER_VELOCITY_NUMERATOR) >> MAP_EMITTER_VELOCITY_SHIFT;
                effect_pool_construct(
                    object->link.fields.spawn.effect_id,
                    KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    KF_EFFECT_KIND_MAP_EMITTER_PROJECTILE,
                    &object->position,
                    &direction,
                    KfEffectRotationArguments{&object->rotation.vector});
                object->action_timer = kf_enum_decode<KfMapObjectProgress>((kf::random_next() >> MAP_EMITTER_COUNTDOWN_RANDOM_SHIFT) + MAP_EMITTER_COUNTDOWN_BASE);
                break;
            case KF_MAP_OBJECT_FIRE_BALL_EMITTER:
                direction.vy = 0;
                direction.vx = (kf::angle_sine(object->rotation.angles.y) * MAP_FIRE_BALL_EMITTER_VELOCITY_NUMERATOR) >> MAP_FIRE_BALL_EMITTER_VELOCITY_SHIFT;
                direction.vz = (-kf::angle_cosine(object->rotation.angles.y) * MAP_FIRE_BALL_EMITTER_VELOCITY_NUMERATOR) >> MAP_FIRE_BALL_EMITTER_VELOCITY_SHIFT;
                point.vx = object->position.vx;
                point.vz = object->position.vz;
                point.vy = object->position.vy + MAP_FIRE_BALL_EMITTER_Y_OFFSET;
                effect_pool_construct(
                    object->link.fields.spawn.effect_id,
                    KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    KF_MAGIC_FIRE_BALL,
                    &point,
                    &direction,
                    KfEffectRotationArguments{&object->rotation.vector});
                object->action_timer = kf_enum_decode<KfMapObjectProgress>((kf::random_next() >> MAP_EMITTER_COUNTDOWN_RANDOM_SHIFT) + MAP_EMITTER_COUNTDOWN_BASE);
                break;
            case KF_MAP_OBJECT_WIND_CUTTER_EMITTER:
                direction.vy = 0;
                direction.vx = (kf::angle_sine(object->rotation.angles.y) * MAP_EMITTER_VELOCITY_NUMERATOR) >> MAP_EMITTER_VELOCITY_SHIFT;
                direction.vz = (-kf::angle_cosine(object->rotation.angles.y) * MAP_EMITTER_VELOCITY_NUMERATOR) >> MAP_EMITTER_VELOCITY_SHIFT;
                point.vx = object->position.vx;
                point.vz = object->position.vz;
                point.vy = object->position.vy + MAP_WIND_CUTTER_EMITTER_Y_OFFSET;
                effect_pool_construct(
                    object->link.fields.spawn.effect_id,
                    KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    KF_MAGIC_WIND_CUTTER,
                    &point,
                    &direction,
                    KfEffectRotationSoundArguments{&object->rotation.vector, KF_EFFECT_SOUND_PLAY});
                object->action_timer = kf_enum_decode<KfMapObjectProgress>((kf::random_next() >> MAP_EMITTER_COUNTDOWN_RANDOM_SHIFT) + MAP_EMITTER_COUNTDOWN_BASE);
                break;
            case KF_MAP_OBJECT_BOSS_PROJECTILE_EMITTER:
                if (map_floor_script(KF_FLOOR_5).floor5.boss_encounter_started == KF_MAP_SCRIPT_UNSET) {
                    break;
                }
                direction.vy = 0;
                direction.vx = (kf::angle_sine(object->rotation.angles.y + KF_ANGLE_QUARTER_TURN) * MAP_BOSS_EMITTER_VELOCITY_NUMERATOR) >> MAP_EMITTER_VELOCITY_SHIFT;
                direction.vz = (-kf::angle_cosine(object->rotation.angles.y + KF_ANGLE_QUARTER_TURN) * MAP_BOSS_EMITTER_VELOCITY_NUMERATOR) >> MAP_EMITTER_VELOCITY_SHIFT;
                switch (object->rotation.angles.y) {
                case 0:
                    point.vx = object->position.vx + MAP_BOSS_EMITTER_X_OFFSET;
                    point.vz = object->position.vz + MAP_BOSS_EMITTER_Z_OFFSET;
                    break;
                case KF_ANGLE_HALF_TURN:
                    point.vx = object->position.vx - MAP_BOSS_EMITTER_X_OFFSET;
                    point.vz = object->position.vz - MAP_BOSS_EMITTER_Z_OFFSET;
                    break;
                }
                point.vy = object->position.vy + MAP_BOSS_EMITTER_Y_OFFSET;
                effect_pool_construct(
                    object->link.fields.spawn.effect_id,
                    KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    KF_MAGIC_WIND_CUTTER,
                    &point,
                    &direction,
                    KfEffectRotationSoundArguments{&object->rotation.vector, effect_sound_request(kf::random_next() < MAP_BOSS_EMITTER_SOUND_RANDOM_LIMIT)});
                object->action_timer = kf_enum_decode<KfMapObjectProgress>((kf::random_next() >> MAP_EMITTER_COUNTDOWN_RANDOM_SHIFT) + MAP_BOSS_EMITTER_COUNTDOWN_BASE);
                break;
            }
            } else {
                object->action_timer--;
            }
            break;
        case KF_MAP_OBJECT_OP_RELEASE_ORBIT_OR_SHORT_SWING:
        case KF_MAP_OBJECT_OP_RELEASE_LONG_SWING:
            if (object->link.fields.link_id == KF_MAP_LINK_NONE && object->action_timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                effect_state.records[object->link.fields.action_parameter.effect_index].phase = KF_EFFECT_HAZARD_RELEASE_REQUEST;
                object->action_timer = KF_MAP_OBJECT_PROGRESS_RUNNING;
            }
            break;
        case KF_MAP_OBJECT_OP_EFFECT_SWITCH:
            if (object->action_timer == KF_MAP_OBJECT_SWITCH_DISABLED) {
                break;
            }
            if (object->link.fields.link_id == KF_MAP_LINK_NONE) {
                effect_state.records[object->link.fields.action_parameter.effect_index].visual.animation_phase = (KF_FIXED12_ONE - 1);
                object->action_timer = KF_MAP_OBJECT_SWITCH_DISABLED;
                break;
            }
            if (object->action_timer == KF_MAP_OBJECT_SWITCH_FORWARD) {
                record = &effect_state.records[object->link.fields.action_parameter.effect_index];
                if (record->visual.animation_phase == 0) {
                    audio_play_spatial_default_range(
                        &gameplay_sound_refs[KF_GAMEPLAY_SOUND_EFFECT_SWITCH], &object->position, KF_AUDIO_MAX_VOLUME);
                }
                record->visual.animation_phase += MAP_EFFECT_SWITCH_PHASE_STEP;
                if (record->visual.animation_phase >= KF_FIXED12_ONE) {
                    record->visual.animation_phase = (KF_FIXED12_ONE - 1);
                    map_object_pool_trigger_link(object->link.fields.link_id);
                    if (object->link.fields.link_id >= KF_MAP_LINK_REUSABLE_FIRST) {
                        object->action_timer = KF_MAP_OBJECT_SWITCH_REVERSE;
                    } else {
                        object->link.fields.link_id = KF_MAP_LINK_NONE;
                    }
                }
            } else if (object->action_timer == KF_MAP_OBJECT_SWITCH_REVERSE) {
                record = &effect_state.records[object->link.fields.action_parameter.effect_index];
                if (record->visual.animation_phase == (KF_FIXED12_ONE - 1)) {
                    audio_play_spatial_default_range(
                        &gameplay_sound_refs[KF_GAMEPLAY_SOUND_EFFECT_SWITCH], &object->position, KF_AUDIO_MAX_VOLUME);
                }
                record->visual.animation_phase -= MAP_EFFECT_SWITCH_PHASE_STEP;
                if (record->visual.animation_phase > KF_FIXED12_ONE) {
                    record->visual.animation_phase = 0;
                    object->action_timer = KF_MAP_OBJECT_SWITCH_READY;
                }
            }
            break;
        case KF_MAP_OBJECT_OP_COPY_REGION:
            if (object->link.fields.link_id == KF_MAP_LINK_NONE && object->action_timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                if (object->link.fields.action_parameter.copy_region != KF_MAP_COPY_REGION_NONE) {
                    map_apply_copy_region(object->link.fields.action_parameter.copy_region);
                }
                object->action_timer = KF_MAP_OBJECT_PROGRESS_RUNNING;
            }
            break;
        case KF_MAP_OBJECT_OP_REVEAL_MAP_PIECE:
            if (object->link.fields.link_id != KF_MAP_LINK_NONE) {
                break;
            }
            if (object->action_timer == KF_MAP_OBJECT_PROGRESS_INIT) {
                object->position.vy -= MAP_REVEAL_LIFT;
                object->action_timer = KF_MAP_OBJECT_PROGRESS_RUNNING;
            } else if (object->action_timer < KF_MAP_OBJECT_REVEAL_SETTLE_END) {
                object->position.vy += KF_MAP_OBJECT_REVEAL_SETTLE_STEP;
                object->action_timer++;
            } else if (object->action_timer == KF_MAP_OBJECT_REVEAL_SETTLE_END) {
                if (player_state.progress_state.current_floor == KF_FLOOR_3) {
                    audio_play_spatial_default_range(
                        &gameplay_sound_refs[KF_GAMEPLAY_SOUND_MAP_PIECE_REVEAL], &object->position, KF_AUDIO_MAX_VOLUME);
                    counter = &map_floor_script(KF_FLOOR_3).floor3.revealed_piece_count;
                    if (*counter != KF_MAP_FLOOR3_REQUIRED_REVEALS) {
                        (*counter)++;
                        if (*counter >= KF_MAP_FLOOR3_REQUIRED_REVEALS) {
                            map_apply_copy_region(KF_MAP_COPY_FLOOR3_REVEAL_FIRST);
                            map_apply_copy_region(KF_MAP_COPY_FLOOR3_REVEAL_SECOND);
                            sound_ref_play(audio_playback(), &gameplay_sound_refs[KF_GAMEPLAY_SOUND_STONE_PASSAGE], KF_AUDIO_MAX_VOLUME);
                            *counter = KF_MAP_FLOOR3_REQUIRED_REVEALS;
                        }
                    }
                } else if (player_state.progress_state.current_floor == KF_FLOOR_1) {
                    if (map_floor_script(KF_FLOOR_1).floor1.revival_enabled == KF_MAP_SCRIPT_UNSET) {
                        audio_play_spatial_default_range(
                            &gameplay_sound_refs[KF_GAMEPLAY_SOUND_FLOOR1_REVIVAL], &object->position, KF_AUDIO_MAX_VOLUME);
                        map_floor_script(KF_FLOOR_1).floor1.revival_enabled = KF_MAP_SCRIPT_SET;
                    }
                }
                object->action_timer++;
            }
            break;
        case KF_MAP_OBJECT_OP_RESTORE_POINT:
            if (object->link.fields.link_id == KF_MAP_LINK_NONE) {
                object->object_id = KF_MAP_OBJECT_FILLED_FOUNTAIN;
                object->rotation.angles.y = (object->rotation.angles.y + MAP_RESTORE_POINT_YAW_STEP) & KF_ANGLE_WRAP_MASK;
            }
            break;
        }
    }
}

void map_object_reset_module_state(void)
{
    kf::restore_initial_value<gameplay_sound_refs>();
}
