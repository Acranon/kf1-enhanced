#include <kf/platform/prelude.h>
#include <kf/game/actor.h>
#include <kf/game/audio.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/lib/map_data.h>
#include <kf/lib/random.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <kf/platform/input.h>


enum {
    ACTOR_ACTIVATION_RANGE = 28000,
    ACTOR_NEAR_SPAWN_EXCLUSION_RANGE = 26000,
    ACTOR_ACTIVE_RANGE = 32000,

    ACTOR_GRAVITY = 20,

    ACTOR_DRIFT_AXIS_ACCELERATION = 1,
    ACTOR_DRIFT_AXIS_SPEED_LIMIT = 100,

    ACTOR_DRIFT_YAW_SPEED_LIMIT = 32,

    ACTOR_HOME_AXIS_TOLERANCE = 200,

    ACTOR_WALK_COLLISION_FLAGS = (KF_COLLISION_CELL_BLOCKS_WANDER << KF_COLLISION_CELL_FLAG_SHIFT)
        | KF_COLLISION_SKIP_MAP_EVENTS,
    ACTOR_VELOCITY_COLLISION_FLAGS = ACTOR_WALK_COLLISION_FLAGS
        | KF_COLLISION_SKIP_MAP_OBJECTS
};

enum {
    ACTOR_PURSUIT_DISTANCE_SHIFT = 8,
    ACTOR_MULTI_HIT_SELECTION_WEIGHT = 80,
    ACTOR_RETREAT_RANGE_FACTOR = 2,
    ACTOR_RECENT_PURSUIT_RANGE_FACTOR = 6,
    ACTOR_RETREAT_RANDOM_MIN = 5462,
    ACTOR_WANDER_TO_IDLE_RANDOM_LIMIT = 1092,
    ACTOR_REMAIN_IDLE_RANDOM_MIN = 8193,
    ACTOR_SPAWN_CHANCE_SHIFT = 7,
    ACTOR_WANDER_TURN_RANDOM_LIMIT = 2048,
    ACTOR_PURSUIT_TURN_RANDOM_LIMIT = 4096,
    ACTOR_HOME_TURN_RANDOM_LIMIT = 2048,
    ACTOR_PURSUIT_BACKOFF_RANDOM_SHIFT = 11
};

enum {
    ACTOR_BLOCKED_TURN_STEP = KF_ANGLE_FULL_TURN / 64,
    ACTOR_LONG_DROP_HEIGHT = 600,
    ACTOR_DEEP_DROP_HEIGHT = 3000,
    ACTOR_LONG_DROP_RANDOM_MAX = 16384,
    ACTOR_DEEP_DROP_RANDOM_MAX = 1024,
    ACTOR_STEP_HEIGHT_LIMIT = 1001,
    ACTOR_HIGH_STEP_HEIGHT = 400,
    ACTOR_HIGH_STEP_VELOCITY_Y = -300,
    ACTOR_LOW_STEP_VELOCITY_Y = -120,
    ACTOR_CHARGE_LENGTH_SHIFT = 2,
    ACTOR_CHARGE_COMPONENT_DIVISOR = 16,
    ACTOR_JUMP_HEIGHT_THRESHOLD = -5000,
    ACTOR_JUMP_SHORT_VELOCITY_Y = -220,
    ACTOR_JUMP_LONG_VELOCITY_Y = -300,
    ACTOR_JUMP_SHORT_ANIMATION_STEP = 140,
    ACTOR_JUMP_LONG_ANIMATION_STEP = 110,
    ACTOR_JUMP_BOUNCE_VELOCITY_Y = -120,
    ACTOR_JUMP_CEILING_VELOCITY_Y = 100,
    ACTOR_JUMP_CONTACT_STRIKING_DAMAGE = 15
};

enum {
    ACTOR_PAIRED_EFFECT_X_OFFSET = 1500,
    ACTOR_EFFECT_AIM_RANGE = 50000,
    ACTOR_PROPAGATING_EFFECT_SPEED = 250,
    ACTOR_LIGHTNING_FALLBACK_PITCH = -32,
    ACTOR_EFFECT_FALLBACK_MOVE_COUNT = 20,
    ACTOR_LIGHTNING_TARGET_Y_OFFSET = 1000,
    ACTOR_SCATTER_TARGET_STANDOFF = 4500,
    ACTOR_SPAWNER_TARGET_STANDOFF = 2000,
    ACTOR_SCATTER_GENERATIONS = 3,
    ACTOR_SCATTER_INITIAL_SCALE = 3000
};

enum {
    ACTOR_MELEE_CONTACT_PHASE = 2700,
    ACTOR_SPECIAL_CONTACT_PHASE = 3000,
    ACTOR_JUMP_CONTACT_PHASE = 3400,
    ACTOR_HOME_ANIMATION_RESET_PHASE = 1200,
    ACTOR_MULTI_HIT_FORWARD_PHASE1 = 2240,
    ACTOR_MULTI_HIT_FORWARD_PHASE2 = 2688,
    ACTOR_MULTI_HIT_FORWARD_PHASE3 = 3200,
    ACTOR_MULTI_HIT_FORWARD_PHASE4 = 3584,
    ACTOR_MULTI_HIT_NEGATIVE_YAW_PHASE = 3500,
    ACTOR_MULTI_HIT_POSITIVE_YAW_PHASE = 3800,
    ACTOR_DEATH_DROP_PHASE = KF_ACTOR_ANIMATION_PHASE_PERIOD / 2,
    ACTOR_GOLD_RANDOM_SHIFT = 15,
    ACTOR_DROP_CHANCE_RANDOM_SHIFT = 7,
    ACTOR_BOSS_DEATH_SOUND_PHASE1 = 500,
    ACTOR_BOSS_DEATH_SOUND_PHASE2 = 1000,
    ACTOR_BOSS_DEATH_SOUND_PHASE3 = 1500,
    ACTOR_BOSS_DEATH_EFFECT_PERIOD = 2,
    ACTOR_BOSS_DEATH_SOUND_PERIOD = 4,
    ACTOR_BOSS_DEATH_SCATTER_XZ_MASK = 8191,
    ACTOR_BOSS_DEATH_SCATTER_XZ_BIAS = 4096,
    ACTOR_BOSS_DEATH_SCATTER_Y_MASK = 4095,
    ACTOR_BOSS_DEATH_LOOP_VOLUME = 100
};

static inline s32 actor_player_distance(const KfActor *actor, s32 maximum)
{
    return actor_distance_to_point(actor,
        actor_state.player_position.vx, KF_COLLISION_IGNORE_HEIGHT,
        actor_state.player_position.vz, maximum, 0, 0);
}

static KfActorAction actor_choose_movement_action(const KfActor *actor,
    const KfActorDefinition *definition, KfActorAction action,
    s32 player_distance, s32 awareness, s32 near_range)
{
    bool recently_active;

    recently_active = action == KF_ACTOR_ACTION_RETREAT
        || action == KF_ACTOR_ACTION_MELEE_ATTACK
        || action == KF_ACTOR_ACTION_JUMP_ATTACK
        || action == KF_ACTOR_ACTION_SPECIAL_ATTACK
        || action == KF_ACTOR_ACTION_EFFECT0
        || action == KF_ACTOR_ACTION_EFFECT1
        || action == KF_ACTOR_ACTION_EFFECT2;
    if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE] != KF_ANIMATION_CLIP_NONE) {
        if (recently_active || !(awareness < player_distance)) {
            if (!(awareness * ACTOR_RETREAT_RANGE_FACTOR < player_distance) && !(kf::random_next() < ACTOR_RETREAT_RANDOM_MIN)) {
                return KF_ACTOR_ACTION_RETREAT;
            }
        }
    }
    if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE] != KF_ANIMATION_CLIP_NONE) {
        if (recently_active || action == KF_ACTOR_ACTION_HIT_REACTION || action == KF_ACTOR_ACTION_PURSUE) {
            near_range = near_range * ACTOR_RECENT_PURSUIT_RANGE_FACTOR;
        }
        if (!(near_range < player_distance)) {
            return KF_ACTOR_ACTION_PURSUE;
        }
    }
    if (definition->action_animations[KF_ACTOR_ANIM_SLOT_IDLE] != KF_ANIMATION_CLIP_NONE) {
        if (action != KF_ACTOR_ACTION_IDLE) {
            if (action == KF_ACTOR_ACTION_WANDER) {
                if (kf::random_next() < ACTOR_WANDER_TO_IDLE_RANDOM_LIMIT) {
                    return KF_ACTOR_ACTION_IDLE;
                }
            }
        } else {
            if (!(kf::random_next() < ACTOR_REMAIN_IDLE_RANDOM_MIN)) {
                return KF_ACTOR_ACTION_IDLE;
            }
        }
        if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE] == KF_ANIMATION_CLIP_NONE) {
            return KF_ACTOR_ACTION_IDLE;
        }
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE] == KF_ANIMATION_CLIP_NONE) {
        return KF_ACTOR_ACTION_NONE;
    }
    return actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND
        ? KF_ACTOR_ACTION_RETURN_HOME : KF_ACTOR_ACTION_WANDER;
}

void actor_select_next_action(s32 player_distance)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    KfActorAction action = actor->action;
    KfActorAction chosen = KF_ACTOR_ACTION_NONE;
    s32 awareness;
    s32 near_range;

    if (actor->action_progress == KF_ACTOR_PROGRESS_LOCKED) {
        return;
    }
    if (actor->action_progress != KF_ACTOR_PROGRESS_COMPLETE) {
        if (action == KF_ACTOR_ACTION_POST_DEATH) {
            return;
        }
        if (action == KF_ACTOR_ACTION_HIT_REACTION || action == KF_ACTOR_ACTION_DYING) {
            return;
        }
        if (action == KF_ACTOR_ACTION_EXIT_BLOCKED_PLACEMENT) {
            return;
        }
    }
    near_range = definition->pursuit_distance_scale << ACTOR_PURSUIT_DISTANCE_SHIFT;
    awareness = definition->awareness_distance;
    if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK] != KF_ANIMATION_CLIP_NONE
        && actor_try_select_multi_hit_action(KF_ACTOR_ACTION_MULTI_HIT_ATTACK, player_distance, ACTOR_MULTI_HIT_SELECTION_WEIGHT)
            != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_MULTI_HIT_ATTACK;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_JUMP_ATTACK] != KF_ANIMATION_CLIP_NONE
               && actor_try_select_ground_action(
                      KF_ACTOR_ACTION_JUMP_ATTACK, player_distance, definition->special_attack_chance)
                   != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_JUMP_ATTACK;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_SPECIAL_ATTACK] != KF_ANIMATION_CLIP_NONE
               && actor_try_select_action_distance_facing(
                      KF_ACTOR_ACTION_SPECIAL_ATTACK,
                      player_distance,
                      definition->special_attack_chance,
                      definition->special_attack_range)
                   != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_SPECIAL_ATTACK;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_EFFECT0] != KF_ANIMATION_CLIP_NONE
               && actor_try_select_profiled_action(
                      KF_ACTOR_ACTION_EFFECT0,
                      player_distance,
                      definition->action_parameters.effect_codes[0],
                      definition->action_parameters.effect_chances[0])
                   != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_EFFECT0;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_EFFECT1] != KF_ANIMATION_CLIP_NONE
               && actor_try_select_profiled_action(
                      KF_ACTOR_ACTION_EFFECT1,
                      player_distance,
                      definition->action_parameters.effect_codes[1],
                      definition->action_parameters.effect_chances[1])
                   != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_EFFECT1;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_EFFECT2] != KF_ANIMATION_CLIP_NONE
               && actor_try_select_profiled_action(
                      KF_ACTOR_ACTION_EFFECT2,
                      player_distance,
                      definition->action_parameters.effect_codes[2],
                      definition->action_parameters.effect_chances[2])
                   != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_EFFECT2;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MELEE] != KF_ANIMATION_CLIP_NONE
               && actor_try_select_action_distance_facing(
                      KF_ACTOR_ACTION_MELEE_ATTACK,
                      player_distance,
                      definition->melee_attack_chance,
                      definition->awareness_distance)
                   != KF_ACTOR_ACTION_NONE) {
        chosen = KF_ACTOR_ACTION_MELEE_ATTACK;
    } else if (definition->action_animations[KF_ACTOR_ANIM_SLOT_DRIFT] != KF_ANIMATION_CLIP_NONE) {
        chosen = KF_ACTOR_ACTION_DRIFT;
    } else {
        chosen = actor_choose_movement_action(actor, definition, action,
            player_distance, awareness, near_range);
    }
    if (chosen != actor->action || actor->action_progress == KF_ACTOR_PROGRESS_COMPLETE) {
        actor_set_action(actor, chosen);
    }
}

static bool actor_try_activate_nearby(KfActor *actor, const KfActorDefinition *definition,
    KfActorSlotState spawn_policy, s32 distance)
{
    if (distance < ACTOR_NEAR_SPAWN_EXCLUSION_RANGE && player_state.allow_near_actor_spawn == KF_ACTOR_NEAR_SPAWN_FORBIDDEN) {
        return false;
    }
    if ((actor->spawn_chance << ACTOR_SPAWN_CHANCE_SHIFT) > kf::random_next()
        || spawn_policy == KF_ACTOR_SLOT_PERSISTENT || spawn_policy == KF_ACTOR_SLOT_HOMEBOUND) {
        if (actor_pool_find_overlap(
                map_placement_axis_position(actor->tile_x, actor->local_x),
                KF_COLLISION_IGNORE_HEIGHT,
                map_placement_axis_position(actor->tile_z, actor->local_z),
                definition->collision_radius,
                0)
            != -1) {
            return false;
        }
        actor_initialize_current();
        actor_select_next_action(distance);
        return true;
    }
    return false;
}

void actor_update_awareness(void)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    KfActorSlotState slot_state = actor->slot_state;
    s32 distance;
    KfActorSlotState spawn_policy;

    switch (actor->lifecycle) {
    case KF_ACTOR_LIFECYCLE_DISABLED:
        break;
    case KF_ACTOR_LIFECYCLE_DORMANT:
        distance = actor_player_distance(actor, ACTOR_ACTIVATION_RANGE);
        if (distance == -1) {
            return;
        }
        spawn_policy = slot_state;
        if (spawn_policy == KF_ACTOR_SLOT_RESPAWNING) {
            if ((actor->spawn_chance << ACTOR_SPAWN_CHANCE_SHIFT) > kf::random_next()) {
                if (actor_pool_find_overlap(
                        map_placement_axis_position(actor->tile_x, actor->local_x),
                        KF_COLLISION_IGNORE_HEIGHT,
                        map_placement_axis_position(actor->tile_z, actor->local_z),
                        definition->collision_radius,
                        0)
                    == -1) {
                    actor_initialize_current();
                    if (definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE] != KF_ANIMATION_CLIP_NONE) {
                        actor_set_action(actor, KF_ACTOR_ACTION_EXIT_BLOCKED_PLACEMENT);
                    } else {
                        actor_select_next_action(distance);
                    }
                }
            }
        } else {
            if (actor_try_activate_nearby(actor, definition, spawn_policy, distance)) {
                return;
            }
            actor->lifecycle = KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT;
        }
        break;
    case KF_ACTOR_LIFECYCLE_ACTIVE:
        distance = actor_player_distance(actor, ACTOR_ACTIVE_RANGE);
        if (distance == -1) {
            actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
        } else {
            actor_select_next_action(distance);
        }
        break;
    case KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT:
        if (actor_player_distance(actor, ACTOR_ACTIVE_RANGE)
            == -1) {
            actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
        }
        break;
    }
}

static KfActorMoveResult actor_handle_blocked_movement(
    KfActor *actor, const KfActorDefinition *definition,
    const KfVecXZs *delta, const VECTOR &target, KfActorCollisionPolicy collision_policy)
{
    if (collision_policy == KF_ACTOR_COLLISION_STEER) {
        if (actor->collision_state != KF_ACTOR_COLLISION_BLOCKED) {
            if (collision_query_world(
                    actor->position.vx,
                    target.vy,
                    target.vz,
                    definition->collision_radius,
                    definition->collision_height,
                    ACTOR_WALK_COLLISION_FLAGS).kind
                == KfCollisionKind::None) {
                if (delta->z >= 0) {
                    actor->movement_yaw = KF_ANGLE_HALF_TURN;
                } else {
                    actor->movement_yaw = 0;
                }
                actor->position.vz = target.vz;
                actor->cell_z = target.vz / KF_MAP_TILE_SIZE;
                actor->collision_state = KF_ACTOR_COLLISION_SLIDING;
            } else if (collision_query_world(
                           target.vx,
                           target.vy,
                           actor->position.vz,
                           definition->collision_radius,
                           definition->collision_height,
                           ACTOR_WALK_COLLISION_FLAGS).kind
                       == KfCollisionKind::None) {
                if (delta->x < 0) {
                    actor->movement_yaw = KF_ANGLE_QUARTER_TURN;
                } else {
                    actor->movement_yaw = KF_ANGLE_THREE_QUARTER_TURN;
                }
                actor->position.vx = target.vx;
                actor->cell_x = target.vx / KF_MAP_TILE_SIZE;
                actor->collision_state = KF_ACTOR_COLLISION_SLIDING;
            } else {
                actor->movement_yaw = (actor->movement_yaw + KF_ANGLE_HALF_TURN) & KF_ANGLE_WRAP_MASK;
                actor->collision_state = KF_ACTOR_COLLISION_BLOCKED;
            }
        } else if (actor->movement_yaw == actor->rotation.angles.y) {
            actor->movement_yaw = (actor->movement_yaw + ACTOR_BLOCKED_TURN_STEP) & KF_ANGLE_WRAP_MASK;
        }
    }
    return KF_ACTOR_MOVE_BLOCKED;
}

KfActorMoveResult actor_move_xz_with_collision(const struct KfVecXZs *delta, KfActorCollisionPolicy collision_policy)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    VECTOR target;
    KfCollisionResult result;
    s32 drop;
    s32 threshold;

    target = actor->position;
    vector3i_add_xz(&target, delta);
    result = collision_query_world(
        target.vx,
        target.vy,
        target.vz,
        definition->collision_radius,
        definition->collision_height,
        ACTOR_WALK_COLLISION_FLAGS);
    if (result.kind != KfCollisionKind::None
        && (result.kind != KfCollisionKind::BelowFloor || actor->vertical_state == KF_ACTOR_VERTICAL_LONG_DROP)) {
        return actor_handle_blocked_movement(actor, definition, delta, target, collision_policy);
    }
    drop = target.vy - map_floor_height_at_position(&target);
    if (drop < 0) {
        if (drop < -ACTOR_LONG_DROP_HEIGHT) {
            threshold = ACTOR_LONG_DROP_RANDOM_MAX;
            if (drop < -ACTOR_DEEP_DROP_HEIGHT) {
                threshold = ACTOR_DEEP_DROP_RANDOM_MAX;
            }
            if (threshold < kf::random_next()) {
                return actor_handle_blocked_movement(actor, definition, delta, target, collision_policy);
            }
            if (actor->vertical_state == KF_ACTOR_VERTICAL_NONE) {
                actor->vertical_state = KF_ACTOR_VERTICAL_LONG_DROP;
                actor->vertical_velocity = 0;
            }
        } else if (actor->vertical_state == KF_ACTOR_VERTICAL_NONE) {
            actor->vertical_state = KF_ACTOR_VERTICAL_FALL;
            actor->vertical_velocity = 0;
        }
    } else if (drop > 0) {
        if (!(drop < ACTOR_STEP_HEIGHT_LIMIT)) {
            return actor_handle_blocked_movement(actor, definition, delta, target, collision_policy);
        }
        if (drop >= ACTOR_HIGH_STEP_HEIGHT) {
            if (actor->vertical_state == KF_ACTOR_VERTICAL_NONE) {
                actor->vertical_state = KF_ACTOR_VERTICAL_STEP_UP;
                actor->vertical_velocity = ACTOR_HIGH_STEP_VELOCITY_Y;
            }
        } else if (actor->vertical_state == KF_ACTOR_VERTICAL_NONE) {
            actor->vertical_state = KF_ACTOR_VERTICAL_STEP_UP;
            actor->vertical_velocity = ACTOR_LOW_STEP_VELOCITY_Y;
        }
    }
    actor->collision_state = KF_ACTOR_COLLISION_CLEAR;
    actor->position.vx = target.vx;
    actor->position.vz = target.vz;
    actor->cell_x = target.vx / KF_MAP_TILE_SIZE;
    actor->cell_z = target.vz / KF_MAP_TILE_SIZE;
    return KF_ACTOR_MOVE_SUCCEEDED;
}

KfActorMoveResult actor_move_along_heading(KfActorMoveDirection direction, KfActorCollisionPolicy collision_policy)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    struct KfVecXZs delta;
    u32 rate;

    if (actor->collision_state == KF_ACTOR_COLLISION_SLIDING) {
        rate = definition->turn_rate;
        actor->rotation.angles.y = angle_approach(actor->rotation.angles.y, actor->movement_yaw, (rate + rate + rate) >> 1);
    } else {
        actor->rotation.angles.y = angle_approach(actor->rotation.angles.y, actor->movement_yaw, definition->turn_rate);
    }
    angle_to_forward_xz(actor->rotation.angles.y, &delta);
    vector2s_scale_shift11(definition->move_speed, &delta);
    if (kf_enum_encode<s32>(direction) < 0) {
        delta.x = -delta.x;
        delta.z = -delta.z;
    }
    return actor_move_xz_with_collision(&delta, collision_policy);
}

static s32 actor_effect_travel_steps(s32 distance, s32 speed)
{
    return distance <= speed ? 1 : distance / speed;
}

void actor_spawn_action_effect(KfActorEffectCode effect_code, KfActorEffectSlot effect_slot)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    SVECTOR direction;
    SVECTOR offset;
    KfRotation effect_rotation;
    VECTOR position;
    MATRIX matrix;
    KfRotation burst_rotation;
    s32 repeat;
    s32 i;
    s16 facing;
    s32 distance;
    s32 speed;

    repeat = 1;
    if ((effect_code & KF_ACTOR_EFFECT_PAIRED) != KF_ACTOR_EFFECT_CODE_NONE) {
        repeat = 2;
    }
    effect_code &= KF_ACTOR_EFFECT_KIND_MASK;
    for (i = 0; i < repeat; i++) {
        switch (actor_effect_kind_from_payload(effect_code)) {
        default:
            // Other effect kinds have no actor-launch recipe here.
            break;
        case KF_MAGIC_FIRE_BALL:
        case KF_MAGIC_WIND_CUTTER:
        case KF_MAGIC_LIGHT_NEEDLE:
        case KF_EFFECT_KIND_ACTOR_SPAWNER:
        case KF_EFFECT_KIND_SCATTER_PROJECTILE:
        case KF_EFFECT_KIND_DARKNESS_PROJECTILE:
        case KF_EFFECT_KIND_CURSE_PROJECTILE:
        case KF_EFFECT_KIND_EMERGING_PROJECTILE:
        case KF_EFFECT_KIND_PHYSICAL_PROJECTILE:
        case KF_EFFECT_KIND_LIGHTNING_BOLT_ALTERNATE:
        case KF_EFFECT_KIND_HOMING_PROJECTILE_ALTERNATE:
            offset = {
                definition->attachment_offsets[kf_enum_encode<s32>(effect_slot)].x,
                definition->attachment_offsets[kf_enum_encode<s32>(effect_slot)].y,
                definition->attachment_offsets[kf_enum_encode<s32>(effect_slot)].z};
            if (repeat == 2) {
                if (i == 0) {
                    offset.vx = offset.vx + ACTOR_PAIRED_EFFECT_X_OFFSET;
                } else {
                    offset.vx = offset.vx - ACTOR_PAIRED_EFFECT_X_OFFSET;
                }
            }
            effect_rotation.angles.x = actor->rotation.angles.x;
            effect_rotation.angles.y = -actor->rotation.angles.y & KF_ANGLE_WRAP_MASK;
            effect_rotation.angles.z = actor->rotation.angles.z;
            matrix_set_rotation_yxz(&effect_rotation.angles, &matrix);
            position = kf::matrix_apply_rotation(matrix, offset);
            position += actor->position;
            facing = (KF_ANGLE_HALF_TURN - actor->rotation.angles.y) & KF_ANGLE_WRAP_MASK;
            distance = player_distance_to_point_in_cone(
                &position, facing, ACTOR_EFFECT_AIM_RANGE, KF_ACTOR_AIM_TOLERANCE);
            if (distance == -1) {
                effect_rotation.angles.x = 0;
                if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_LIGHTNING_BOLT_ALTERNATE) {
                    speed = KF_EFFECT_LIGHTNING_SPEED;
                    effect_rotation.angles.x = ACTOR_LIGHTNING_FALLBACK_PITCH;
                    distance = ACTOR_EFFECT_FALLBACK_MOVE_COUNT;
                } else if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_ACTOR_SPAWNER
                           || actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_SCATTER_PROJECTILE) {
                    speed = ACTOR_PROPAGATING_EFFECT_SPEED;
                    distance = ACTOR_EFFECT_FALLBACK_MOVE_COUNT;
                } else {
                    speed = KF_EFFECT_PROJECTILE_DEFAULT_SPEED;
                }
                effect_rotation.angles.y = facing;
            } else {
                effect_rotation.angles.y = vector_xz_to_angle(
                    actor_state.player_position.vx - position.vx,
                    position.vz - actor_state.player_position.vz);
                if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_LIGHTNING_BOLT_ALTERNATE) {
                    speed = KF_EFFECT_LIGHTNING_SPEED;
                    effect_rotation.angles.x = vector_xz_to_angle(
                        position.vy - (actor_state.player_position.vy - ACTOR_LIGHTNING_TARGET_Y_OFFSET), -distance);
                    distance = distance / speed;
                } else {
                    effect_rotation.angles.x = vector_xz_to_angle(
                        position.vy - actor_state.player_position.vy, -distance);
                    if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_SCATTER_PROJECTILE) {
                        speed = ACTOR_PROPAGATING_EFFECT_SPEED;
                        distance -= ACTOR_SCATTER_TARGET_STANDOFF;

                        distance = actor_effect_travel_steps(distance, speed);
                    } else if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_ACTOR_SPAWNER) {
                        speed = ACTOR_PROPAGATING_EFFECT_SPEED;
                        distance -= ACTOR_SPAWNER_TARGET_STANDOFF;
                        distance = actor_effect_travel_steps(distance, speed);
                    } else {
                        speed = KF_EFFECT_PROJECTILE_DEFAULT_SPEED;
                    }
                }
            }
            effect_rotation.angles.z = 0;
            if (actor_effect_kind_from_payload(effect_code) == KF_MAGIC_WIND_CUTTER) {
                speed = KF_EFFECT_WIND_CUTTER_SPEED;
            }
            pitch_yaw_to_forward_vector(&effect_rotation.angles, &direction);
            vector3s_scale_shift12(speed, &direction);
            if (actor_effect_kind_from_payload(effect_code) == KF_MAGIC_LIGHT_NEEDLE || actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_PHYSICAL_PROJECTILE) {
                effect_pool_construct(
                    definition->effect_owner_id, KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    actor_effect_kind_from_payload(effect_code), &position, &direction, KfEffectRotationSoundArguments{&effect_rotation.vector, KF_EFFECT_SOUND_PLAY});
            } else if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_HOMING_PROJECTILE_ALTERNATE) {
                burst_rotation.angles.x = actor->rotation.angles.x;
                burst_rotation.angles.y = facing;
                burst_rotation.angles.z = actor->rotation.angles.z;
                effect_pool_construct(
                    definition->effect_owner_id, KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_HOMING_PROJECTILE_ALTERNATE,
                    &position, &direction, KfEffectHomingArguments{&burst_rotation.vector, KF_EFFECT_HOMING_PLAYER, KF_EFFECT_SOUND_PLAY});
            } else if (actor_effect_kind_from_payload(effect_code) == KF_EFFECT_KIND_SCATTER_PROJECTILE) {
                effect_pool_construct(
                    definition->effect_owner_id, KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    KF_EFFECT_KIND_SCATTER_PROJECTILE, &position, &direction, KfEffectScatterArguments{ACTOR_SCATTER_GENERATIONS, distance, ACTOR_SCATTER_INITIAL_SCALE});
            } else {
                effect_pool_construct(
                    definition->effect_owner_id, KF_EFFECT_CLASS_20 | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER,
                    actor_effect_kind_from_payload(effect_code), &position, &direction, KfEffectDurationSoundArguments{distance, KF_EFFECT_SOUND_PLAY});
            }
            break;
        }
    }
}

void actor_prepare_charge_toward_player(void)
{
    KfActor *actor = actor_state.current;
    struct KfVecXZs delta;
    s32 length;

    actor->movement_yaw = actor_bearing_to_player(actor);
    if (angle_within_tolerance(actor->rotation.angles.y, actor->movement_yaw, KF_ACTOR_AIM_TOLERANCE) == 0) {
        actor->movement_yaw = actor->rotation.angles.y;
    }
    length = fixed_vector2_length(
        actor_state.player_position.vx - actor->position.vx,
        actor_state.player_position.vz - actor->position.vz);
    angle_to_forward_xz(actor->movement_yaw, &delta);
    vector2s_scale_shift11(length >> ACTOR_CHARGE_LENGTH_SHIFT, &delta);
    actor->movement_x = delta.x / ACTOR_CHARGE_COMPONENT_DIVISOR;
    actor->movement_z = delta.z / ACTOR_CHARGE_COMPONENT_DIVISOR;
}

void actor_apply_horizontal_movement(void)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition;
    VECTOR target;
    KfCollisionResult result;

    target.vx = actor->movement_x + actor->position.vx;
    target.vz = actor->movement_z + actor->position.vz;
    definition = actor_state.current_definition;
    result = collision_query_world(
        target.vx,
        actor->position.vy,
        target.vz,
        definition->collision_radius,
        definition->collision_height,
        ACTOR_VELOCITY_COLLISION_FLAGS);
    if (result.kind != KfCollisionKind::None) {
        if (result.kind != KfCollisionKind::Player) {
            actor->movement_x = -actor->movement_x >> 1;
            actor->movement_z = -actor->movement_z >> 1;
        } else {
            actor->movement_z = 0;
            actor->movement_x = 0;
        }
    } else {
        actor->position.vx = target.vx;
        actor->position.vz = target.vz;
    }
    actor->cell_x = actor->position.vx / KF_MAP_TILE_SIZE;
    actor->cell_z = actor->position.vz / KF_MAP_TILE_SIZE;
}

void actor_update_effect_action(KfActorEffectSlot effect_slot)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    s32 index = kf_enum_encode<s32>(effect_slot) + KF_ACTOR_ANIM_SLOT_EFFECT0;

    if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
        actor->action_progress = KF_ACTOR_PROGRESS_LOCKED;
        actor->animation_clip = definition->action_animations[index];
        actor->animation_phase = 0;
    }
    actor_advance_animation_clamped(actor, definition->action_animation_steps[index]);
    if (actor_animation_crossed_phase(actor, definition->action_animation_phases[index])) {
        actor_spawn_action_effect(definition->action_parameters.effect_codes[kf_enum_encode<s32>(effect_slot)], effect_slot);
    }
    if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
        actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
        actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
    }
}

void actor_apply_random_movement(s16 step, s16 limit)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    VECTOR target;
    KfCollisionResult result;

    if (kf::random_next() < (kf::random_max + 1) / 2) {
        actor->movement_x += step;
        actor->movement_x = std::min<s32>(actor->movement_x, limit);
    } else {
        actor->movement_x -= step;
        actor->movement_x = std::max<s32>(actor->movement_x, -limit);
    }
    if (kf::random_next() < (kf::random_max + 1) / 2) {
        actor->movement_z += step;
        actor->movement_z = std::min<s32>(actor->movement_z, limit);
    } else {
        actor->movement_z -= step;
        actor->movement_z = std::max<s32>(actor->movement_z, -limit);
    }
    if (kf::random_next() < (kf::random_max + 1) / 2) {
        actor->movement_y += step;
        actor->movement_y = std::min<s32>(actor->movement_y, limit);
    } else {
        actor->movement_y -= step;
        actor->movement_y = std::max<s32>(actor->movement_y, -limit);
    }
    target.vx = actor->movement_x + actor->position.vx;
    target.vz = actor->movement_z + actor->position.vz;
    target.vy = actor->movement_y + actor->position.vy;
    result = collision_query_world(
        target.vx,
        target.vy,
        target.vz,
        definition->collision_radius,
        definition->collision_height,
        ACTOR_VELOCITY_COLLISION_FLAGS);
    if (result.kind == KfCollisionKind::None) {
        actor->position = target;
    } else {
        actor->action_progress = KF_ACTOR_PROGRESS_DRIFT_COLLIDED;
        result = collision_query_world(
            target.vx,
            actor->position.vy,
            actor->position.vz,
            definition->collision_radius,
            definition->collision_height,
            ACTOR_VELOCITY_COLLISION_FLAGS);
        if (result.kind != KfCollisionKind::None) {
            actor->movement_x = -actor->movement_x;
        } else if (collision_query_world(
                       actor->position.vx,
                       target.vy,
                       actor->position.vz,
                       definition->collision_radius,
                       definition->collision_height,
                       ACTOR_VELOCITY_COLLISION_FLAGS).kind
                   != KfCollisionKind::None) {
            actor->movement_y = -actor->movement_y;
        }
        if (collision_query_world(
                actor->position.vx,
                actor->position.vy,
                target.vz,
                definition->collision_radius,
                definition->collision_height,
                ACTOR_VELOCITY_COLLISION_FLAGS).kind
            != KfCollisionKind::None) {
            actor->movement_z = -actor->movement_z;
        }
    }
    actor->cell_x = actor->position.vx / KF_MAP_TILE_SIZE;
    actor->cell_z = actor->position.vz / KF_MAP_TILE_SIZE;
}

void actor_update_boss_death_sequence(void)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    SVECTOR direction;
    VECTOR position;

    actor_play_sound_at_phase(&boss_death_phase_sounds[1], ACTOR_BOSS_DEATH_SOUND_PHASE1);
    actor_play_sound_at_phase(&boss_death_phase_sounds[2], ACTOR_BOSS_DEATH_SOUND_PHASE2);
    actor_play_sound_at_phase(&boss_death_phase_sounds[3], ACTOR_BOSS_DEATH_SOUND_PHASE3);
    if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
        actor->animation_phase = KF_ACTOR_ANIMATION_PHASE_MAX;
        actor->action_progress = KF_ACTOR_PROGRESS_INIT;
        actor->lifecycle = KF_ACTOR_LIFECYCLE_DISABLED;
        map_floor_script(KF_FLOOR_5).floor5.boss_defeat = KF_MAP_SCRIPT_SET;
        map_object_pool_trigger_link(KF_MAP_LINK_BOSS_EMITTERS);
        actor_pool_begin_death_by_definition(0);
        actor_pool_begin_death_by_definition(2);
        actor_pool_begin_death_by_definition(3);
        actor_pool_begin_death_by_definition(4);
    }
    if (actor->animation_phase % (definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_DEATH] * ACTOR_BOSS_DEATH_EFFECT_PERIOD) == 0) {
        position.vx = actor->position.vx + (kf::random_next() & ACTOR_BOSS_DEATH_SCATTER_XZ_MASK) - ACTOR_BOSS_DEATH_SCATTER_XZ_BIAS;
        position.vz = actor->position.vz + (kf::random_next() & ACTOR_BOSS_DEATH_SCATTER_XZ_MASK) - ACTOR_BOSS_DEATH_SCATTER_XZ_BIAS;
        position.vy = actor->position.vy - (kf::random_next() & ACTOR_BOSS_DEATH_SCATTER_Y_MASK);

        effect_pool_construct(
            0, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER, KF_EFFECT_KIND_RADIAL_BLAST_ALTERNATE,
            &position, &direction, KfEffectSoundArguments{KF_EFFECT_SOUND_SILENT});
        if (actor->animation_phase % (definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_DEATH] * ACTOR_BOSS_DEATH_SOUND_PERIOD) == 0) {
            sound_ref_play(audio_playback(), &boss_death_loop_sound, ACTOR_BOSS_DEATH_LOOP_VOLUME);
        }
    }
}

static void actor_update_pursuit(KfActor *actor, const KfActorDefinition *definition)
{
    switch (actor->action_progress) {
    case KF_ACTOR_PROGRESS_INIT:
        actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
        if (actor->animation_clip != definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE]) {
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE];
            actor->animation_phase = 0;
        }
        actor->movement_yaw = actor_bearing_to_player(actor);
        break;
    case KF_ACTOR_PROGRESS_RUNNING:
        if (actor_move_along_heading(KF_ACTOR_MOVE_FORWARD, KF_ACTOR_COLLISION_STOP) != KF_ACTOR_MOVE_SUCCEEDED) {
            actor->action_progress = kf_enum_decode<KfActorActionProgress>((kf::random_next() >> ACTOR_PURSUIT_BACKOFF_RANDOM_SHIFT)
                + kf_enum_encode<u8>(KF_ACTOR_PROGRESS_BACKOFF_BASE));
            return;
        }
        if (kf::random_next() < ACTOR_PURSUIT_TURN_RANDOM_LIMIT) {
            actor->movement_yaw = actor_bearing_to_player(actor);
        }
        break;
    default:
        if (actor_move_along_heading(KF_ACTOR_MOVE_BACKWARD, KF_ACTOR_COLLISION_STOP) != KF_ACTOR_MOVE_SUCCEEDED || actor->action_progress < KF_ACTOR_PROGRESS_BACKOFF_END) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
        } else {
            actor->action_progress--;
        }
        break;
    }
    actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MOVE]);
}

static void actor_land(KfActor *actor, s32 floor_height)
{
    actor->position.vy = floor_height;
    actor->vertical_state = KF_ACTOR_VERTICAL_NONE;
    actor->vertical_velocity = 0;
}

static void actor_apply_fall_step(KfActor *actor, s32 next_y)
{
    actor->position.vy = next_y;
    actor->vertical_velocity += ACTOR_GRAVITY;
}

static void actor_bounce_from_jump(KfActor *actor)
{
    actor->vertical_state = KF_ACTOR_VERTICAL_JUMP_ATTACK;
    actor->vertical_velocity = ACTOR_JUMP_BOUNCE_VELOCITY_Y;
    actor->animation_phase = 0;
}

static void actor_update_vertical_motion(KfActor *actor, const KfActorDefinition *definition)
{
    s32 floor_height;
    s32 next_y;
    KfCollisionResult hit;

    switch (actor->vertical_state) {
    case KF_ACTOR_VERTICAL_NONE:
        break;
    case KF_ACTOR_VERTICAL_STEP_UP:
        floor_height = map_floor_height_at_position(&actor->position);
        next_y = actor->vertical_velocity + actor->position.vy;
        if (next_y > floor_height) {
            actor_apply_fall_step(actor, next_y);
            break;
        }
        actor_land(actor, floor_height);
        break;
    case KF_ACTOR_VERTICAL_FALL:
    case KF_ACTOR_VERTICAL_LONG_DROP:
        floor_height = map_floor_height_at_position(&actor->position);
        next_y = actor->vertical_velocity + actor->position.vy;
        if (next_y >= floor_height) {
            actor_land(actor, floor_height);
            break;
        }
        actor_apply_fall_step(actor, next_y);
        break;
    case KF_ACTOR_VERTICAL_JUMP_ATTACK:
        next_y = actor->vertical_velocity + actor->position.vy;
        hit = collision_query_world(
            actor->position.vx,
            next_y,
            actor->position.vz,
            definition->collision_radius,
            definition->collision_height,
            ACTOR_VELOCITY_COLLISION_FLAGS);
        if (hit.kind == KfCollisionKind::None) {
            actor_apply_fall_step(actor, next_y);
            break;
        }
        if (hit.kind == KfCollisionKind::Player) {
            player_apply_damage(0, ACTOR_JUMP_CONTACT_STRIKING_DAMAGE, 0, KF_PLAYER_STATUS_NONE, 0, 0, KF_FIXED12_ONE, KF_PLAYER_DAMAGE_MULTIPLIER_ONE);
            actor_bounce_from_jump(actor);
        } else if (hit.kind == KfCollisionKind::BelowFloor) {
            floor_height = map_floor_height_at_position(&actor->position);
            actor_land(actor, floor_height);
        } else if (hit.kind == KfCollisionKind::Ceiling) {
            actor->vertical_state = KF_ACTOR_VERTICAL_JUMP_ATTACK;
            actor->vertical_velocity = ACTOR_JUMP_CEILING_VELOCITY_Y;
        } else if (hit.kind == KfCollisionKind::Actor) {
            actor_bounce_from_jump(actor);
        }
        break;
    }
}

void actor_update_current_action(void)
{
    KfActor *actor = actor_state.current;
    KfActorDefinition *definition = actor_state.current_definition;
    struct KfVecXZs direction;
    VECTOR target;
    KfCollisionResult result;
    u16 gold_amount;
    KfMapAttribute attribute;

    collision_adjust_cell_occupancy(actor->cell_x, actor->cell_z, -1);
    switch (actor->action) {
    case KF_ACTOR_ACTION_NONE:
        // Vertical movement and occupancy maintenance still run below.
        break;
    case KF_ACTOR_ACTION_IDLE:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_IDLE];
            actor->animation_phase = 0;
        }
        actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_IDLE]);
        break;
    case KF_ACTOR_ACTION_WANDER:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            if (actor->animation_clip != definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE]) {
                actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE];
                actor->animation_phase = 0;
            }
            actor->movement_yaw = kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
        } else if (actor->collision_state == KF_ACTOR_COLLISION_CLEAR && kf::random_next() < ACTOR_WANDER_TURN_RANDOM_LIMIT) {
            actor->movement_yaw = kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
        }
        actor_move_along_heading(KF_ACTOR_MOVE_FORWARD, KF_ACTOR_COLLISION_STEER);
        actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MOVE]);
        break;
    case KF_ACTOR_ACTION_PURSUE:
        actor_update_pursuit(actor, definition);
        break;
    case KF_ACTOR_ACTION_HIT_REACTION:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_HIT_REACTION];
            actor->animation_phase = 0;
        }
        actor_advance_animation_clamped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_HIT_REACTION]);
        actor_play_sound_at_phase(&definition->sounds[KF_ACTOR_SOUND_HIT_REACTION], definition->action_animation_phases[KF_ACTOR_ANIM_SLOT_HIT_REACTION]);
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
            actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
        }
        break;
    case KF_ACTOR_ACTION_DYING:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_DEATH];
            actor->animation_phase = 0;
        }
        actor_advance_animation_clamped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_DEATH]);
        actor_play_sound_at_phase(&definition->sounds[KF_ACTOR_SOUND_DEATH], definition->action_animation_phases[KF_ACTOR_ANIM_SLOT_DEATH]);
        if (player_state.progress_state.current_floor == KF_FLOOR_5 && actor->definition_id == KF_FLOOR5_BOSS_DEFINITION) {
            actor_update_boss_death_sequence();
            return;
        }
        if (actor_animation_crossed_phase(actor, ACTOR_DEATH_DROP_PHASE)) {
            gold_amount = ((u32)kf::random_next() * definition->gold_drop_limit) >> ACTOR_GOLD_RANDOM_SHIFT;
            if (gold_amount != 0) {
                map_object_spawn_gold_drop(
                    gold_amount, &actor->position, -(definition->collision_height >> 1));
            }
            if (actor->slot_state == KF_ACTOR_SLOT_DYNAMIC || actor->slot_state == KF_ACTOR_SLOT_RESPAWNING) {
                if (definition->action_parameters.drop_object != KF_MAP_OBJECT_DROP_DISABLED && definition->action_parameters.drop_object != KF_OBJECT_NONE
                    && (kf::random_next() >> ACTOR_DROP_CHANCE_RANDOM_SHIFT) <= definition->action_parameters.drop_chance) {
                    map_object_spawn_drop(
                        KF_MAP_OBJECT_DROP_FROM_DEFINITION,
                        definition->action_parameters.drop_object,
                        &actor->position,
                        -(definition->collision_height >> 1));
                }
            } else if (actor->death_drop_object_id != KF_MAP_OBJECT_DROP_DISABLED) {
                map_object_spawn_drop(
                    KF_MAP_OBJECT_DROP_FROM_PLACEMENT,
                    actor->death_drop_object_id,
                    &actor->position,
                    -(definition->collision_height >> 1));
            }
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor->action = KF_ACTOR_ACTION_POST_DEATH;
            actor->animation_phase = KF_ACTOR_ANIMATION_PHASE_MAX;
            actor->action_progress = KF_ACTOR_PROGRESS_INIT;
        }
        break;
    case KF_ACTOR_ACTION_RETREAT:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            if (actor->animation_clip != definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE]) {
                actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE];
                actor->animation_phase = 0;
            }
            actor->movement_yaw = actor_bearing_to_player(actor);
        }
        if (actor_move_along_heading(KF_ACTOR_MOVE_BACKWARD, KF_ACTOR_COLLISION_STOP) != KF_ACTOR_MOVE_SUCCEEDED) {
            actor->action = KF_ACTOR_ACTION_NONE;
            actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
        } else {
            actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MOVE]);
        }
        break;
    case KF_ACTOR_ACTION_MELEE_ATTACK:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_LOCKED;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MELEE];
            actor->animation_phase = 0;
        }
        actor_advance_animation_clamped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MELEE]);
        actor_play_sound_at_phase(&definition->sounds[KF_ACTOR_SOUND_ATTACK], definition->action_animation_phases[KF_ACTOR_ANIM_SLOT_MELEE]);
        if (actor->animation_phase >= ACTOR_MELEE_CONTACT_PHASE
            && actor->animation_phase < definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MELEE] + ACTOR_MELEE_CONTACT_PHASE) {
            actor_try_attack_player(0, definition->awareness_distance, 0, KF_ACTOR_AIM_TOLERANCE);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
            actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
        }
        break;
    case KF_ACTOR_ACTION_POST_DEATH:
        if (player_state.progress_state.current_floor == KF_FLOOR_4 && actor->definition_id == KF_FLOOR4_TRANSFORM_SOURCE_DEFINITION) {
            actor_transform_definition5_to6(actor);
            actor_initialize(actor);
            return;
        }
        if (actor->action_progress >= KF_ACTOR_PROGRESS_POST_DEATH_END) {
            if (actor->slot_state == KF_ACTOR_SLOT_RESPAWNING) {
                actor->lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
            } else if (actor->slot_state == KF_ACTOR_SLOT_PERSISTENT || actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND) {
                actor->lifecycle = KF_ACTOR_LIFECYCLE_DISABLED;
            } else {
                actor->lifecycle = KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT;
            }
            return;
        }
        actor->action_progress++;
        break;
    case KF_ACTOR_ACTION_JUMP_ATTACK:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_JUMP_RISING;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_JUMP_ATTACK];
            actor->animation_phase = 0;
            attribute = map_cell_attribute_grid.cells[actor->cell_z][actor->cell_x];
            if (map_cell_attribute_height_table[kf_enum_encode<u8>(attribute) - 1] > ACTOR_JUMP_HEIGHT_THRESHOLD) {
                actor->vertical_state = KF_ACTOR_VERTICAL_JUMP_ATTACK;
                actor->vertical_velocity = ACTOR_JUMP_SHORT_VELOCITY_Y;
                actor->animation_step = ACTOR_JUMP_SHORT_ANIMATION_STEP;
            } else {
                actor->vertical_state = KF_ACTOR_VERTICAL_JUMP_ATTACK;
                actor->vertical_velocity = ACTOR_JUMP_LONG_VELOCITY_Y;
                actor->animation_step = ACTOR_JUMP_LONG_ANIMATION_STEP;
            }
            actor_prepare_charge_toward_player();
        }
        actor_apply_horizontal_movement();
        actor->animation_phase += actor->animation_step;
        actor->animation_phase = std::min<s32>(actor->animation_phase, KF_ACTOR_ANIMATION_PHASE_MAX);
        actor_play_sound_at_phase(&definition->sounds[KF_ACTOR_SOUND_ATTACK], definition->action_animation_phases[KF_ACTOR_ANIM_SLOT_JUMP_ATTACK]);
        switch (actor->action_progress) {
        default:
            // Only the jump-attack progress markers transition in this action.
            break;
        case KF_ACTOR_PROGRESS_JUMP_RISING:
            if (actor->vertical_velocity >= 0) {
                actor_prepare_charge_toward_player();
                actor->action_progress = KF_ACTOR_PROGRESS_JUMP_ATTACK_PENDING;
            }
            break;
        case KF_ACTOR_PROGRESS_JUMP_ATTACK_PENDING:
            if (actor_animation_crossed_phase(actor, ACTOR_JUMP_CONTACT_PHASE)) {
                actor_try_attack_player(0, definition->special_attack_range, 0, KF_ACTOR_AIM_TOLERANCE);
                actor->action_progress = KF_ACTOR_PROGRESS_JUMP_WAIT_FOR_LANDING;
            }
            break;
        case KF_ACTOR_PROGRESS_JUMP_WAIT_FOR_LANDING:
            if (actor->vertical_state != KF_ACTOR_VERTICAL_JUMP_ATTACK) {
                actor->vertical_state = KF_ACTOR_VERTICAL_NONE;
                actor->vertical_velocity = 0;
                result = collision_query_world(
                    actor->position.vx, actor->position.vy, actor->position.vz,
                    definition->collision_radius, definition->collision_height,
                    ACTOR_VELOCITY_COLLISION_FLAGS);

                // Retail compares the shifted kind with the ceiling detail, so this never
                // ran; the port option checks for a ceiling hit (upstream sushi-shi #50).
                if (kf::fix_actor_ceiling_fall && result.kind == KfCollisionKind::Ceiling) {
                    actor->vertical_state = KF_ACTOR_VERTICAL_FALL;
                }
                actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
                actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
            }
            break;
        }
        break;
    case KF_ACTOR_ACTION_SPECIAL_ATTACK:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_LOCKED;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_SPECIAL_ATTACK];
            actor->animation_phase = 0;
        }
        actor_advance_animation_clamped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_SPECIAL_ATTACK]);
        actor_play_sound_at_phase(&definition->sounds[KF_ACTOR_SOUND_ATTACK], definition->action_animation_phases[KF_ACTOR_ANIM_SLOT_SPECIAL_ATTACK]);
        if (actor->animation_phase >= ACTOR_SPECIAL_CONTACT_PHASE
            && actor->animation_phase < definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_SPECIAL_ATTACK] + ACTOR_SPECIAL_CONTACT_PHASE) {
            actor_try_attack_player(0, definition->special_attack_range, 0, KF_ACTOR_AIM_TOLERANCE);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
            actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
        }
        break;
    case KF_ACTOR_ACTION_EXIT_BLOCKED_PLACEMENT:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE];
            actor->animation_phase = 0;
        }
        result = collision_query_world(
            actor->position.vx, KF_COLLISION_IGNORE_HEIGHT, actor->position.vz, definition->collision_radius, 0, ACTOR_WALK_COLLISION_FLAGS);
        if (result.kind != KfCollisionKind::None) {
            target.vx = actor->position.vx;
            target.vz = actor->position.vz;
            angle_to_forward_xz(actor->rotation.angles.y, &direction);
            vector2s_scale_shift11(definition->move_speed, &direction);
            vector3i_add_xz(&target, &direction);
            if (actor_pool_find_overlap(target.vx, KF_COLLISION_IGNORE_HEIGHT, target.vz, definition->collision_radius, 0)
                == -1) {
                actor->position.vx = target.vx;
                actor->position.vz = target.vz;
            }
        } else {
            actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
            actor_select_next_action(ACTOR_ACTIVE_RANGE);
        }
        break;
    case KF_ACTOR_ACTION_DRIFT:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_DRIFT];
            actor->animation_phase = 0;
            actor->movement_y = 0;
            actor->movement_z = 0;
            actor->movement_x = 0;
            actor->movement_yaw = 0;
            actor->vertical_state = KF_ACTOR_VERTICAL_NONE;
            actor->vertical_velocity = 0;
        }
        actor_apply_random_movement(ACTOR_DRIFT_AXIS_ACCELERATION, ACTOR_DRIFT_AXIS_SPEED_LIMIT);
        if (kf::random_next() < (kf::random_max + 1) / 2) {
            actor->movement_yaw++;
            actor->movement_yaw = std::min<s32>(actor->movement_yaw, ACTOR_DRIFT_YAW_SPEED_LIMIT);
        } else {
            actor->movement_yaw--;
            actor->movement_yaw = std::max<s32>(actor->movement_yaw, -ACTOR_DRIFT_YAW_SPEED_LIMIT);
        }
        actor->rotation.angles.y = (actor->rotation.angles.y + actor->movement_yaw) & KF_ANGLE_WRAP_MASK;
        actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_DRIFT]);
        break;
    case KF_ACTOR_ACTION_EFFECT0:
        actor_update_effect_action(KF_ACTOR_EFFECT_SLOT_FIRST);
        break;
    case KF_ACTOR_ACTION_EFFECT1:
        actor_update_effect_action(KF_ACTOR_EFFECT_SLOT_SECOND);
        break;
    case KF_ACTOR_ACTION_EFFECT2:
        actor_update_effect_action(KF_ACTOR_EFFECT_SLOT_THIRD);
        break;
    case KF_ACTOR_ACTION_RETURN_HOME: {
        // The initial heading needs the same home position as later updates.
        const s32 home_x = map_placement_axis_position(actor->tile_x, actor->local_x);
        const s32 home_z = map_placement_axis_position(actor->tile_z, actor->local_z);
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_RUNNING;
            if (actor->animation_clip != definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE]) {
                actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MOVE];
                actor->animation_phase = 0;
            }

            actor->movement_yaw =
                vector_xz_to_angle(home_x - actor->position.vx, home_z - actor->position.vz);
        } else if (actor->collision_state == KF_ACTOR_COLLISION_CLEAR) {
            s32 home_dx;
            s32 home_dz;

            home_dx = actor->position.vx - home_x;
            home_dz = actor->position.vz - home_z;
            if (home_dx > -ACTOR_HOME_AXIS_TOLERANCE && home_dx < ACTOR_HOME_AXIS_TOLERANCE
                && home_dz > -ACTOR_HOME_AXIS_TOLERANCE && home_dz < ACTOR_HOME_AXIS_TOLERANCE) {
                actor->movement_yaw = kf_enum_encode<u8>(actor->heading_quadrant) * KF_ANGLE_QUARTER_TURN;
                actor->rotation.angles.y = angle_approach(
                    actor->rotation.angles.y, actor->movement_yaw, definition->turn_rate);
                if (actor->animation_clip != definition->action_animations[KF_ACTOR_ANIM_SLOT_MELEE]) {
                    break;
                }
                if (actor->movement_yaw == actor->rotation.angles.y
                    && actor_animation_crossed_phase(actor, ACTOR_HOME_ANIMATION_RESET_PHASE)) {
                    actor->animation_phase = 0;
                    actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MELEE];
                    break;
                }
                actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MOVE]);
                break;
            }
            if (kf::random_next() < ACTOR_HOME_TURN_RANDOM_LIMIT) {
                actor->movement_yaw = vector_xz_to_angle(
                    home_x - actor->position.vx, home_z - actor->position.vz);
            }
        }
        actor_move_along_heading(KF_ACTOR_MOVE_FORWARD, KF_ACTOR_COLLISION_STEER);
        actor_advance_animation_wrapped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MOVE]);
        break;
    }
    case KF_ACTOR_ACTION_MULTI_HIT_ATTACK:
        if (actor->action_progress == KF_ACTOR_PROGRESS_INIT) {
            actor->action_progress = KF_ACTOR_PROGRESS_LOCKED;
            actor->animation_clip = definition->action_animations[KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK];
            actor->animation_phase = 0;
        }
        actor_advance_animation_clamped(actor, definition->action_animation_steps[KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK]);
        actor_play_sound_at_phase(&definition->sounds[KF_ACTOR_SOUND_ATTACK], definition->action_animation_phases[KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK]);
        if (actor_animation_crossed_phase(actor, ACTOR_MULTI_HIT_FORWARD_PHASE1) || actor_animation_crossed_phase(actor, ACTOR_MULTI_HIT_FORWARD_PHASE2)
            || actor_animation_crossed_phase(actor, ACTOR_MULTI_HIT_FORWARD_PHASE3)
            || actor_animation_crossed_phase(actor, ACTOR_MULTI_HIT_FORWARD_PHASE4)) {
            actor_try_attack_player(KF_ACTOR_MULTI_HIT_FORWARD_MIN_RANGE, KF_ACTOR_MULTI_HIT_MAX_RANGE, 0, KF_ACTOR_AIM_TOLERANCE);
        }
        if (actor_animation_crossed_phase(actor, ACTOR_MULTI_HIT_NEGATIVE_YAW_PHASE)) {
            actor_try_attack_player(0, KF_ACTOR_MULTI_HIT_MAX_RANGE, -KF_ANGLE_EIGHTH_TURN, KF_ACTOR_AIM_TOLERANCE);
        }
        if (actor_animation_crossed_phase(actor, ACTOR_MULTI_HIT_POSITIVE_YAW_PHASE)) {
            actor_try_attack_player(0, KF_ACTOR_MULTI_HIT_MAX_RANGE, KF_ANGLE_EIGHTH_TURN, KF_ACTOR_AIM_TOLERANCE);
        }
        if (actor->animation_phase >= KF_ACTOR_ANIMATION_PHASE_MAX) {
            actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
            actor_select_next_action(actor_player_distance(actor, ACTOR_ACTIVE_RANGE));
        }
        break;
    }
    actor_update_vertical_motion(actor, definition);
    collision_adjust_cell_occupancy(actor->cell_x, actor->cell_z, 1);
}
