#ifndef KF_GAME_ACTOR_H
#define KF_GAME_ACTOR_H

inline constexpr unsigned KF_FLOOR5_BOSS_DEFINITION = 7;
inline constexpr unsigned KF_FLOOR4_TRANSFORM_SOURCE_DEFINITION = 5;
inline constexpr unsigned KF_FLOOR4_TRANSFORM_RESULT_DEFINITION = 6;

#include <kf/game/audio.h>
#include <kf/game/combat.h>
#include <kf/game/effect.h>
#include <kf/game/player_status.h>
#include <kf/lib/animation.h>
#include <kf/lib/enum.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/map.h>
#include <kf/lib/math.h>
#include <kf/lib/types.h>

#include <array>

struct KfAnimationCacheRecord;

enum {
    KF_ACTOR_DEFINITION_COUNT = 12,
    KF_ACTOR_DEFINITION_WORD_COUNT = 456,
    KF_ACTOR_ATTACHMENT_OFFSET_COUNT = 2,
    KF_ACTOR_CAPACITY = 128
};

enum class KfActorSlotState : u8 {
    KF_ACTOR_SLOT_DYNAMIC = 0,
    KF_ACTOR_SLOT_PERSISTENT = 1,
    KF_ACTOR_SLOT_RESPAWNING = 2,
    KF_ACTOR_SLOT_HOMEBOUND = 3,
    KF_ACTOR_SLOT_FREE = 0xff
}; using enum KfActorSlotState;

enum class KfActorLifecycle : u8 {
    KF_ACTOR_LIFECYCLE_DORMANT = 0,
    KF_ACTOR_LIFECYCLE_ACTIVE = 1,
    KF_ACTOR_LIFECYCLE_WAIT_FOR_RANGE_EXIT = 2,
    KF_ACTOR_LIFECYCLE_DISABLED = 3
}; using enum KfActorLifecycle;

enum class KfActorCullingMode : u8 {
    KF_ACTOR_CULL_VISIBILITY_GRID = 0,
    KF_ACTOR_CULL_NEAR_SQUARE = 1
}; using enum KfActorCullingMode;

enum class KfActorAction : u8 {
    KF_ACTOR_ACTION_IDLE = 0,
    KF_ACTOR_ACTION_WANDER = 1,
    KF_ACTOR_ACTION_PURSUE = 2,
    KF_ACTOR_ACTION_RETREAT = 3,
    KF_ACTOR_ACTION_MELEE_ATTACK = 4,
    KF_ACTOR_ACTION_HIT_REACTION = 5,
    KF_ACTOR_ACTION_DYING = 6,
    KF_ACTOR_ACTION_JUMP_ATTACK = 16,
    KF_ACTOR_ACTION_SPECIAL_ATTACK = 17,
    KF_ACTOR_ACTION_DRIFT = 18,
    KF_ACTOR_ACTION_EFFECT0 = 19,
    KF_ACTOR_ACTION_EFFECT1 = 20,
    KF_ACTOR_ACTION_EFFECT2 = 21,
    KF_ACTOR_ACTION_MULTI_HIT_ATTACK = 22,
    KF_ACTOR_ACTION_EXIT_BLOCKED_PLACEMENT = 32,
    KF_ACTOR_ACTION_RETURN_HOME = 33,
    KF_ACTOR_ACTION_POST_DEATH = 127,
    KF_ACTOR_ACTION_NONE = 0xff
}; using enum KfActorAction;

enum class KfActorActionProgress : u8 {
    KF_ACTOR_PROGRESS_INIT = 0,
    KF_ACTOR_PROGRESS_RUNNING = 1,
    KF_ACTOR_PROGRESS_JUMP_RISING = 1,
    KF_ACTOR_PROGRESS_JUMP_ATTACK_PENDING = 2,
    KF_ACTOR_PROGRESS_JUMP_WAIT_FOR_LANDING = 3,
    KF_ACTOR_PROGRESS_DRIFT_COLLIDED = 2,
    KF_ACTOR_PROGRESS_POST_DEATH_END = 7,
    KF_ACTOR_PROGRESS_BACKOFF_END = 11,
    KF_ACTOR_PROGRESS_BACKOFF_BASE = 13,
    KF_ACTOR_PROGRESS_BACKOFF_LAST = 28,
    KF_ACTOR_PROGRESS_LOCKED = 0xf0,
    KF_ACTOR_PROGRESS_COMPLETE = 0xff
}; using enum KfActorActionProgress;
inline KfActorActionProgress& operator++(KfActorActionProgress& value)
    { value = static_cast<KfActorActionProgress>(static_cast<u8>(value) + 1); return value; }
    inline KfActorActionProgress operator++(KfActorActionProgress& value, int)
    { KfActorActionProgress previous = value; ++value; return previous; }
    inline KfActorActionProgress& operator--(KfActorActionProgress& value)
    { value = static_cast<KfActorActionProgress>(static_cast<u8>(value) - 1); return value; }
    inline KfActorActionProgress operator--(KfActorActionProgress& value, int)
    { KfActorActionProgress previous = value; --value; return previous; }

enum class KfActorMoveDirection : s32 {
    KF_ACTOR_MOVE_BACKWARD = -1,
    KF_ACTOR_MOVE_FORWARD = 1
}; using enum KfActorMoveDirection;

enum class KfActorCollisionPolicy : s32 {
    KF_ACTOR_COLLISION_STEER = 0,
    KF_ACTOR_COLLISION_STOP = 1
}; using enum KfActorCollisionPolicy;

enum class KfActorMoveResult : s32 {
    KF_ACTOR_MOVE_SUCCEEDED = 0,
    KF_ACTOR_MOVE_BLOCKED = 1
}; using enum KfActorMoveResult;

enum class KfActorVerticalState : u8 {
    KF_ACTOR_VERTICAL_NONE = 0,
    KF_ACTOR_VERTICAL_STEP_UP = 1,
    KF_ACTOR_VERTICAL_FALL = 2,
    KF_ACTOR_VERTICAL_LONG_DROP = 3,
    KF_ACTOR_VERTICAL_JUMP_ATTACK = 4
}; using enum KfActorVerticalState;

enum class KfActorCollisionState : u8 {
    KF_ACTOR_COLLISION_CLEAR = 0,
    KF_ACTOR_COLLISION_SLIDING = 1,
    KF_ACTOR_COLLISION_BLOCKED = 2
}; using enum KfActorCollisionState;

enum {
    KF_ACTOR_ANIM_SLOT_IDLE = 0,
    KF_ACTOR_ANIM_SLOT_MOVE = 1,
    KF_ACTOR_ANIM_SLOT_MELEE = 2,
    KF_ACTOR_ANIM_SLOT_HIT_REACTION = 3,
    KF_ACTOR_ANIM_SLOT_DEATH = 4,
    KF_ACTOR_ANIM_SLOT_JUMP_ATTACK = 5,
    KF_ACTOR_ANIM_SLOT_SPECIAL_ATTACK = 6,
    KF_ACTOR_ANIM_SLOT_DRIFT = 7,
    KF_ACTOR_ANIM_SLOT_EFFECT0 = 8,
    KF_ACTOR_ANIM_SLOT_EFFECT1 = 9,
    KF_ACTOR_ANIM_SLOT_EFFECT2 = 10,
    KF_ACTOR_ANIM_SLOT_MULTI_HIT_ATTACK = 11,
    KF_ACTOR_ANIM_SLOT_COUNT = 16
};

enum {
    KF_ACTOR_ANIMATION_PHASE_PERIOD = 0x1000,
    KF_ACTOR_ANIMATION_PHASE_MAX = KF_ACTOR_ANIMATION_PHASE_PERIOD - 1,
    KF_ACTOR_AIM_TOLERANCE = 0x155
};

enum {
    KF_ACTOR_SOUND_ATTACK = 0,
    KF_ACTOR_SOUND_HIT_REACTION = 1,
    KF_ACTOR_SOUND_DEATH = 2,
    KF_ACTOR_SOUND_COUNT = 3
};

enum {
    KF_ACTOR_MULTI_HIT_FORWARD_MIN_RANGE = 8000,
    KF_ACTOR_MULTI_HIT_MAX_RANGE = 11000,
    KF_ACTOR_BOSS_DEATH_SOUND_COUNT = 4
};

enum {
    KF_ACTOR_ACTION_PROFILE_COUNT = 25
};

enum class KfActorEffectSlot : s32 {
    KF_ACTOR_EFFECT_SLOT_FIRST = 0,
    KF_ACTOR_EFFECT_SLOT_SECOND = 1,
    KF_ACTOR_EFFECT_SLOT_THIRD = 2
}; using enum KfActorEffectSlot;

enum class KfActorEffectCode : u8 {
    KF_ACTOR_EFFECT_CODE_NONE = 0,
    KF_ACTOR_EFFECT_KIND_MASK = 0x1f,
    KF_ACTOR_EFFECT_PAIRED = 0x20,
    KF_ACTOR_EFFECT_CODE_UNSET = 0xff
}; using enum KfActorEffectCode;
constexpr KfActorEffectCode operator|(KfActorEffectCode lhs, KfActorEffectCode rhs)
    { return static_cast<KfActorEffectCode>(static_cast<u8>(lhs) | static_cast<u8>(rhs)); }
    constexpr KfActorEffectCode operator&(KfActorEffectCode lhs, KfActorEffectCode rhs)
    { return static_cast<KfActorEffectCode>(static_cast<u8>(lhs) & static_cast<u8>(rhs)); }
    constexpr KfActorEffectCode operator^(KfActorEffectCode lhs, KfActorEffectCode rhs)
    { return static_cast<KfActorEffectCode>(static_cast<u8>(lhs) ^ static_cast<u8>(rhs)); }
    constexpr KfActorEffectCode operator~(KfActorEffectCode value)
    { return static_cast<KfActorEffectCode>(~static_cast<u8>(value)); }
    inline KfActorEffectCode& operator|=(KfActorEffectCode& lhs, KfActorEffectCode rhs) { return lhs = lhs | rhs; }
    inline KfActorEffectCode& operator&=(KfActorEffectCode& lhs, KfActorEffectCode rhs) { return lhs = lhs & rhs; }
    inline KfActorEffectCode& operator^=(KfActorEffectCode& lhs, KfActorEffectCode rhs) { return lhs = lhs ^ rhs; }

constexpr KfEffectKind actor_effect_kind_from_payload(KfActorEffectCode payload)
{
    return kf_enum_decode<KfEffectKind>(kf_enum_encode<u8>(payload));
}

enum { KF_ACTOR_EFFECT_PARAMETER_COUNT = 3 };

typedef struct KfActorActionParameters {
    KfActorEffectCode effect_codes[KF_ACTOR_EFFECT_PARAMETER_COUNT];
    u8 effect_chances[KF_ACTOR_EFFECT_PARAMETER_COUNT];
    KfObjectId drop_object;
    u8 drop_chance;
} KfActorActionParameters;

enum {
    KF_ACTOR_DAMAGE_SCALE_ONE = 5000
};

typedef struct KfActorDefinition {
    u8 pursuit_distance_scale;
    u8 model_and_texture;
    u8 melee_attack_chance;
    KfEnumStorage<KfPlayerStatusFlags, u8> status_effect;
    u8 status_effect_chance;
    KfActorActionParameters action_parameters;
    u8 move_speed;
    KfAnimationClip action_animations[KF_ACTOR_ANIM_SLOT_COUNT];
    u8 turn_rate;
    SoundRef sounds[KF_ACTOR_SOUND_COUNT];
    struct KfVec3s attachment_offsets[KF_ACTOR_ATTACHMENT_OFFSET_COUNT];
    s16 special_attack_chance;
    s16 special_attack_range;
    u8 unknown_38[2];
    u16 action_animation_steps[KF_ACTOR_ANIM_SLOT_COUNT];
    u16 action_animation_phases[KF_ACTOR_ANIM_SLOT_COUNT];
    u16 collision_radius;
    u16 collision_height;
    u16 awareness_distance;
    u16 initial_health;
    u16 effect_owner_id;
    u16 experience_reward;
    u16 attack_components[KF_COMBAT_PHYSICAL_COMPONENT_COUNT];
    u16 defenses[KF_COMBAT_COMPONENT_COUNT];
    u16 gold_drop_limit;
} KfActorDefinition;

typedef struct KfActorDefinitionTable {
    KfActorDefinition entries[KF_ACTOR_DEFINITION_COUNT];
} KfActorDefinitionTable;

typedef struct KfActorActionProfile {
    s16 far_distance;
    s16 far_weight;
    s16 near_distance;
    s16 middle_weight;
    s16 near_weight;
} KfActorActionProfile;

enum class KfActorHeadingQuadrant : u8 {
    KF_ACTOR_HEADING_0 = 0,
    KF_ACTOR_HEADING_90 = 1,
    KF_ACTOR_HEADING_180 = 2,
    KF_ACTOR_HEADING_270 = 3
}; using enum KfActorHeadingQuadrant;

typedef struct KfActor {
    KfActorSlotState slot_state;
    u8 definition_id;
    KfActorCullingMode culling_mode;
    KfActorHeadingQuadrant heading_quadrant;
    u8 tile_z;
    u8 tile_x;
    KfActorLifecycle lifecycle;
    u8 spawn_chance;
    KfActorAction action;
    KfObjectId death_drop_object_id;
    KfAnimationClip animation_clip;
    KfActorVerticalState vertical_state;
    std::array<u8, 2> unknown_0c;
    s16 local_z;
    s16 local_x;
    u16 animation_phase;
    u16 health;
    u16 cell_x;
    u16 cell_z;
    s16 unknown_1a;
    VECTOR position;
    KfRotation rotation;
    struct KfAnimationCacheRecord *animation_cache;
    KfActorActionProgress action_progress;
    KfActorCollisionState collision_state;
    s16 movement_yaw;
    s16 animation_step;
    s16 vertical_velocity;
    s16 movement_x;
    s16 movement_z;
    s16 movement_y;
    std::array<u8, 2> unknown_46;
} KfActor;

typedef struct KfActorState {
    KfActorDefinitionTable definitions;
    std::array<KfActor, KF_ACTOR_CAPACITY> actors;
    VECTOR player_position;
    SVECTOR player_rotation;
    KfActorDefinition *current_definition;
    KfActor *current;
    u16 current_index;
    u16 current_definition_id;
    KfActor *player_target;
} KfActorState;

extern std::array<KfActorActionProfile, KF_ACTOR_ACTION_PROFILE_COUNT> actor_action_profiles;
extern KfActorState actor_state;

extern s32 actor_bearing_to_player(const KfActor *actor);

extern SoundRef boss_death_loop_sound;
extern std::array<SoundRef, KF_ACTOR_BOSS_DEATH_SOUND_COUNT> boss_death_phase_sounds;

extern bool actor_animation_crossed_phase(const KfActor *actor, u16 phase);
extern void actor_advance_animation_clamped(KfActor *actor, s16 delta);
extern void actor_advance_animation_wrapped(KfActor *actor, s16 delta);
extern void actor_apply_horizontal_movement(void);
extern void actor_apply_random_movement(s16 step, s16 limit);
extern void actor_bind_current(KfActor *actor);
extern void actor_apply_damage(
    u16 actor_index, u16 base_power, u16 component0, u16 component1,
    u16 component2, u16 component3, u16 component4, u16 scale, KfEffectType hit_flags);
extern void actor_definitions_load(const KfActorDefinitionTable *definitions);
extern void actor_initialize(KfActor *actor);
extern void actor_initialize_current(void);
extern void actor_initialize_slot(u16 actor_index);
extern KfActorMoveResult actor_move_along_heading(KfActorMoveDirection direction, KfActorCollisionPolicy collision_policy);
extern KfActorMoveResult actor_move_xz_with_collision(const struct KfVecXZs *delta, KfActorCollisionPolicy collision_policy);
extern void actor_play_sound_at_phase(const SoundRef *sound, u16 phase);
extern void actor_pool_begin_death_by_definition(u16 definition_id);
extern void actor_pool_clear(void);
extern void actor_pool_apply_radial_damage(
    const VECTOR *origin, u32 radius, u16 falloff_q12, u16 base_power,
    u16 component0, u16 component1, u16 component2, u16 component3,
    u16 component4, u16 scale, KfEffectType hit_flags);
extern s32 actor_pool_find_at_tile(u8 tile_x, u8 tile_z);
extern KfActor *actor_pool_find_target_in_cone(
    const VECTOR *origin, s16 facing, u32 max_distance,
    s32 angle_tolerance, s32 *distance_out);
extern s32 actor_pool_find_overlap(s32 point_x, s32 point_y, s32 point_z, s32 radius_padding, s32 point_height);
extern void actor_pool_load_placements(KfResourceChunk placements);
extern void actor_pool_spawn(
    u8 definition_id, const VECTOR *position,
    const struct KfVec3s *rotation);
extern void actor_pool_update(void);
extern s32 actor_distance_to_point(
    const KfActor *actor, s32 point_x, s32 point_y, s32 point_z,
    s32 max_distance, s32 actor_height, s32 point_height);
extern void actor_prepare_charge_toward_player(void);
extern void actor_select_next_action(s32 player_distance);
extern void actor_set_action(KfActor *actor, KfActorAction action);
extern void actor_set_player_transform( const VECTOR *position_or_null, const SVECTOR *rotation_or_null);
extern KfActorAction actor_try_select_action_distance_facing(
    KfActorAction action, s32 distance, u16 chance, u16 distance_scale);
extern KfActorAction actor_try_select_multi_hit_action(
    KfActorAction action, s32 distance, u16 chance);
extern KfActorAction actor_try_select_ground_action(
    KfActorAction action, s32 distance, u16 chance);
extern KfActorAction actor_try_select_profiled_action(
    KfActorAction action, s32 distance, KfActorEffectCode effect_code, u16 chance);
extern void actor_try_attack_player(
    u16 minimum_distance, u16 maximum_distance,
    s16 angle_offset, s16 angle_tolerance);
extern void actor_update_awareness(void);
extern void actor_update_current_action(void);
extern void actor_update_effect_action(KfActorEffectSlot effect_slot);
extern void actor_transform_definition5_to6(KfActor *actor);

#endif // KF_GAME_ACTOR_H
