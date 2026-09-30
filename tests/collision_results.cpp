#include "../src/game/collision.cpp"
#define map_object_pool_find_near_point original_map_object_pool_find_near_point
#include "../src/game/map_object_pool.cpp"
#undef map_object_pool_find_near_point
#include "../src/game/effect_map_collision.cpp"
#include <cassert>

KfMapCollisionGrid map_collision_grid {};
KfMapGrid map_collision_flag_grid {}, map_floor_height_grid {};
KfMapAttributeGrid map_cell_attribute_grid {};
KfMapOrientationGrid map_cell_orientation_grid {};
KfPlayerState player_state {};
KfActorState actor_state {};
KfMapRuntimeState map_runtime_state {};
KfEffectState effect_state {};
static s32 player_hit = -1, actor_hit = -1, object_hit = -1, event_hit = -1;
static s32 queried_x, queried_y, queried_z, queried_radius, queried_height;

s32 player_distance_to_point(s32 x, s32 y, s32 z, s32 radius, s32 height)
{
    queried_x = x; queried_y = y; queried_z = z;
    queried_radius = radius; queried_height = height;
    return player_hit;
}
s32 actor_pool_find_overlap(s32, s32, s32, s32, s32) { return actor_hit; }
s32 map_object_pool_find_near_point(s32, s32, s32) { return object_hit; }
s32 map_event_pool_find_overlap(s32, s32, s32) { return event_hit; }
s32 map_floor_height_for_cell_position(u16, s32, s32) { return 0; }
namespace kf {
[[noreturn]] void host_fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}
}

int main()
{
    constexpr s32 cell = 50 * KF_MAP_COLUMNS + 50;
    constexpr s32 coordinate = 50 * KF_MAP_TILE_SIZE;
    map_collision_grid.linear[cell] = KF_MAP_CELL_FLOOR;
    map_cell_attribute_grid.linear[cell] = kf_enum_decode<KfMapAttribute>(1);
    const auto query = [&](s32 y, u32 flags = 0) {
        return collision_query_world(coordinate, y, coordinate, 100, 0, flags);
    };
    assert(query(-1).kind == KfCollisionKind::None);
    assert(query(1).kind == KfCollisionKind::BelowFloor);
    assert(query(-4000).kind == KfCollisionKind::Ceiling);
    map_cell_attribute_grid.linear[cell] = KF_MAP_ATTRIBUTE_NONE;
    assert(query(-1).kind == KfCollisionKind::MissingAttribute);
    map_cell_attribute_grid.linear[cell] = kf_enum_decode<KfMapAttribute>(1);
    map_collision_grid.linear[cell] = KF_MAP_CELL_BLOCKED;
    assert(query(-1).kind == KfCollisionKind::Terrain);
    map_collision_grid.linear[cell] = KF_MAP_CELL_FLOOR;
    map_collision_flag_grid.linear[cell] = 0xa1;
    const auto rejection = query(-1, 0xa000);
    assert(rejection.kind == KfCollisionKind::CellFlags && rejection.detail == 0xa0);
    map_collision_flag_grid.linear[cell] = 1;
    player_hit = 0; actor_hit = 2; object_hit = 3; event_hit = 4;
    assert(query(-1).kind == KfCollisionKind::Player);
    const auto actor = query(-1, KF_COLLISION_SKIP_PLAYER);
    assert(actor.kind == KfCollisionKind::Actor && actor.detail == 2);
    const auto object = query(-1, KF_COLLISION_SKIP_PLAYER | KF_COLLISION_SKIP_ACTORS);
    assert(object.kind == KfCollisionKind::MapObject && object.detail == 3);
    const auto skip = KF_COLLISION_SKIP_PLAYER | KF_COLLISION_SKIP_ACTORS | KF_COLLISION_SKIP_MAP_OBJECTS;
    const auto event = query(-1, skip);
    assert(event.kind == KfCollisionKind::MapEvent && event.detail == 4);
    assert(query(-1, skip | KF_COLLISION_SKIP_MAP_EVENTS).kind == KfCollisionKind::None);
    player_state.camera_position = {1, 2, 3};
    query(-1, KF_COLLISION_CAPTURE_TARGET);
    assert(collision_target.position.vx == 1 && collision_target.radius == KF_COLLISION_PLAYER_RADIUS);
    actor_state.actors[2].position = {4, 5, 6};
    actor_state.definitions.entries[0].collision_radius = 123;
    query(-1, KF_COLLISION_SKIP_PLAYER | KF_COLLISION_CAPTURE_TARGET);
    assert(collision_target.position.vx == 4 && collision_target.radius == 123);

    KfEffectRecord effect {};
    effect_state.current_record = &effect;
    VECTOR point {coordinate, -1, coordinate};
    const auto fallback = effect_map_collision(&point, 100);
    assert(fallback.kind == KfCollisionKind::EffectWithoutTargets);
    effect.type = KF_EFFECT_COLLISION_TARGET_ACTORS;
    const auto effect_hit = effect_map_collision(&point, 100);
    assert(effect_hit.kind == KfCollisionKind::Actor && effect_hit.detail == 2);

    // Door probes skip terrain and map objects but still detect actors/events/player.
    auto &door = map_object_state.objects[0];
    door.object_id = kf_enum_decode<KfObjectId>(0);
    door.position = {coordinate, 0, coordinate};
    auto &definition = map_object_state.definitions.entries[0];
    definition.behavior_type = KF_MAP_OBJECT_OP_HINGED_DOOR;
    for (const u16 yaw : {0, 1024, 2048, 3072}) {
        const s32 x = coordinate + (yaw == 0 ? KF_MAP_TILE_SIZE : yaw == 2048 ? -KF_MAP_TILE_SIZE : 0);
        const s32 z = coordinate + (yaw == 1024 ? KF_MAP_TILE_SIZE : yaw == 3072 ? -KF_MAP_TILE_SIZE : 0);
        map_collision_flag_grid.cells[z / KF_MAP_TILE_SIZE][x / KF_MAP_TILE_SIZE] = 1;
        assert(map_object_probe_door_closing(&door, yaw).kind == KfCollisionKind::Player);
        assert(queried_x == x && queried_z == z && queried_y == KF_COLLISION_IGNORE_HEIGHT);
        assert(queried_radius == 3000 + KF_COLLISION_PLAYER_RADIUS && queried_height == 0);
    }
    definition.behavior_type = KF_MAP_OBJECT_OP_LIFT_DOOR;
    player_hit = -1;
    assert(map_object_probe_door_closing(&door, 123).kind == KfCollisionKind::Actor);
    assert(queried_x == coordinate && queried_z == coordinate);
    actor_hit = -1;
    assert(map_object_probe_door_closing(&door, 0).kind == KfCollisionKind::MapEvent);
    event_hit = -1;
    assert(map_object_probe_door_closing(&door, 0).kind == KfCollisionKind::None);
}
