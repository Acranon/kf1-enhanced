#ifndef KF_LIB_MAP_H
#define KF_LIB_MAP_H

#include <kf/lib/animation.h>
#include <kf/lib/enum.h>
#include <kf/lib/floor.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/item.h>
#include <kf/lib/map_data.h>
#include <kf/lib/map_object_types.h>
#include <kf/lib/math.h>
#include <kf/lib/notify_types.h>
#include <kf/lib/types.h>

#include <array>
#include <span>

struct KfAnimationCacheRecord;
struct KfCollisionResult;
struct KfResourceChunk;

enum {
    MAP_INTERACTION_PROBE_DISTANCE = 1000,
    MAP_INTERACTION_RADIUS_PADDING = 800,
    MAP_DOOR_FACING_TOLERANCE = KF_ANGLE_FULL_TURN / 12
};

enum {
    KF_MAP_SAVED_FLOOR_COUNT = 5,
    KF_MAP_SAVED_FLOOR_BYTES = 1700,
    KF_MAP_SAVED_RECORDS_OFFSET = 10,
    KF_MAP_SAVED_RECORD_BYTES = 1690,
    KF_MAP_SAVED_WORLD_WORDS = 2125,
    KF_MAP_SAVED_WORLD_BYTES = 8500,
    KF_MAP_SAVED_YAW_SHIFT = 4,
    KF_MAP_FLOOR3_REQUIRED_REVEALS = 4
};

enum class KfMapScriptFlag : u8 {
    KF_MAP_SCRIPT_UNSET = 0,
    KF_MAP_SCRIPT_SET = 1
}; using enum KfMapScriptFlag;

enum class KfMapAreaTriggerStage : u8 {
    KF_MAP_TRIGGER_AWAIT_ENTRY = 0,
    KF_MAP_TRIGGER_AWAIT_EXIT = 1,
    KF_MAP_TRIGGER_COMPLETE = 2
}; using enum KfMapAreaTriggerStage;

typedef struct KfMapFloor1Script {
    KfMapAreaTriggerStage object_removal_stage;
    KfMapAreaTriggerStage actor_activation_stage;
    KfMapScriptFlag passage_opened;
    KfMapScriptFlag revival_enabled;
} KfMapFloor1Script;

typedef struct KfMapFloor3Script {
    u8 revealed_piece_count;
} KfMapFloor3Script;

typedef struct KfMapFloor5Script {
    KfMapScriptFlag character_arrived;
    KfMapScriptFlag weapon_transformed;
    KfMapScriptFlag boss_encounter_started;
    KfMapScriptFlag boss_defeat;
} KfMapFloor5Script;

typedef union KfMapFloorScript {
    u8 bytes[KF_MAP_SAVED_RECORDS_OFFSET];
    KfMapFloor1Script floor1;
    KfMapFloor3Script floor3;
    KfMapFloor5Script floor5;
} KfMapFloorScript;

typedef struct KfMapSavedFloor {
    KfMapFloorScript script;
    std::array<u8, KF_MAP_SAVED_RECORD_BYTES> records;
} KfMapSavedFloor;

typedef struct KfMapSavedWorld {
    std::array<KfMapSavedFloor, KF_MAP_SAVED_FLOOR_COUNT> floors;
} KfMapSavedWorld;

enum {
    KF_MAP_LINK_BOSS_EMITTERS = 13,
    KF_MAP_LINK_WEAPON_TRANSFORM_DOORS = 51,
    KF_MAP_LINK_FLOOR5_SWORD_DOOR = 52
};

enum class KfMapCopyRegionId : u8 {
    KF_MAP_COPY_FLOOR1_GRAVESTONE = 0,
    KF_MAP_COPY_FLOOR1_PASSAGE = 1,
    KF_MAP_COPY_FLOOR3_REVEAL_FIRST = 2,
    KF_MAP_COPY_FLOOR3_REVEAL_SECOND = 3,
    KF_MAP_COPY_FLOOR5_BOSS_ENCOUNTER = 4,
    KF_MAP_COPY_REGION_NONE = 255
}; using enum KfMapCopyRegionId;

enum {
    KF_MAP_COPY_REGION_COUNT = 5,
    KF_MAP_LINK_NONE = 255,
    KF_MAP_OBJECT_PARAMETER_NONE = 255,
    KF_MAP_LINK_REUSABLE_FIRST = 128,
    KF_MAP_OBJECT_EFFECT_GROUP_CAPACITY = 10,
    KF_MAP_OBJECT_GOLD_DROP_FIRST = 160,
    KF_MAP_OBJECT_DEFINITION_DROP_FIRST = 170,
    KF_MAP_OBJECT_PLACEMENT_DROP_FIRST = 180
};

enum class KfMapObjectProgress : u16 {
    KF_MAP_OBJECT_PROGRESS_INIT = 0,
    KF_MAP_OBJECT_PROGRESS_RUNNING = 1,
    KF_MAP_OBJECT_SWING_OPEN_LAST = 31,
    KF_MAP_OBJECT_SWING_OPEN_END = 32,
    KF_MAP_OBJECT_LIFT_OPEN_LAST = 40,
    KF_MAP_OBJECT_LIFT_OPEN_END = 41,
    KF_MAP_OBJECT_DOOR_HOLD_FIRST = 250,
    KF_MAP_OBJECT_DOOR_CLOSE_FIRST = 300,
    KF_MAP_OBJECT_SWING_CLOSE_END = 332,
    KF_MAP_OBJECT_LIFT_CLOSE_END = 341,
    KF_MAP_OBJECT_REVEAL_SETTLE_END = 6,
    KF_MAP_OBJECT_SWITCH_READY = 0,
    KF_MAP_OBJECT_SWITCH_FORWARD = 1,
    KF_MAP_OBJECT_SWITCH_DISABLED = 2,
    KF_MAP_OBJECT_SWITCH_REVERSE = 3
}; using enum KfMapObjectProgress;
inline KfMapObjectProgress& operator++(KfMapObjectProgress& value)
    { value = static_cast<KfMapObjectProgress>(static_cast<u16>(value) + 1); return value; }
    inline KfMapObjectProgress operator++(KfMapObjectProgress& value, int)
    { KfMapObjectProgress previous = value; ++value; return previous; }
    inline KfMapObjectProgress& operator--(KfMapObjectProgress& value)
    { value = static_cast<KfMapObjectProgress>(static_cast<u16>(value) - 1); return value; }
    inline KfMapObjectProgress operator--(KfMapObjectProgress& value, int)
    { KfMapObjectProgress previous = value; --value; return previous; }

constexpr KfMapObjectProgress map_object_toggle_progress(KfMapObjectProgress progress)
{
    return progress == KF_MAP_OBJECT_PROGRESS_INIT
        ? KF_MAP_OBJECT_PROGRESS_RUNNING : KF_MAP_OBJECT_PROGRESS_INIT;
}

enum class KfMapObjectDropSource : u8 {
    KF_MAP_OBJECT_DROP_FROM_PLACEMENT = 0,
    KF_MAP_OBJECT_DROP_FROM_DEFINITION = 1
}; using enum KfMapObjectDropSource;

enum {
    KF_MAP_OBJECT_REVEAL_DEPTH = 10000,
    KF_MAP_OBJECT_REVEAL_SETTLE_STEP = 40
};

typedef struct KfMapCellCoordinates {
    u8 z;
    u8 x;
} KfMapCellCoordinates;

constexpr bool map_cells_equal(KfMapCellCoordinates left, KfMapCellCoordinates right)
{
    return left.x == right.x && left.z == right.z;
}

typedef struct KfMapCopyRegion {
    u8 source_x;
    u8 source_z;
    u8 destination_x;
    u8 destination_z;
    u8 width;
    u8 height;
} KfMapCopyRegion;

typedef union KfMapObjectSpawn {
    u16 sequence;
    u8 effect_id;
} KfMapObjectSpawn;

typedef union KfMapObjectParameter {
    u8 effect_index;
    u8 object_index;
    KfMapCopyRegionId copy_region;
} KfMapObjectParameter;

typedef struct KfMapObjectLinkFields {
    u8 link_id;
    KfMapObjectParameter action_parameter;
    KfMapObjectSpawn spawn;
    s16 vertical_velocity;
    KfNotificationId linked_notification;
    KfNotificationId default_notification;
} KfMapObjectLinkFields;

typedef struct KfMapObjectHingedContainer {
    u8 link_id;
    KfObjectId item_ids[KF_MAP_CONTAINER_ITEM_COUNT];
} KfMapObjectHingedContainer;

typedef union KfMapObjectLink {
    KfMapObjectLinkFields fields;
    u16 gold_amount;
    KfMapObjectHingedContainer hinged_container;
    KfObjectId item_ids[KF_MAP_CONTAINER_ITEM_COUNT];
    u32 words[2];
} KfMapObjectLink;

typedef struct KfMapObject {
    KfObjectId object_id;
    u8 unknown_01;
    u16 cell_x;
    u16 cell_z;
    std::array<u8, 2> unknown_06;
    VECTOR position;
    KfRotation rotation;
    KfMapObjectLink link;
    KfMapObjectOperation action;
    u8 unknown_29;
    KfMapObjectProgress action_timer;
} KfMapObject;

#include <kf/lib/camera_path.h>

enum class KfCharacterId : u8 {
    KF_CHARACTER_KEY_OF_THE_DEAD_EXCHANGE = 3,
    KF_CHARACTER_HARP_EXCHANGE = 7,
    KF_CHARACTER_HEALING_EXCHANGE = 8,
    KF_CHARACTER_FLOOR3_DOOR_UNLOCKER = 12
}; using enum KfCharacterId;

enum class KfMapEventState : u8 {
    KF_MAP_EVENT_INACTIVE = 0,
    KF_MAP_EVENT_ACTIVE = 1,
    KF_MAP_EVENT_DISABLED = 3,
    KF_MAP_EVENT_FREE = 255
}; using enum KfMapEventState;

enum class KfMapEventBehavior : u8 {
    KF_MAP_EVENT_BEHAVIOR_SHOP = 0,
    KF_MAP_EVENT_BEHAVIOR_WANDER = 1,
    KF_MAP_EVENT_BEHAVIOR_ANIMATION_LOOP = 2
}; using enum KfMapEventBehavior;

enum class KfMapEventCollisionTurn : u8 {
    KF_MAP_EVENT_COLLISION_TURN_NONE = 0,
    KF_MAP_EVENT_COLLISION_TURN_PENDING = 1
}; using enum KfMapEventCollisionTurn;

enum {
    KF_DIALOGUE_STAGE_COUNT = 5,
    KF_DIALOGUE_FIRST_STAGE = 1,
    KF_DIALOGUE_FIRST_PAGE = 1,
    KF_DIALOGUE_GATE_RELOAD = 3,
    KF_DIALOGUE_PAGE_DELAY_TICKS = 40,
    KF_MAP_EVENT_ANIMATION_PHASE_MASK = 4095,
    KF_MAP_EVENT_ANIMATION_TALK_POSE = 2048,
    KF_MAP_EVENT_ANIMATION_WANDER_STEP = 110,
    KF_MAP_EVENT_ANIMATION_LOOP_STEP = 200,
    KF_MAP_EVENT_ANIMATION_TALK_STEP = 200,
    KF_MAP_EVENT_ANIMATION_FINISH_STEP = 400
};

typedef struct KfDialoguePageLimits {
    u8 last_page[KF_DIALOGUE_STAGE_COUNT];
} KfDialoguePageLimits;

typedef struct KfDialogueState {
    u8 stage_limit;
    u8 stage;
    u8 page;
    u8 page_delay;
} KfDialogueState;

typedef struct KfMapEvent {
    KfMapEventState state;
    KfCharacterId character_id;
    u8 model_index;
    KfDialoguePageLimits dialogue_pages;
    KfDialogueState dialogue;
    u8 unknown_0c;
    u8 unknown_0d;
    KfMapEventBehavior behavior;
    KfAnimationClip animation_clip;
    KfMapEventCollisionTurn collision_turn_pending;
    u8 unknown_11;
    u16 animation_phase;
    s32 home_x;
    s32 home_z;
    u16 cell_x;
    u16 cell_z;
    u16 radius;
    u16 unknown_22;
    VECTOR reference_position;
    SVECTOR rotation;
    struct KfAnimationCacheRecord *animation_cache;
    s16 rotation_target;
    u16 unknown_42;
} KfMapEvent;

typedef struct KfMapObjectState {
    KfMapObjectDefinitionTable definitions;
    std::array<KfMapObject, KF_MAP_OBJECT_CAPACITY> objects;
    std::array<u8, 10> unknown_25a8;
    u16 gold_drop_sequence;
    u16 definition_drop_sequence;
    u16 placement_drop_sequence;
} KfMapObjectState;

typedef struct KfMapRuntimeState {
    std::array<KfMapEvent, KF_MAP_EVENT_CAPACITY> events;
    KfMapEvent *current_event;
    u8 *variant_asset_buffer;
    u16 dialogue_advance_gate;
    u16 ambient_script_countdown;
    KfMapSavedWorld world_state;
} KfMapRuntimeState;

extern std::array<KfMapCopyRegion, KF_MAP_COPY_REGION_COUNT> map_copy_regions;
extern KfMapRuntimeState map_runtime_state;

inline KfMapFloorScript &map_floor_script(KfFloorId floor)
{
    return map_runtime_state.world_state.floors[kf_enum_encode<u8>(floor) - 1].script;
}

extern KfMapObjectState map_object_state;
extern std::array<char, KF_MAP_RESOURCE_PATH_BYTES> map_resource_path;

extern void camera_path_begin(KfCameraPathState *path, std::span<const KfCameraPathPoint> points);
extern void camera_path_compute_segment(KfCameraPathState *path);
extern void camera_path_step(KfCameraPathState *path, s32 y_offset);
extern void map_apply_copy_region(KfMapCopyRegionId region_id);
extern void map_ambient_script_floor1(void);
extern void map_ambient_script_floor2(void);
extern void map_ambient_script_floor3(void);
extern void map_ambient_script_floor4(void);
extern void map_ambient_script_floor5(void);
extern void map_action_script_floor1(void);
extern void map_action_script_floor2(void);
extern void map_action_script_floor3(void);
extern void map_action_script_floor4(void);
extern void map_action_script_floor5(void);
extern void map_event_advance_animation_blocking(KfMapEvent *event, u16 target, s16 step);
extern s32 map_event_distance_to_point( const KfMapEvent *event, s32 point_x, s32 point_z, s32 max_distance);
extern s32 map_event_pool_find_overlap(s32 point_x, s32 point_z, s32 radius_padding);
extern KfMapEvent *map_event_pool_find_target_in_cone( const VECTOR *origin, s16 facing, s32 max_distance, s32 angle_tolerance, s32 *distance_out);
extern void map_event_pool_load(KfResourceChunk placements);
extern void map_event_pool_update(void);
extern void map_event_refresh_dialogue_stage(KfMapEvent *event);
extern void map_event_set_current(KfMapEvent *event);
extern void map_event_timers_reset(void);
extern void map_interaction_dispatch(
    const VECTOR *position, SVECTOR *rotation);
extern void map_load_floor_wrapper(void);
extern void map_load_floor(void);
extern void map_object_definitions_load(const KfMapObjectDefinitionTable *definitions);
extern s32 map_object_distance_to_point( const KfMapObject *object, s32 point_x, s32 point_z, s32 max_distance);
extern KfMapObject *map_object_effect_pool_acquire(u16 first_index, u16 count, u16 sequence);
extern void map_object_mark_collision_edge(const KfMapObject *object, KfMapCellKind cell_kind, u16 yaw);
extern void map_object_pool_clear(void);
extern void map_object_pool_clear_link(u8 link_id);
extern s32 map_object_pool_find_interaction_from(
    s32 start_index, s32 point_x, s32 point_z, s32 radius_padding);
extern s32 map_object_pool_find_near_point(s32 point_x, s32 point_z, s32 radius_padding);
extern void map_object_pool_load(KfResourceChunk placements);
extern void map_object_pool_trigger_link(u8 link_id);
extern void map_object_pool_update(void);
extern KfCollisionResult map_object_probe_door_closing(const KfMapObject *object, u16 yaw);
extern void map_object_spawn_gold_drop(u16 gold_amount, const VECTOR *position, s32 y_offset);
extern void map_object_spawn_drop(KfMapObjectDropSource drop_source, KfObjectId object_id, const VECTOR *position, s32 y_offset);
extern void map_object_start_action_if_idle(KfMapObject *object, KfMapObjectOperation action);

extern u8 *map_resource_load_file(const char *filename, std::size_t *loaded_size = nullptr);
extern void map_resource_path_set_floor(KfFloorId floor);
extern void map_resources_load(KfFloorId floor, KfMapVariant map_variant);
extern void map_unload_floor(void);
extern void map_variant_assets_load(void);
extern void map_world_state_persist(void);
extern bool map_saved_link_valid(KfMapObjectOperation operation, const KfMapObjectLink &link);
extern s32 map_floor1_cross_index(void);

#endif // KF_LIB_MAP_H
