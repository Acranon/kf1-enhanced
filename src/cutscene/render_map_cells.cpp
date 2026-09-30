#include <kf/platform/prelude.h>
#include <kf/cutscene/render.h>
#include <kf/cutscene/resources.h>
#include <kf/cutscene/scene0.h>
#include <kf/lib/map_data.h>

KfOpeningCellStorage opening_cell_storage;

void cutscene_render_map_cell(s32 col, s32 row, KfCellVisibility visibility)
{
    MATRIX cell_matrix;
    SVECTOR position;
    s32 orientation;
    u8 object_index;

    object_index = kf_enum_encode<u8>(cutscene_map_cell_attribute_grid.cells[row][col]);
    if (object_index == kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_NONE)) {
        return;
    }
    object_index = object_index - 1;
    if (object_index > KF_MAP_MESHES_PER_BANK - 1) {
        return;
    }
    orientation = kf_enum_encode<u8>(cutscene_map_cell_orientation_grid.cells[row][col]) - 1;
    if (visibility == KF_CELL_WINDOW_DISTANT) {
        object_index += KF_MAP_MESHES_PER_BANK;
    }
    position = VECTOR{
        col * KF_MAP_TILE_SIZE - open_graphics_runtime.render_state.view_position.vx,
        cutscene_map_floor_height_grid.cells[row][col] * -KF_MAP_HEIGHT_STEP -
            open_graphics_runtime.render_state.view_position.vy,
        row * KF_MAP_TILE_SIZE - open_graphics_runtime.render_state.view_position.vz}.narrowed();
    if (orientation == kf_enum_encode<u8>(KF_MAP_ORIENT_QUARTER_TURN) - 1) {
        position.vz += KF_MAP_TILE_SIZE;
    } else if (orientation == kf_enum_encode<u8>(KF_MAP_ORIENT_HALF_TURN) - 1) {
        position.vx += KF_MAP_TILE_SIZE;
        position.vz += KF_MAP_TILE_SIZE;
    } else if (orientation == kf_enum_encode<u8>(KF_MAP_ORIENT_THREE_QUARTER_TURN) - 1) {
        position.vx += KF_MAP_TILE_SIZE;
    }

    kf::render_place_model(cell_matrix, open_graphics_runtime.render_state.view_matrix, position);
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.view_matrix, open_graphics_runtime.render_state.quadrant_matrices[orientation], cell_matrix);
    tmd_select_object_vertices(cutscene_tmd_context(), object_index);
    cutscene_render_enqueue_map(object_index, &open_graphics_runtime.light_quadrant_matrices[orientation], &cell_matrix, open_graphics_runtime.render_state.projection);
}

void opening_render_map_cells(void)
{
    const KfCellVisibility *cell;
    s32 row;
    s32 col_base;
    s32 col;
    u8 rows;
    u8 cols;

    open_graphics_runtime.active_cell_window =
        &opening_cell_storage.scene.windows[KF_CELL_WINDOW_YAW_COUNT - 1
            - (open_graphics_runtime.render_state.view_rotation.vy
                >> KF_CELL_WINDOW_YAW_SHIFT)];
    cell = open_graphics_runtime.active_cell_window->cells;
    row = (u16)open_graphics_runtime.render_state.view_cell.z - open_graphics_runtime.active_cell_window->origin_z;
    col_base = (u16)open_graphics_runtime.render_state.view_cell.x - open_graphics_runtime.active_cell_window->origin_x;
    tmd_select(cutscene_tmd_context(), KF_TMD_SLOT_MAP);

    rows = open_graphics_runtime.active_cell_window->height;
    do {
        if ((u32)row < KF_MAP_ROWS) {
            col = col_base;
            cols = open_graphics_runtime.active_cell_window->width;
            do {
                if ((u32)col < KF_MAP_COLUMNS && *cell != KF_CELL_WINDOW_HIDDEN) {
                    cutscene_render_map_cell(col, row, *cell);
                }
                cell++;
                col++;
                cols--;
            } while (cols != 0);
        } else {
            cell += open_graphics_runtime.active_cell_window->width;
        }
        row++;
        rows--;
    } while (rows != 0);
}

void cutscene_render_map_cells_reset_module_state(void)
{
    kf::restore_initial_value<opening_cell_storage>();
}
