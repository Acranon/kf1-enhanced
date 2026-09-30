#ifndef KF_MAP_DATA_H
#define KF_MAP_DATA_H

#include <kf/lib/map_types.h>

extern KfMapAttributeGrid map_cell_attribute_grid;
extern KfMapOrientationGrid map_cell_orientation_grid;
extern KfMapGrid map_collision_flag_grid;
extern KfMapCollisionGrid map_collision_grid;
extern KfMapGrid map_floor_height_grid;

inline s32 map_base_floor_height(s32 x, s32 z)
{
    return -(map_floor_height_grid.cells[z][x] * KF_MAP_HEIGHT_STEP);
}

inline KfMapAttribute map_attribute_at_cell(s32 x, s32 z)
{
    return map_cell_attribute_grid.cells[z][x];
}

#endif // KF_MAP_DATA_H
