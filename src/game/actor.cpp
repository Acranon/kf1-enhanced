#include <kf/platform/prelude.h>
#include <kf/game/actor.h>
#include <kf/game/collision.h>
#include <kf/game/effect.h>
#include <kf/game/game.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>
#include <kf/lib/random.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum {
    ACTOR_SELECTION_ANGLE_TOLERANCE = 0x18e,
    ACTOR_MULTI_HIT_SELECTION_ANGLE_TOLERANCE = 0x1c7
};

enum {
    COMBAT_BASE_POWER_WEIGHT_DIVISOR = 5,
    COMBAT_DEFENSE_DENOMINATOR_MULTIPLIER = 2,
    ACTOR_DYING_DAMAGE_CUTOFF_PHASE = 1548,
    ACTOR_STATUS_CHANCE_RANDOM_SHIFT = 7,
    ACTOR_SELECTION_RANDOM_SHIFT = 4,
    ACTOR_SELECTION_FACING_BYPASS_LIMIT = 1638,
    ACTOR_PROFILE_FACING_BYPASS_LIMIT = 819,
    ACTOR_SELECTION_FAR_RANGE_FACTOR = 4,
    ACTOR_SELECTION_FAR_CHANCE_SHIFT = 4,
    ACTOR_SELECTION_NEAR_CHANCE_SHIFT = 2,
    ACTOR_SELECTION_MIDDLE_CHANCE_SHIFT = 1,
    ACTOR_SELECTION_OUTER_CHANCE_SHIFT = 2,
    ACTOR_GROUND_SELECTION_FAR_RANGE = 7000,
    ACTOR_GROUND_SELECTION_MIN_RANGE = 4001,
    ACTOR_GROUND_FAR_CHANCE_SHIFT = 1,
    ACTOR_GROUND_NEAR_CHANCE_SHIFT = 3,
    ACTOR_MULTI_HIT_FAR_CHANCE_SHIFT = 4,
    ACTOR_MULTI_HIT_NEAR_CHANCE_SHIFT = 2,
    ACTOR_SPAWNER_POPULATION_LIMIT = 2
};

std::array<KfActorActionProfile, KF_ACTOR_ACTION_PROFILE_COUNT> actor_action_profiles = {
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{12000, 256, 5500, 768, 32},
    KfActorActionProfile{9000, 256, 4000, 768, 48},
    KfActorActionProfile{},
    KfActorActionProfile{9000, 256, 4000, 768, 48},
    KfActorActionProfile{9000, 256, 4000, 768, 48},
    KfActorActionProfile{10000, 256, 1000, 768, 32},
    KfActorActionProfile{6000, 64, 3000, 768, 48},
    KfActorActionProfile{20000, 64, 5000, 768, 48},
    KfActorActionProfile{6000, 64, 2000, 768, 48},
    KfActorActionProfile{9000, 256, 4000, 768, 48},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{},
    KfActorActionProfile{20000, 256, 6000, 1024, 48},
    KfActorActionProfile{},
    KfActorActionProfile{9000, 256, 4000, 768, 48},
    KfActorActionProfile{12000, 256, 5500, 768, 32},
    KfActorActionProfile{20000, 256, 6000, 1024, 48},
};

std::array<SoundRef, KF_ACTOR_BOSS_DEATH_SOUND_COUNT> boss_death_phase_sounds = {
    SoundRef{27, 1, 88},
    SoundRef{27, 2, 88},
    SoundRef{27, 3, 88},
    SoundRef{88, 88, 88},
};

SoundRef boss_death_loop_sound = {70, 0, 65};

KfActor *actor_pool_find_free(void)
{
    for (auto &actor : actor_state.actors) {
        if (actor.slot_state == KF_ACTOR_SLOT_FREE) {
            return &actor;
        }
    }
    return NULL;
}

void func_8002cab4(void)
{
    actor_pool_find_free();
}

s32 actor_bearing_to_player(const KfActor *actor)
{
    return vector_xz_to_angle(
        actor_state.player_position.vx - actor->position.vx,
        actor_state.player_position.vz - actor->position.vz);
}

void actor_set_player_transform(
    const VECTOR *position_or_null,
    const SVECTOR *rotation_or_null)
{
    if (position_or_null != NULL) {
        actor_state.player_position = *position_or_null;
    }
    if (rotation_or_null != NULL) {
        actor_state.player_rotation = *rotation_or_null;
    }
}

void actor_update_cell_from_position(KfActor *actor)
{
    actor->cell_x = actor->position.vx / KF_MAP_TILE_SIZE;
    actor->cell_z = actor->position.vz / KF_MAP_TILE_SIZE;
}

void actor_set_position(KfActor *actor, const VECTOR *position)
{
    actor->position.vx = position->vx;
    actor->position.vz = position->vz;
    actor->position.vy = position->vy;
    actor->cell_x = position->vx / KF_MAP_TILE_SIZE;
    actor->cell_z = position->vz / KF_MAP_TILE_SIZE;
}

void actor_set_rotation(
    KfActor *actor,
    s16 x,
    s16 y,
    s16 z)
{
    actor->rotation.angles.x = x;
    actor->rotation.angles.y = y;
    actor->rotation.angles.z = z;
}

void actor_initialize(KfActor *actor)
{
    actor->lifecycle = KF_ACTOR_LIFECYCLE_ACTIVE;
    actor->animation_clip = KF_ANIMATION_CLIP_FIRST;
    actor->animation_phase = 0;
    actor->collision_state = KF_ACTOR_COLLISION_CLEAR;
    actor->vertical_velocity = 0;
    actor->vertical_state = KF_ACTOR_VERTICAL_NONE;
    actor->action = KF_ACTOR_ACTION_NONE;
    actor->action_progress = KF_ACTOR_PROGRESS_COMPLETE;
    actor->health = actor_state.definitions.entries[actor->definition_id].initial_health;
    if (actor->slot_state == KF_ACTOR_SLOT_RESPAWNING
        || actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND
        || actor->slot_state == KF_ACTOR_SLOT_PERSISTENT) {
        actor->rotation.angles.y = kf_enum_encode<u8>(actor->heading_quadrant) * KF_ANGLE_QUARTER_TURN;
    } else {
        actor->rotation.angles.y = kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
    }
    collision_adjust_cell_occupancy(actor->cell_x, actor->cell_z, 1);
}

static inline void actor_initialize_at_home(KfActor *actor)
{
    VECTOR position;

    position.vx = map_placement_axis_position(actor->tile_x, actor->local_x);
    position.vz = map_placement_axis_position(actor->tile_z, actor->local_z);
    position.vy = map_floor_height_at_position(&position);
    actor_set_position(actor, &position);
    actor_set_rotation(actor, 0, 0, 0);
    actor_initialize(actor);
}

void actor_initialize_current(void)
{
    KfActor *actor = actor_state.current;

    actor_initialize_at_home(actor);
}

void actor_initialize_slot(u16 actor_index)
{
    KfActor *actor = &actor_state.actors[actor_index];

    actor->lifecycle = KF_ACTOR_LIFECYCLE_ACTIVE;
    actor_initialize_at_home(actor);
}

void actor_pool_clear(void)
{
    for (auto &actor : actor_state.actors) {
        actor.slot_state = KF_ACTOR_SLOT_FREE;
        actor.lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
        actor.animation_cache = NULL;
    }
}

void actor_set_action(KfActor *actor, KfActorAction action)
{
    actor->action = action;
    actor->action_progress = KF_ACTOR_PROGRESS_INIT;
}

void actor_pool_spawn(
    u8 definition_id,
    const VECTOR *position,
    const struct KfVec3s *rotation)
{
    KfActor *actor = actor_pool_find_free();
    if (actor == NULL)
        return;
    actor->definition_id = definition_id;
    actor->slot_state = KF_ACTOR_SLOT_DYNAMIC;
    actor->tile_z = KF_MAP_CELL_COORD_INVALID;
    actor->tile_x = KF_MAP_CELL_COORD_INVALID;
    actor->culling_mode = KF_ACTOR_CULL_VISIBILITY_GRID;
    actor_set_position(actor, position);
    actor_set_rotation(actor, rotation->x, rotation->y, rotation->z);
    actor_initialize(actor);
    actor_set_action(actor, KF_ACTOR_ACTION_PURSUE);
}

void actor_pool_begin_death_by_definition(u16 definition_id)
{
    KfActorDefinition *definition = &actor_state.definitions.entries[definition_id];

    for (auto &actor : actor_state.actors) {
        if (actor.slot_state != KF_ACTOR_SLOT_FREE && actor.definition_id == definition_id) {
            if (actor.lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE
                && definition->action_animations[KF_ACTOR_ANIM_SLOT_DEATH] != KF_ANIMATION_CLIP_NONE) {
                actor_set_action(&actor, KF_ACTOR_ACTION_DYING);
            } else {
                actor.lifecycle = KF_ACTOR_LIFECYCLE_DISABLED;
            }
        }
    }
}

s32 combat_calculate_damage_component(s32 base_power, s32 attack, s32 defense)
{
    s32 difference;

    if (attack == 0) {
        return 0;
    }
    attack += base_power / COMBAT_BASE_POWER_WEIGHT_DIVISOR;
    difference = attack - defense;
    difference = std::max<s32>(difference, 0);
    if (defense == 0) {
        defense = 1;
    }
    return difference + attack * attack / (defense * COMBAT_DEFENSE_DENOMINATOR_MULTIPLIER);
}

void actor_apply_damage(
    u16 actor_index,
    u16 base_power,
    u16 component0,
    u16 component1,
    u16 component2,
    u16 component3,
    u16 component4,
    u16 scale,
    KfEffectType hit_flags)
{
    KfActor *actor = &actor_state.actors[actor_index];
    KfActorDefinition *definition = &actor_state.definitions.entries[actor->definition_id];
    s32 damage;
    s32 health;
    s32 remaining;

    if (player_state.progress_state.current_floor == KF_FLOOR_5 && actor->definition_id == KF_FLOOR5_BOSS_DEFINITION) {
        if (map_floor_script(KF_FLOOR_5).floor5.boss_encounter_started == KF_MAP_SCRIPT_UNSET) {
            return;
        }
        if (actor->health == 0) {
            return;
        }
    }
    if (actor->action == KF_ACTOR_ACTION_DYING && actor->animation_phase >= ACTOR_DYING_DAMAGE_CUTOFF_PHASE) {
        return;
    }
    if (actor->action == KF_ACTOR_ACTION_POST_DEATH) {
        return;
    }
    damage = combat_calculate_damage_component(
        base_power * KF_DAMAGE_SUBUNITS_PER_HP,
        component0 * KF_DAMAGE_SUBUNITS_PER_HP,
        definition->defenses[KF_COMBAT_COMPONENT_CUTTING] * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += combat_calculate_damage_component(
        base_power * KF_DAMAGE_SUBUNITS_PER_HP,
        component1 * KF_DAMAGE_SUBUNITS_PER_HP,
        definition->defenses[KF_COMBAT_COMPONENT_STRIKING] * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += combat_calculate_damage_component(
        base_power * KF_DAMAGE_SUBUNITS_PER_HP,
        component2 * KF_DAMAGE_SUBUNITS_PER_HP,
        definition->defenses[KF_COMBAT_COMPONENT_PIERCING] * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += combat_calculate_damage_component(
        base_power * KF_DAMAGE_SUBUNITS_PER_HP,
        component3 * KF_DAMAGE_SUBUNITS_PER_HP,
        definition->defenses[KF_COMBAT_COMPONENT_HOLY] * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += combat_calculate_damage_component(
        base_power * KF_DAMAGE_SUBUNITS_PER_HP,
        component4 * KF_DAMAGE_SUBUNITS_PER_HP,
        definition->defenses[KF_COMBAT_COMPONENT_FIRE] * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += KF_DAMAGE_SUBUNITS_PER_HP / 2;
    damage = (damage / KF_DAMAGE_SUBUNITS_PER_HP) * scale / KF_ACTOR_DAMAGE_SCALE_ONE;
    hit_flags &= KF_ACTOR_DAMAGE_CREDIT_MASK;
    if (damage == 0) {
        return;
    }
    if (actor->health != 0 && hit_flags == KF_ACTOR_DAMAGE_CREDIT_PLAYER) {
        if (component0 == 0 && component1 == 0 && component2 == 0) {
            player_increment_magic_training();
        } else if (component1 != 0 || component2 != 0) {
            player_increment_physical_power_training();
        }
    }
    if (actor->action == KF_ACTOR_ACTION_DRIFT) {
        actor->vertical_state = KF_ACTOR_VERTICAL_FALL;
        actor->vertical_velocity = 0;
    }
    health = actor->health;
    remaining = health - damage;
    if (remaining <= 0) {
        remaining = 0;
        if (health != 0 && hit_flags == KF_ACTOR_DAMAGE_CREDIT_PLAYER) {
            player_add_experience(definition->experience_reward);
        }
        if (definition->action_animations[KF_ACTOR_ANIM_SLOT_DEATH] != KF_ANIMATION_CLIP_NONE) {
            actor_set_action(actor, KF_ACTOR_ACTION_DYING);
        }
    } else {
        if (definition->action_animations[KF_ACTOR_ANIM_SLOT_HIT_REACTION] != KF_ANIMATION_CLIP_NONE) {
            actor_set_action(actor, KF_ACTOR_ACTION_HIT_REACTION);
        }
    }
    actor->health = remaining;
}

void actor_pool_apply_radial_damage(
    const VECTOR *origin,
    u32 radius,
    u16 falloff_q12,
    u16 base_power,
    u16 component0,
    u16 component1,
    u16 component2,
    u16 component3,
    u16 component4,
    u16 scale,
    KfEffectType hit_flags)
{
    KfActorDefinition *definition;
    s32 distance;
    u16 damage_scale;

    for (s16 index = 0; index < KF_ACTOR_CAPACITY; index++) {
        KfActor *actor = &actor_state.actors[index];
        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
            continue;
        }
        if (actor->action == KF_ACTOR_ACTION_POST_DEATH) {
            continue;
        }
        if (actor == actor_state.current) {
            continue;
        }
        definition = &actor_state.definitions.entries[actor->definition_id];
        distance = actor_distance_to_point(
            actor,
            origin->vx,
            origin->vy,
            origin->vz,
            radius,
            definition->collision_height,
            radius);
        if (distance == -1) {
            continue;
        }
        if (falloff_q12 != KF_FIXED12_ONE) {
            damage_scale = radial_damage_attenuated_scale(distance, radius, falloff_q12, scale);
        } else {
            damage_scale = scale;
        }
        actor_apply_damage(
            index,
            base_power,
            component0,
            component1,
            component2,
            component3,
            component4,
            damage_scale,
            hit_flags);
    }
}

void actor_try_attack_player(
    u16 minimum_distance,
    u16 maximum_distance,
    s16 angle_offset,
    s16 angle_tolerance)
{
    KfActorDefinition *definition = actor_state.current_definition;
    KfActor *actor = actor_state.current;
    s32 distance;
    s32 angle;
    KfPlayerStatusFlags status_effect;

    distance = actor_distance_to_point(
        actor,
        actor_state.player_position.vx,
        actor_state.player_position.vy + KF_PLAYER_CAMERA_HEIGHT,
        actor_state.player_position.vz,
        maximum_distance,
        definition->collision_height,
        KF_COLLISION_PLAYER_HEIGHT);
    if (distance == -1) {
        return;
    }
    if (distance < minimum_distance) {
        return;
    }
    angle = actor_bearing_to_player(actor);
    if (!angle_within_tolerance(actor->rotation.angles.y + angle_offset, angle, angle_tolerance)) {
        return;
    }
    status_effect = KF_PLAYER_STATUS_NONE;
    if (definition->status_effect_chance != 0
        && (kf::random_next() >> ACTOR_STATUS_CHANCE_RANDOM_SHIFT) < definition->status_effect_chance) {
        status_effect = definition->status_effect;
    }
    player_apply_damage(
        definition->attack_components[KF_COMBAT_COMPONENT_CUTTING],
        definition->attack_components[KF_COMBAT_COMPONENT_STRIKING],
        definition->attack_components[KF_COMBAT_COMPONENT_PIERCING],
        status_effect,
        0,
        0,
        KF_FIXED12_ONE,
        KF_PLAYER_DAMAGE_MULTIPLIER_ONE);
}

KfActor *actor_pool_find_target_in_cone(
    const VECTOR *origin,
    s16 facing,
    u32 max_distance,
    s32 angle_tolerance,
    s32 *distance_out)
{
    KfActor *best = NULL;
    s16 best_difference = KF_CONE_SEARCH_INITIAL_ANGLE_ERROR;
    s32 best_distance = 0;
    s32 distance;
    s16 delta;
    s16 folded;

    for (auto &actor : actor_state.actors) {
        if (actor.lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
            continue;
        }
        if (actor.action == KF_ACTOR_ACTION_POST_DEATH) {
            continue;
        }
        if (&actor == actor_state.current) {
            continue;
        }
        distance = actor_distance_to_point(
            &actor, origin->vx, KF_COLLISION_IGNORE_HEIGHT, origin->vz, max_distance, 0, 0);
        if (distance == -1) {
            continue;
        }
        delta = vector_xz_to_angle(
            actor.position.vx - origin->vx, origin->vz - actor.position.vz) - facing;
        folded = angle_error_magnitude(delta);
        if (angle_tolerance < folded) {
            continue;
        }
        if (folded < best_difference) {
            best_difference = folded;
            best = &actor;
            best_distance = distance;
        }
    }
    *distance_out = best_distance;
    return best;
}

s32 actor_distance_to_point(
    const KfActor *actor,
    s32 point_x,
    s32 point_y,
    s32 point_z,
    s32 max_distance,
    s32 actor_height,
    s32 point_height)
{
    s32 delta_y;
    s32 distance;

    point_x = actor->position.vx - point_x;
    if (point_x < -max_distance || max_distance < point_x) {
        return -1;
    }
    point_z = actor->position.vz - point_z;
    if (point_z < -max_distance || max_distance < point_z) {
        return -1;
    }
    if (point_y != KF_COLLISION_IGNORE_HEIGHT) {
        actor_height >>= 1;
        point_height >>= 1;
        delta_y = (actor->position.vy - actor_height) - (point_y - point_height);
        point_height += actor_height;
        if (delta_y < -point_height) {
            return -1;
        }
        if (point_height < delta_y) {
            return -1;
        }
    }
    distance = fixed_vector2_length(point_x, point_z);
    if (max_distance < distance) {
        return -1;
    }
    return distance;
}

s32 actor_pool_find_overlap(s32 point_x, s32 point_y, s32 point_z, s32 radius_padding, s32 point_height)
{
    KfActorDefinition *definition;

    for (s16 index = 0; index < KF_ACTOR_CAPACITY; index++) {
        KfActor *actor = &actor_state.actors[index];
        if (actor->lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE) {
            continue;
        }
        if (actor->action == KF_ACTOR_ACTION_DYING) {
            continue;
        }
        if (actor->action == KF_ACTOR_ACTION_POST_DEATH) {
            continue;
        }
        if (actor == actor_state.current) {
            continue;
        }
        definition = &actor_state.definitions.entries[actor->definition_id];
        if (actor_distance_to_point(
                actor,
                point_x,
                point_y,
                point_z,
                definition->collision_radius + radius_padding,
                definition->collision_height,
                point_height) != -1) {
            return index;
        }
    }
    return -1;
}

void actor_bind_current(KfActor *actor)
{
    s32 index;

    actor_state.current = actor;
    index = actor ? actor - actor_state.actors.data() : -1;
    actor_state.current_definition = actor ? &actor_state.definitions.entries[actor->definition_id] : nullptr;
    actor_state.current_index = index;
    actor_state.current_definition_id = actor ? actor->definition_id : 0;
}

void actor_advance_animation_wrapped(KfActor *actor, s16 delta)
{
    actor->animation_step = std::abs(delta);
    actor->animation_phase = (actor->animation_phase + delta) & KF_ACTOR_ANIMATION_PHASE_MAX;
}

void actor_advance_animation_clamped(KfActor *actor, s16 delta)
{
    actor->animation_step = std::abs(delta);
    const s16 phase = actor->animation_phase + delta;
    actor->animation_phase = std::clamp<s32>(phase, 0, KF_ACTOR_ANIMATION_PHASE_MAX);
}

bool actor_animation_crossed_phase(const KfActor *actor, u16 phase)
{
    return phase < actor->animation_phase
        && phase >= actor->animation_phase - actor->animation_step;
}

void actor_play_sound_at_phase(const SoundRef *sound, u16 phase)
{
    KfActor *actor = actor_state.current;

    if (!actor_animation_crossed_phase(actor, phase)) {
        return;
    }
    if (player_state.progress_state.current_floor == KF_FLOOR_5 && actor->definition_id == KF_FLOOR5_BOSS_DEFINITION) {
        audio_play_spatial_range(
            sound, &actor->position, KF_AUDIO_MAX_VOLUME, KF_AUDIO_EXTENDED_MAX_DISTANCE, KF_AUDIO_EXTENDED_ATTENUATION_DISTANCE);
    } else {
        audio_play_spatial_default_range(
            sound, &actor->position, KF_AUDIO_MAX_VOLUME);
    }
}

KfActorAction actor_try_select_action_distance_facing(
    KfActorAction action,
    s32 distance,
    u16 chance,
    u16 distance_scale)
{
    KfActor *actor = actor_state.current;
    u16 odds = chance;

    if (actor->action == action && actor->action_progress != KF_ACTOR_PROGRESS_COMPLETE) {
        return actor->action;
    }
    if (distance_scale * ACTOR_SELECTION_FAR_RANGE_FACTOR < distance) {
        if (actor->slot_state == KF_ACTOR_SLOT_HOMEBOUND) {
            return KF_ACTOR_ACTION_NONE;
        }
        odds >>= ACTOR_SELECTION_FAR_CHANCE_SHIFT;
    }
    if (distance < distance_scale) {
        odds <<= ACTOR_SELECTION_NEAR_CHANCE_SHIFT;
    } else if (distance < distance_scale + distance_scale / 2) {
        odds <<= ACTOR_SELECTION_MIDDLE_CHANCE_SHIFT;
    } else {
        odds >>= ACTOR_SELECTION_OUTER_CHANCE_SHIFT;
    }
    if (!((kf::random_next() >> ACTOR_SELECTION_RANDOM_SHIFT) < odds)) {
        return KF_ACTOR_ACTION_NONE;
    }
    if (kf::random_next() < ACTOR_SELECTION_FACING_BYPASS_LIMIT) {
        return action;
    }
    if (angle_within_tolerance(
            actor->rotation.angles.y,
            actor_bearing_to_player(actor),
            ACTOR_SELECTION_ANGLE_TOLERANCE)) {
        return action;
    }
    return KF_ACTOR_ACTION_NONE;
}

KfActorAction actor_try_select_ground_action(KfActorAction action, s32 distance, u16 chance)
{
    KfActor *actor = actor_state.current;
    u16 odds = chance;

    if (actor->action == action && actor->action_progress != KF_ACTOR_PROGRESS_COMPLETE) {
        return actor->action;
    }
    if (-(map_floor_height_grid.cells[actor->cell_z][actor->cell_x] * KF_MAP_HEIGHT_STEP)
        != actor->position.vy) {
        return KF_ACTOR_ACTION_NONE;
    }
    if (actor_state.player_target == actor) {
        actor_state.player_target = NULL;
        return action;
    }
    if (distance > ACTOR_GROUND_SELECTION_FAR_RANGE) {
        odds >>= ACTOR_GROUND_FAR_CHANCE_SHIFT;
    } else {
        if (distance < ACTOR_GROUND_SELECTION_MIN_RANGE) {
            return KF_ACTOR_ACTION_NONE;
        }
        odds <<= ACTOR_GROUND_NEAR_CHANCE_SHIFT;
    }
    if (!((kf::random_next() >> ACTOR_SELECTION_RANDOM_SHIFT) < odds)) {
        return KF_ACTOR_ACTION_NONE;
    }
    if (kf::random_next() < ACTOR_SELECTION_FACING_BYPASS_LIMIT) {
        return action;
    }
    if (angle_within_tolerance(
            actor->rotation.angles.y,
            actor_bearing_to_player(actor),
            ACTOR_SELECTION_ANGLE_TOLERANCE)) {
        return action;
    }
    return KF_ACTOR_ACTION_NONE;
}

KfActorAction actor_try_select_multi_hit_action(KfActorAction action, s32 distance, u16 chance)
{
    KfActor *actor = actor_state.current;
    u16 odds = chance;

    if (actor->action == action && actor->action_progress != KF_ACTOR_PROGRESS_COMPLETE) {
        return actor->action;
    }
    if (distance > KF_ACTOR_MULTI_HIT_MAX_RANGE) {
        odds >>= ACTOR_MULTI_HIT_FAR_CHANCE_SHIFT;
    } else {
        if (distance < KF_ACTOR_MULTI_HIT_FORWARD_MIN_RANGE) {
            return KF_ACTOR_ACTION_NONE;
        }
        odds <<= ACTOR_MULTI_HIT_NEAR_CHANCE_SHIFT;
    }
    if (!((kf::random_next() >> ACTOR_SELECTION_RANDOM_SHIFT) < odds)) {
        return KF_ACTOR_ACTION_NONE;
    }
    if (angle_within_tolerance(
            actor->rotation.angles.y,
            actor_bearing_to_player(actor),
            ACTOR_MULTI_HIT_SELECTION_ANGLE_TOLERANCE)) {
        return action;
    }
    return KF_ACTOR_ACTION_NONE;
}

KfActorAction actor_try_select_profiled_action(KfActorAction action, s32 distance, KfActorEffectCode effect_code, u16 chance)
{
    KfEffectKind profile = kf_enum_decode<KfEffectKind>(kf_enum_encode<u16>(effect_code & KF_ACTOR_EFFECT_KIND_MASK));
    KfActorActionProfile *weights = &actor_action_profiles[kf_enum_encode<u16>(profile)];
    KfActor *actor = actor_state.current;
    s32 odds;
    KfActor *candidate;
    const KfEffectRecord *record;
    s16 index;
    s16 count;

    if (actor->action == action && actor->action_progress != KF_ACTOR_PROGRESS_COMPLETE) {
        return actor->action;
    }
    odds = weights->near_weight;
    if (distance >= weights->far_distance) {
        odds = weights->far_weight;
    } else {
        if (distance >= weights->near_distance) {
            odds = weights->middle_weight;
        }
    }
    odds = (chance * odds) >> KF_FIXED8_BITS;
    if (!((kf::random_next() >> ACTOR_SELECTION_RANDOM_SHIFT) < odds)) {
        return KF_ACTOR_ACTION_NONE;
    }
    if (!angle_within_tolerance(
            actor->rotation.angles.y,
            actor_bearing_to_player(actor),
            KF_ACTOR_AIM_TOLERANCE)
        && kf::random_next() >= ACTOR_PROFILE_FACING_BYPASS_LIMIT) {
        return KF_ACTOR_ACTION_NONE;
    }
    if (profile == KF_EFFECT_KIND_ACTOR_SPAWNER) {
        candidate = actor_state.actors.data();
        count = 0;
        index = KF_ACTOR_CAPACITY - 1;
        do {
            if (candidate->slot_state != KF_ACTOR_SLOT_FREE
                && candidate->lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
                count++;
            }
            candidate++;
        } while (--index != -1);
        record = effect_state.records.data();
        index = KF_EFFECT_CAPACITY - 1;
        do {
            if (record->type != KF_EFFECT_SLOT_FREE
                && record->kind == KF_EFFECT_KIND_ACTOR_SPAWNER) {
                count++;
            }
            record++;
        } while (--index != -1);
        if (count >= ACTOR_SPAWNER_POPULATION_LIMIT) {
            return KF_ACTOR_ACTION_NONE;
        }
    }
    return action;
}


void actor_reset_module_state(void)
{
    kf::restore_initial_value<actor_action_profiles>();
    kf::restore_initial_value<boss_death_phase_sounds>();
    kf::restore_initial_value<boss_death_loop_sound>();
}
