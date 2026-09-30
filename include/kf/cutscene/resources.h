#ifndef KF_OPEN_RESOURCES_H
#define KF_OPEN_RESOURCES_H

#include <kf/lib/memory.h>
#include <kf/lib/resources.h>

#include <array>
#include <span>

extern KfMemoryArena cutscene_memory_arena;

#include <kf/lib/map.h>

extern KfMapAttributeGrid cutscene_map_cell_attribute_grid;
extern KfMapOrientationGrid cutscene_map_cell_orientation_grid;
extern KfMapCollisionGrid cutscene_map_collision_grid;
extern KfMapGrid cutscene_map_floor_height_grid;

enum {
    KF_OPENING_ENTITY_CAPACITY = 32,
    KF_OPENING_ENTITY_FLOOR_HEIGHT = 0,
    KF_OPENING_SCENE_BASE_Y = -10000
};

enum class KfOpeningModelId : u8 {
    KF_OPENING_SCENE0_DECREASING_YAW_MODEL = 11,
    KF_OPENING_SCENE0_INCREASING_YAW_MODEL = 12,
    KF_OPENING_SCENE3_INCREASING_YAW_MODEL = 13,
    KF_OPENING_SCENE3_DECREASING_YAW_MODEL = 14,
    KF_OPENING_TRANSITION_CYLINDER = 19,
    KF_OPENING_TAPERED_COLUMN = 20,
    KF_OPENING_GREEN_CRYSTAL_INCREASING_YAW = 21,
    KF_OPENING_PINK_CRYSTAL_INCREASING_YAW = 22,
    KF_OPENING_GREEN_CRYSTAL_DECREASING_YAW = 23,
    KF_OPENING_PINK_CRYSTAL_DECREASING_YAW = 24,
    KF_OPENING_CASTLE_MOUNTAIN_BACKDROP = 25,
    KF_OPENING_ENDING_ORANGE_DISK = 26,
    KF_OPENING_ENDING_STARFIELD = 27,
    KF_OPENING_ENTITY_MODEL_LIMIT = 32,
    KF_OPENING_ENTITY_FREE = 0xff
}; using enum KfOpeningModelId;

typedef struct KfOpeningEntity {
    KfOpeningModelId object_id;
    u8 unknown_01;
    u16 cell_x;
    u16 cell_z;
    std::array<u8, 2> unknown_06;
    VECTOR position;
    struct KfEulerAngles rotation;
    u16 unknown_1e;
    SVECTOR scale;
} KfOpeningEntity;

typedef struct KfOpeningEntityState {
    std::array<KfOpeningEntity, KF_OPENING_ENTITY_CAPACITY> entities;
    std::array<u8, 10> unknown_500;
    u16 unknown_control_50a;
    u16 unknown_control_50c;
    u16 unknown_control_50e;
} KfOpeningEntityState;

extern KfOpeningEntityState opening_entity_state;

extern void opening_entity_pool_reset(void);
extern KfOpeningEntity *opening_entity_find_by_object_id(
    std::span<KfOpeningEntity> entities, KfOpeningModelId object_id);
extern void opening_entity_pool_load_placements(
    KfResourceChunk placements, s32 base_y);

extern void opening_resources_load_scene0(void);
extern void opening_resources_load_scene1(void);
extern void opening_resources_load_scene3(void);
extern void opening_resources_load_ending(void);
extern void opening_resources_load_ending_entities(void);
extern void opening_resources_load_ending_sequence(void);

#endif // KF_OPEN_RESOURCES_H
