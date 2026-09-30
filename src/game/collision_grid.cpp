#include <kf/platform/prelude.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/lib/map_data.h>

s32 map_floor_height_for_cell_position(
    u16 cell_index, s32 point_x, s32 point_z)
{
    s32 floor_height = -(map_floor_height_grid.linear[cell_index] * KF_MAP_HEIGHT_STEP);

    if (map_collision_grid.linear[cell_index] == KF_MAP_CELL_STEP) {
        switch (map_cell_orientation_grid.linear[cell_index]) {
        case KF_MAP_ORIENT_UNROTATED:
            if (point_x % KF_MAP_TILE_SIZE > KF_MAP_TILE_CENTER) {
                floor_height += KF_MAP_HALF_CELL_STEP_HEIGHT;
            }
            break;
        case KF_MAP_ORIENT_QUARTER_TURN:
            if (point_z % KF_MAP_TILE_SIZE < KF_MAP_TILE_CENTER) {
                floor_height += KF_MAP_HALF_CELL_STEP_HEIGHT;
            }
            break;
        case KF_MAP_ORIENT_HALF_TURN:
            if (point_x % KF_MAP_TILE_SIZE < KF_MAP_TILE_CENTER) {
                floor_height += KF_MAP_HALF_CELL_STEP_HEIGHT;
            }
            break;
        case KF_MAP_ORIENT_THREE_QUARTER_TURN:
            if (point_z % KF_MAP_TILE_SIZE > KF_MAP_TILE_CENTER) {
                floor_height += KF_MAP_HALF_CELL_STEP_HEIGHT;
            }
            break;
        }
    }
    return floor_height;
}

s32 map_floor_height_at_position(const VECTOR *position)
{
    s32 point_z = position->vz;
    s32 point_x = position->vx;

    return map_floor_height_for_cell_position(
        (point_z / KF_MAP_TILE_SIZE) * KF_MAP_COLUMNS + point_x / KF_MAP_TILE_SIZE,
        point_x,
        point_z);
}

void collision_adjust_cell_occupancy(u16 cell_x, u16 cell_z, s32 delta)
{
    // The original unsigned bounds checks exclude off-map cells. Form each
    // pointer only after that check, including at maps' outer two rows/columns.
    const s32 first_x = static_cast<s32>(cell_x) - KF_OCCUPANCY_CELL_RADIUS;
    const s32 first_z = static_cast<s32>(cell_z) - KF_OCCUPANCY_CELL_RADIUS;
    for (s32 row = 0; row < KF_OCCUPANCY_CELL_SPAN; ++row) {
        const u16 z = static_cast<u16>(first_z + row);
        if (z >= KF_MAP_ROWS)
            continue;
        for (s32 column = 0; column < KF_OCCUPANCY_CELL_SPAN; ++column) {
            const u16 x = static_cast<u16>(first_x + column);
            if (x >= KF_MAP_COLUMNS)
                continue;
            u8 &cell = map_collision_flag_grid.cells[z][x];
            cell = (cell & KF_CELL_PRESERVED_FLAGS_MASK) | ((cell + delta) & KF_CELL_OCCUPANT_COUNT_MASK);
        }
    }
}
