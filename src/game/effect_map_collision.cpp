#include <kf/platform/prelude.h>
#include <kf/game/collision.h>
#include <kf/game/effect.h>
#include <kf/game/game.h>
#include <kf/lib/map_data.h>

#include <array>

std::array<KfCellHeightRecord, KF_MAP_CELL_HEIGHT_RECORD_COUNT> map_cell_height_records = {
    KfCellHeightRecord{500, -2000, 1500, -1250},
    KfCellHeightRecord{1000, -2500, 2000, -1000},
    KfCellHeightRecord{0, -2500, 2000, -1000},
    KfCellHeightRecord{0, -2500, 1000, -1000},
    KfCellHeightRecord{1000, -10000, 2000, -15000},
    KfCellHeightRecord{0, -10000, 2000, -15000},
    KfCellHeightRecord{0, -10000, 1000, -15000},
};

static KfCollisionResult effect_query_collision_targets(VECTOR *position, s32 radius, const KfEffectRecord *effect)
{
    switch (effect->type & KF_EFFECT_COLLISION_TARGETS_MASK) {
    default:
        return {KfCollisionKind::EffectWithoutTargets};
    case KF_EFFECT_COLLISION_TARGET_ACTORS:
        return collision_query_world(position->vx, position->vy, position->vz, radius, 0,
            KF_COLLISION_SKIP_TERRAIN | KF_COLLISION_SKIP_PLAYER
                | KF_COLLISION_SKIP_MAP_OBJECTS | KF_COLLISION_SKIP_MAP_EVENTS);
    case KF_EFFECT_COLLISION_TARGET_PLAYER:
        return collision_query_world(position->vx, position->vy, position->vz, radius, 0,
            KF_COLLISION_SKIP_TERRAIN | KF_COLLISION_SKIP_ACTORS
                | KF_COLLISION_SKIP_MAP_OBJECTS | KF_COLLISION_SKIP_MAP_EVENTS);
    case KF_EFFECT_COLLISION_TARGET_ACTORS_AND_PLAYER:
        return collision_query_world(position->vx, position->vy, position->vz, radius, 0,
            KF_COLLISION_SKIP_TERRAIN | KF_COLLISION_SKIP_MAP_OBJECTS
                | KF_COLLISION_SKIP_MAP_EVENTS);
    }
}

static bool effect_within_height_rectangle(const KfCellHeightRecord *record,
    KfMapOrientation orientation, s32 subx, s32 subz)
{
    s16 coordinate;
    switch (orientation) {
    case KF_MAP_ORIENT_UNROTATED:
        coordinate = subz;
        break;
    case KF_MAP_ORIENT_QUARTER_TURN:
        coordinate = subx;
        break;
    case KF_MAP_ORIENT_HALF_TURN:
        coordinate = KF_MAP_TILE_SIZE - subz;
        break;
    case KF_MAP_ORIENT_THREE_QUARTER_TURN:
        coordinate = KF_MAP_TILE_SIZE - subx;
        break;
    default:
        return false;
    }
    return coordinate >= record->x_min && coordinate <= record->x_max;
}

static KfCollisionResult effect_collision_in_cell(
    VECTOR *position, s32 radius, s16 x, s16 z, s32 subz,
    KfEffectRecord *effect)
{
    KfCellHeightRecord *record;
    s32 subx;
    s32 y;
    s32 floor;
    s32 height;
    KfMapAttribute attr;

    y = position->vy;
    floor = map_floor_height_grid.cells[z][x] * -KF_MAP_HEIGHT_STEP;
    if (floor < y) {
        return {KfCollisionKind::Terrain};
    }
    attr = map_cell_attribute_grid.cells[z][x];
    if (attr != KF_MAP_ATTRIBUTE_NONE) {
        height = map_cell_attribute_height_table[kf_enum_encode<u8>(attr)];
        if (height < 0) {
            height += floor;
            if (y < height) {
                return {KfCollisionKind::Terrain};
            }
        } else {
            record = &map_cell_height_records[height];
            if (floor + record->y_min <= y && y <= floor + record->y_max) {
                const KfMapOrientation orientation = map_cell_orientation_grid.cells[z][x];
                subx = position->vx % KF_MAP_TILE_SIZE;
                if (effect_within_height_rectangle(record, orientation, subx, subz)) {
                    return effect_query_collision_targets(position, radius, effect);
                }
            }
        }
    }

    switch (map_collision_grid.cells[z][x]) {
    case KF_MAP_CELL_FLOOR:
    case KF_MAP_CELL_STEP:
        // Full-floor cells need no diagonal/corner rejection.
        break;
    case KF_MAP_CELL_BLOCKED:
        if ((!map_cell_has_full_floor(map_collision_grid.cells[z + 1][x])
                || position->vz % KF_MAP_TILE_SIZE < KF_MAP_TILE_CENTER) &&
            (!map_cell_has_full_floor(map_collision_grid.cells[z - 1][x])
                || KF_MAP_TILE_CENTER < position->vz % KF_MAP_TILE_SIZE) &&
            (!map_cell_has_full_floor(map_collision_grid.cells[z][x + 1])
                || position->vx % KF_MAP_TILE_SIZE < KF_MAP_TILE_CENTER)) {
            if (!map_cell_has_full_floor(map_collision_grid.cells[z][x - 1])) {
                return {KfCollisionKind::Terrain};
            }
            if (KF_MAP_TILE_CENTER < position->vx % KF_MAP_TILE_SIZE) {
                return {KfCollisionKind::Terrain};
            }
        }
        break;
    case KF_MAP_CELL_X_GE_Z:
        if (position->vx % KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER
                >= position->vz % KF_MAP_TILE_SIZE) {
            break;
        }
        return {KfCollisionKind::Terrain};
    case KF_MAP_CELL_SUM_LE_SIZE:
        if ((KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER)
                < position->vx % KF_MAP_TILE_SIZE + position->vz % KF_MAP_TILE_SIZE) {
            return {KfCollisionKind::Terrain};
        }
        break;
    case KF_MAP_CELL_Z_GE_X:
        if (position->vx % KF_MAP_TILE_SIZE
                > position->vz % KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER) {
            return {KfCollisionKind::Terrain};
        }
        break;
    case KF_MAP_CELL_SUM_GE_SIZE:
        if (position->vx % KF_MAP_TILE_SIZE + position->vz % KF_MAP_TILE_SIZE
                < KF_MAP_TILE_CENTER) {
            return {KfCollisionKind::Terrain};
        }
        break;
    }

    return effect_query_collision_targets(position, radius, effect);
}

KfCollisionResult effect_map_collision(VECTOR *position, s32 radius)
{
    KfEffectRecord *effect;
    s16 x;
    s16 z;
    s32 subz;

    x = position->vx / KF_MAP_TILE_SIZE;
    z = position->vz / KF_MAP_TILE_SIZE;
    subz = position->vz % KF_MAP_TILE_SIZE;
    effect = effect_state.current_record;
    if (x < 0 || x >= KF_MAP_COLUMNS || z < 0 || z >= KF_MAP_ROWS) {
        return {KfCollisionKind::Terrain};
    }
    return effect_collision_in_cell(position, radius, x, z, subz, effect);
}


void effect_map_collision_reset_module_state(void)
{
    kf::restore_initial_value<map_cell_height_records>();
}
