// PC extension: publishes the current floor for the automap (kf/platform/automap.hpp).
#include <kf/game/game.h>
#include <kf/platform/automap.hpp>

namespace {
constexpr float tile_size = KF_MAP_TILE_SIZE;

struct WarpLink { u8 x, z, floor_a, floor_b; };
// Floor changes keep the cell, so each link sits on the same cell on both floors
// (player_warp_trigger_update). floor_b 0 marks a same-floor teleport pair.
constexpr WarpLink warp_links[] = {
    {29, 56, 1, 2}, {25, 11, 1, 3}, {39, 35, 1, 4}, {28, 18, 2, 3},
    {7, 22, 3, 4}, {43, 92, 3, 4}, {39, 69, 4, 5}
};
constexpr u8 floor1_exit_x = 15, floor1_exit_z = 2;

u8 warp_label(int floor, int x, int z)
{
    for (const auto &link : warp_links) {
        if (link.x != x || link.z != z)
            continue;
        if (link.floor_a == floor)
            return link.floor_b;
        if (link.floor_b == floor)
            return link.floor_a;
    }
    if (floor == 1 && x == floor1_exit_x && z == floor1_exit_z)
        return kf::automap_warp_exit;
    return kf::automap_warp_teleport;
}

kf::AutomapCell cell_kind(KfMapCellKind kind, KfMapAttribute attribute)
{
    switch (kind) {
    case KF_MAP_CELL_BLOCKED:
        return attribute == KF_MAP_ATTRIBUTE_NONE ? kf::AutomapCell::Void : kf::AutomapCell::Wall;
    case KF_MAP_CELL_X_GE_Z: return kf::AutomapCell::HalfXGeZ;
    case KF_MAP_CELL_SUM_LE_SIZE: return kf::AutomapCell::HalfSumLe;
    case KF_MAP_CELL_Z_GE_X: return kf::AutomapCell::HalfZGeX;
    case KF_MAP_CELL_SUM_GE_SIZE: return kf::AutomapCell::HalfSumGe;
    default: return kf::AutomapCell::Floor;
    }
}

// Doors swing or lift while animating, so remember each door's closed yaw.
u16 door_closed_yaw[KF_MAP_OBJECT_CAPACITY];

bool all_empty(const KfObjectId *ids)
{
    for (int i = 0; i < KF_MAP_CONTAINER_ITEM_COUNT; ++i)
        if (ids[i] != KF_OBJECT_NONE)
            return false;
    return true;
}
}

void automap_publish(void)
{
    auto &snapshot = kf::automap_snapshot();
    const int floor = kf_enum_encode<u8>(player_state.progress_state.current_floor);
    snapshot.serial++;
    if (floor < 1 || floor > kf::automap_floor_count) {
        snapshot.floor = 0;
        return;
    }
    snapshot.floor = floor;
    const auto *stock = item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)];
    snapshot.owns_map = stock[kf_enum_encode<u8>(KF_ITEM_WATCHMAN_MAP)] != 0
        || stock[kf_enum_encode<u8>(KF_ITEM_SORCERER_MAP)] != 0;
    snapshot.player_x = static_cast<float>(player_state.camera_position.vx) / tile_size;
    snapshot.player_z = static_cast<float>(player_state.camera_position.vz) / tile_size;
    snapshot.player_yaw = static_cast<float>(player_state.camera_rotation.vy & KF_ANGLE_WRAP_MASK)
        * (6.2831853f / (KF_ANGLE_WRAP_MASK + 1));

    snapshot.marker_count = 0;
    auto add = [&](kf::AutomapMarkerKind kind, float x, float z, u8 label = 0, const KfObjectId *items = nullptr,
                   int item_count = 0) -> void {
        if (snapshot.marker_count >= kf::automap_marker_capacity)
            return;
        kf::AutomapMarker marker{kind, label, {kf::automap_no_item, kf::automap_no_item, kf::automap_no_item, kf::automap_no_item}, x, z};
        for (int i = 0; i < item_count && i < 4; ++i)
            marker.items[i] = kf_enum_encode<u8>(items[i]);
        snapshot.markers[snapshot.marker_count++] = marker;
    };

    for (int i = 0; i < KF_MAP_ROWS * KF_MAP_COLUMNS; ++i) {
        const auto attribute = map_cell_attribute_grid.linear[i];
        snapshot.cells[i] = cell_kind(map_collision_grid.linear[i], attribute);
        auto hazard = kf::AutomapHazard::None;
        if (attribute == KF_MAP_ATTRIBUTE_PITFALL)
            hazard = kf::AutomapHazard::Pit;
        else if (attribute == KF_MAP_ATTRIBUTE_POISON_HOLE)
            hazard = kf::AutomapHazard::Poison;
        else if (attribute == KF_MAP_ATTRIBUTE_BOTTOMLESS_PIT)
            hazard = kf::AutomapHazard::Bottomless;
        snapshot.hazards[i] = hazard;
        if (attribute == KF_MAP_ATTRIBUTE_WARP) {
            const int x = i % KF_MAP_COLUMNS, z = i / KF_MAP_COLUMNS;
            add(kf::AutomapMarkerKind::Warp, x + 0.5f, z + 0.5f, warp_label(floor, x, z));
        }
    }

    for (int slot = 0; slot < KF_MAP_OBJECT_CAPACITY; ++slot) {
        const auto &object = map_object_state.objects[slot];
        if (object.object_id == KF_OBJECT_NONE)
            continue;
        const auto behavior = map_object_state.definitions.entries[kf_enum_encode<u8>(object.object_id)].behavior_type;
        const float x = static_cast<float>(object.position.vx) / tile_size;
        const float z = static_cast<float>(object.position.vz) / tile_size;
        switch (behavior) {
        case KF_MAP_OBJECT_OP_HINGED_DOOR:
        case KF_MAP_OBJECT_OP_LIFT_DOOR:
        case KF_MAP_OBJECT_OP_03: {
            if (object.action == KF_MAP_OBJECT_OP_NONE)
                door_closed_yaw[slot] = static_cast<u16>(object.rotation.angles.y);
            // Snap to the nearest quarter turn, as the collision marking expects.
            const int yaw = ((door_closed_yaw[slot] + KF_ANGLE_EIGHTH_TURN) & KF_ANGLE_WRAP_MASK) & ~(KF_ANGLE_QUARTER_TURN - 1);
            int cx = object.cell_x, cz = object.cell_z;
            int ax = cx, az = cz, bx = cx, bz = cz;
            if (behavior == KF_MAP_OBJECT_OP_HINGED_DOOR) {
                switch (yaw) {
                case 0: ax = bx = cx + 1; bz = cz - 1; break;
                case KF_ANGLE_QUARTER_TURN: az = bz = cz + 1; bx = cx + 1; break;
                case KF_ANGLE_HALF_TURN: ax = bx = cx - 1; bz = cz + 1; break;
                default: az = bz = cz - 1; bx = cx - 1; break;
                }
            } else {
                switch (yaw) {
                case 0: bz = cz + 1; break;
                case KF_ANGLE_QUARTER_TURN: bx = cx + 1; break;
                case KF_ANGLE_HALF_TURN: bz = cz - 1; break;
                default: bx = cx - 1; break;
                }
            }
            bool placed = false;
            for (const auto [dx, dz] : {std::pair{ax, az}, std::pair{bx, bz}}) {
                if (dx < 0 || dz < 0 || dx >= KF_MAP_COLUMNS || dz >= KF_MAP_ROWS)
                    continue;
                const int i = dz * KF_MAP_COLUMNS + dx;
                snapshot.cells[i] = map_collision_grid.linear[i] == KF_MAP_CELL_BLOCKED
                    ? kf::AutomapCell::Door : kf::AutomapCell::DoorOpen;
                placed = true;
            }
            if (placed) {
                const u8 link = object.link.fields.link_id;
                add(link != KF_MAP_LINK_NONE ? kf::AutomapMarkerKind::LockedDoor : kf::AutomapMarkerKind::Door,
                    (ax + bx) * 0.5f + 0.5f, (az + bz) * 0.5f + 0.5f, link);
            }
            break;
        }
        case KF_MAP_OBJECT_OP_SAVE_POINT:
            add(kf::AutomapMarkerKind::SavePoint, x, z);
            break;
        case KF_MAP_OBJECT_OP_RESTORE_POINT:
            add(kf::AutomapMarkerKind::Fountain, x, z, object.link.fields.link_id == KF_MAP_LINK_NONE);
            break;
        case KF_MAP_OBJECT_OP_HINGED_CONTAINER:
            if (!all_empty(object.link.hinged_container.item_ids))
                add(kf::AutomapMarkerKind::Chest, x, z, object.link.hinged_container.link_id,
                    object.link.hinged_container.item_ids, KF_MAP_CONTAINER_ITEM_COUNT);
            break;
        case KF_MAP_OBJECT_OP_ITEM_CONTAINER:
            if (!all_empty(object.link.item_ids))
                add(kf::AutomapMarkerKind::Chest, x, z, kf::automap_unlocked, object.link.item_ids, KF_MAP_CONTAINER_ITEM_COUNT);
            break;
        case KF_MAP_OBJECT_OP_ITEM_PICKUP:
        case KF_MAP_OBJECT_OP_GOLD_PICKUP:
            add(kf::AutomapMarkerKind::Item, x, z, 0, &object.object_id, 1);
            break;
        default:
            break;
        }
    }

    for (const auto &event : map_runtime_state.events) {
        if (event.state == KF_MAP_EVENT_ACTIVE)
            add(kf::AutomapMarkerKind::Npc, static_cast<float>(event.position_x) / tile_size,
                static_cast<float>(event.position_z) / tile_size);
    }
    for (const auto &actor : actor_state.actors) {
        if (actor.slot_state == KF_ACTOR_SLOT_FREE || actor.lifecycle != KF_ACTOR_LIFECYCLE_ACTIVE
                || actor.action == KF_ACTOR_ACTION_DYING || actor.action == KF_ACTOR_ACTION_POST_DEATH)
            continue;
        add(kf::AutomapMarkerKind::Enemy, static_cast<float>(actor.position.vx) / tile_size,
            static_cast<float>(actor.position.vz) / tile_size);
    }
    kf::automap_published();
}
