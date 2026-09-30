#ifndef KF_GAME_COLLISION_H
#define KF_GAME_COLLISION_H

#include <kf/lib/geometry_types.h>
#include <kf/lib/map_data.h>
#include <kf/lib/types.h>

#include <array>

// Original actors and wandering events reject cells carrying this authored bit.
inline constexpr int KF_COLLISION_CELL_BLOCKS_WANDER = 0x80;

enum {
    KF_COLLISION_SKIP_TERRAIN = 0x1,
    KF_COLLISION_SKIP_ACTORS = 0x10,
    KF_COLLISION_SKIP_MAP_OBJECTS = 0x20,
    KF_COLLISION_SKIP_MAP_EVENTS = 0x40,
    KF_COLLISION_SKIP_PLAYER = 0x80,
    KF_COLLISION_CAPTURE_TARGET = 0x800,
    KF_COLLISION_CELL_FLAG_SHIFT = 8,
    KF_COLLISION_CELL_FLAG_MASK = 0xf0,
    KF_COLLISION_IGNORE_HEIGHT = 0xffff,
    KF_COLLISION_PLAYER_RADIUS = 800,
    KF_COLLISION_PLAYER_HEIGHT = 1700
};

inline constexpr s32 KF_PROXIMITY_NONE = -1;

enum class KfCollisionKind : u16 {
    None,
    Terrain,
    BelowFloor,
    Ceiling,
    MissingAttribute,
    Actor,
    MapObject,
    MapEvent,
    Player,
    CellFlags,
    EffectWithoutTargets
};

struct KfCollisionResult {
    KfCollisionKind kind;
    u16 detail = 0;
};

struct KfCollisionTarget {
    VECTOR position;
    u16 radius;
};

enum { KF_MAP_CELL_HEIGHT_RECORD_COUNT = 7 };

typedef struct KfCellHeightRecord {
    s16 x_min;
    s16 y_min;
    s16 x_max;
    s16 y_max;
} KfCellHeightRecord;

extern KfCollisionTarget collision_target;
extern std::array<s16, KF_MAP_ATTRIBUTE_COUNT> map_cell_attribute_height_table;
extern std::array<KfCellHeightRecord, KF_MAP_CELL_HEIGHT_RECORD_COUNT> map_cell_height_records;

extern void collision_adjust_cell_occupancy(
    u16 cell_x, u16 cell_z, s32 delta);
extern KfCollisionResult collision_query_world(
    s32 point_x, s32 point_y, s32 point_z, s32 radius, s32 height,
    u32 flags);
extern s32 map_floor_height_at_position(const VECTOR *position);
extern s32 map_floor_height_for_cell_position(
    u16 cell_index, s32 point_x, s32 point_z);

#endif // KF_GAME_COLLISION_H
