#include <kf/platform/prelude.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/lib/map_data.h>

#include <array>

std::array<s16, KF_MAP_ATTRIBUTE_COUNT> map_cell_attribute_height_table = {
    -25000, -3000, -3000, -3000, -3000, -3000, -3000, -3000,
    -3000, -3000, -3000, -3000, -3000, -3000, -3000, -3000,
    -3000, -3000, -3000, -5000, -5000, -5000, -5000, -2500,
    -2500, -2500, -2500, -2500, -2500, -2500, -2500, -2500,
    -5000, -5000, -5000, -2500, -2500, -3000, -3000, -3000,
    -3000, -5000, -5000, -5000, -5000, -3000, -3000, -3000,
    -5000, -5000, -3000, -3000, -3000, 0, -5000, -5000,
    -5000, -3000, -3000, -5000, -3000, -5000, -3000, -3000,
    -5000, -3000, -3000, -3000, -3000, -3000, -3000, 4,
    5, 6, -25000, -5000, 1, 2, 3, -3000,
    -3000, -3000, -25000, -3000, -3000, -3000, -3000, -3000,
    -3000, -3000, -5000, -5000, -3000, -13000, -3000, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -25000,
};

KfCollisionTarget collision_target;

KfCollisionResult collision_query_world(
    s32 point_x, s32 point_y, s32 point_z, s32 radius, s32 height, u32 flags)
{
    s32 cell = point_x / KF_MAP_TILE_SIZE
        + (s16)(point_z / KF_MAP_TILE_SIZE) * KF_MAP_COLUMNS;
    s32 floor_height;
    s32 hit;
    KfMapAttribute attribute;
    u8 cell_flags;
    u32 rejection_mask;
    u16 query_flags = flags;

    if ((query_flags & KF_COLLISION_SKIP_TERRAIN) == 0) {
        hit = kf_enum_encode<u8>(map_collision_grid.linear[(u16)cell]);

        if (!map_cell_has_full_floor(kf_enum_decode<KfMapCellKind>(hit))) {
            return {KfCollisionKind::Terrain, static_cast<u16>(hit)};
        }
        if (point_y != KF_COLLISION_IGNORE_HEIGHT) {
            floor_height = map_floor_height_for_cell_position(
                (u16)cell, point_x, point_z);
            if (floor_height < point_y) {
                return {KfCollisionKind::BelowFloor};
            }
            attribute = map_cell_attribute_grid.linear[(u16)cell];
            if (attribute == KF_MAP_ATTRIBUTE_NONE) {
                return {KfCollisionKind::MissingAttribute};
            }
            if (map_cell_attribute_height_table[kf_enum_encode<u8>(attribute)] + height < 0
                && point_y < map_cell_attribute_height_table[kf_enum_encode<u8>(attribute)] + height + floor_height) {
                return {KfCollisionKind::Ceiling};
            }
        }
    }
    cell_flags = map_collision_flag_grid.linear[(u16)cell];
    rejection_mask = (query_flags >> KF_COLLISION_CELL_FLAG_SHIFT) & KF_COLLISION_CELL_FLAG_MASK;
    hit = cell_flags & rejection_mask;
    if (hit != 0) {
        return {KfCollisionKind::CellFlags, static_cast<u16>(hit)};
    }
    if ((cell_flags & KF_CELL_OCCUPANT_COUNT_MASK) == 0) {
        return {KfCollisionKind::None};
    }
    if ((query_flags & KF_COLLISION_SKIP_PLAYER) == 0) {
        hit = player_distance_to_point(
            point_x, point_y, point_z, radius + KF_COLLISION_PLAYER_RADIUS, height);
        if (hit != KF_PROXIMITY_NONE) {
            if (query_flags & KF_COLLISION_CAPTURE_TARGET) {
                collision_target.position = player_state.camera_position;
                collision_target.radius = KF_COLLISION_PLAYER_RADIUS;
            }
            return {KfCollisionKind::Player};
        }
    }
    if ((query_flags & KF_COLLISION_SKIP_ACTORS) == 0) {
        hit = actor_pool_find_overlap(point_x, point_y, point_z, radius, height);
        if (hit != KF_PROXIMITY_NONE) {
            if (query_flags & KF_COLLISION_CAPTURE_TARGET) {
                KfActor *actor = &actor_state.actors[hit];
                KfActorDefinition *definition = &actor_state.definitions.entries[actor->definition_id];

                collision_target.position = actor->position;
                collision_target.radius = definition->collision_radius;
            }
            return {KfCollisionKind::Actor, static_cast<u16>(hit)};
        }
    }
    if ((query_flags & KF_COLLISION_SKIP_MAP_OBJECTS) == 0) {
        hit = map_object_pool_find_near_point(point_x, point_z, radius);
        if (hit != KF_PROXIMITY_NONE) {
            if (query_flags & KF_COLLISION_CAPTURE_TARGET) {
                KfMapObject *object = &map_object_state.objects[hit];
                KfMapObjectDefinition *definition = &map_object_state.definitions.entries[kf_enum_encode<u8>(object->object_id)];

                collision_target.position = object->position;
                collision_target.radius = definition->collision_radius;
            }
            return {KfCollisionKind::MapObject, static_cast<u16>(hit)};
        }
    }
    if (query_flags & KF_COLLISION_SKIP_MAP_EVENTS) {
        return {KfCollisionKind::None};
    }
    hit = map_event_pool_find_overlap(point_x, point_z, radius);
    if (hit != KF_PROXIMITY_NONE) {
        if (query_flags & KF_COLLISION_CAPTURE_TARGET) {
            KfMapEvent *event = &map_runtime_state.events[hit];

            collision_target.position = event->reference_position;
            collision_target.radius = event->radius;
        }
        return {KfCollisionKind::MapEvent, static_cast<u16>(hit)};
    }
    return {KfCollisionKind::None};
}


void collision_reset_module_state(void)
{
    kf::restore_initial_value<map_cell_attribute_height_table>();
    kf::restore_initial_value<collision_target>();
}
