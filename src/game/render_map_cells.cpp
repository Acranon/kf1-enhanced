#include <kf/platform/prelude.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/map_data.h>
#include <kf/game/player_motion.h>
#include <kf/platform/input.h>
#include <cstdlib>

enum {
    ILLUSION_STAFF_REMAP_PHASE_MASK = 3,
    ILLUSION_STAFF_REMAP_ACTIVE_UPDATES = 2
};

#define H KF_CELL_WINDOW_HIDDEN
#define D KF_CELL_WINDOW_DISTANT
#define N KF_CELL_WINDOW_NEAR

static KfCellWindow render_fixed_cell_window = {
    13, 13, 6, 6,
    {
        H, H, H, H, D, D, D, D, D, D, H, H, H,
        H, H, H, D, D, D, D, D, D, D, D, H, H,
        H, H, D, D, D, D, D, D, D, D, D, D, H,
        H, D, D, D, D, D, D, D, D, D, D, D, D,
        D, D, D, D, D, D, D, D, D, D, D, D, D,
        D, D, D, D, D, N, N, N, D, D, D, D, D,
        D, D, D, D, D, N, N, N, D, D, D, D, D,
        D, D, D, D, D, N, N, N, D, D, D, D, D,
        D, D, D, D, D, D, D, D, D, D, D, D, D,
        H, D, D, D, D, D, D, D, D, D, D, D, D,
        H, H, D, D, D, D, D, D, D, D, D, D, H,
        H, H, H, D, D, D, D, D, D, D, D, H, H,
        H, H, H, H, D, D, D, D, D, D, H, H, H,
    }
};
#undef H
#undef D
#undef N

void render_map_cell(s32 col, s32 row, KfCellVisibility visibility)
{
    MATRIX cell_matrix;
    SVECTOR position;
    s32 orientation;
    u8 object_index;
    s16 staff_timer;

    object_index = kf_enum_encode<u8>(map_cell_attribute_grid.cells[row][col]);
    if (object_index == kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_NONE)) {
        return;
    }
    staff_timer = player_state.illusion_staff_timer;

    if (staff_timer != KF_ILLUSION_STAFF_INACTIVE
        && (staff_timer & ILLUSION_STAFF_REMAP_PHASE_MASK) < ILLUSION_STAFF_REMAP_ACTIVE_UPDATES) {
        switch (object_index) {
        case kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_44):
            object_index = kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_17);
            break;
        case kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_HIDDEN_DOOR):
            object_index = kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_18);
            break;
        case kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_46):
            object_index = kf_enum_encode<u8>(KF_MAP_ATTRIBUTE_19);
            break;
        }
    }
    object_index = object_index - 1;
    if (object_index > KF_MAP_MESHES_PER_BANK - 1) {
        return;
    }
    orientation = kf_enum_encode<u8>(map_cell_orientation_grid.cells[row][col]) - 1;
    if (visibility == KF_CELL_WINDOW_DISTANT) {
        object_index += KF_MAP_MESHES_PER_BANK;
    }
    position = VECTOR{
        col * KF_MAP_TILE_SIZE - game_graphics_runtime.render_state.view_position.vx,
        map_floor_height_grid.cells[row][col] * -KF_MAP_HEIGHT_STEP
            - game_graphics_runtime.render_state.view_position.vy,
        row * KF_MAP_TILE_SIZE - game_graphics_runtime.render_state.view_position.vz}.narrowed();
    if (orientation == kf_enum_encode<u8>(KF_MAP_ORIENT_QUARTER_TURN) - 1) {
        position.vz += KF_MAP_TILE_SIZE;
    } else if (orientation == kf_enum_encode<u8>(KF_MAP_ORIENT_HALF_TURN) - 1) {
        position.vx += KF_MAP_TILE_SIZE;
        position.vz += KF_MAP_TILE_SIZE;
    } else if (orientation == kf_enum_encode<u8>(KF_MAP_ORIENT_THREE_QUARTER_TURN) - 1) {
        position.vx += KF_MAP_TILE_SIZE;
    }

    kf::render_place_model(cell_matrix, game_graphics_runtime.render_state.view_matrix, position);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.view_matrix, game_graphics_runtime.render_state.quadrant_matrices[orientation], cell_matrix);
    tmd_select_object_vertices(tmd_context(), object_index);
    const KfPreciseMapTransform precise{&game_graphics_runtime.render_state.view_matrix,
        &game_graphics_runtime.render_state.quadrant_matrices[orientation], position};
    render_precise_map = &precise;
    render_enqueue_map(object_index, &game_graphics_runtime.light_quadrant_matrices[orientation], &cell_matrix, game_graphics_runtime.render_state.projection);
    render_precise_map = nullptr;
}

// PC extension: a widescreen picture shows more to each side than the original
// 4:3 cone, and every map cell, object, actor and effect is culled through the
// active cell window. Each of the 16 yaw windows covers 1/16 turn, so merging a
// window with both neighbours covers any aspect the renderer allows. The cells
// right around the camera are always included too: looking down past the
// original limit shows the floor beside and behind the player.
static const KfCellWindow *render_widen_cell_window(int yaw_index)
{
    constexpr int overflow_capacity = 1024;
    static struct {
        KfCellWindow window;
        KfCellVisibility overflow[overflow_capacity];
    } wide;
    static_assert(offsetof(decltype(wide), overflow) == offsetof(KfCellWindow, cells) + sizeof(KfCellWindow::cells),
                  "merged cells must continue past the original capacity");
    const KfCellWindow *parts[] = {
        &render_cell_windows[(yaw_index + KF_CELL_WINDOW_YAW_COUNT - 1) % KF_CELL_WINDOW_YAW_COUNT],
        &render_cell_windows[yaw_index],
        &render_cell_windows[(yaw_index + 1) % KF_CELL_WINDOW_YAW_COUNT]};
    constexpr int around_camera = 2;
    int min_dx = -around_camera, max_dx = around_camera, min_dz = -around_camera, max_dz = around_camera;
    for (const auto *part : parts) {
        min_dx = std::min(min_dx, -int(part->origin_x));
        max_dx = std::max(max_dx, int(part->width) - 1 - int(part->origin_x));
        min_dz = std::min(min_dz, -int(part->origin_z));
        max_dz = std::max(max_dz, int(part->height) - 1 - int(part->origin_z));
    }
    const int width = max_dx - min_dx + 1, height = max_dz - min_dz + 1;
    if (width * height > KF_CELL_WINDOW_CELL_CAPACITY + overflow_capacity)
        return &render_cell_windows[yaw_index];
    KfCellVisibility *cells = wide.window.cells;
    std::fill_n(cells, width * height, KF_CELL_WINDOW_HIDDEN);
    for (const auto *part : parts) {
        for (int row = 0; row < part->height; ++row) {
            for (int col = 0; col < part->width; ++col) {
                const auto value = part->cells[row * part->width + col];
                auto &merged = cells[(row - part->origin_z - min_dz) * width + (col - part->origin_x - min_dx)];
                // NEAR > DISTANT > HIDDEN: keep the most detailed mesh any window asks for.
                if (kf_enum_encode<int>(value) > kf_enum_encode<int>(merged))
                    merged = value;
            }
        }
    }
    for (int dz = -around_camera; dz <= around_camera; ++dz)
        for (int dx = -around_camera; dx <= around_camera; ++dx)
            cells[(dz - min_dz) * width + (dx - min_dx)] = KF_CELL_WINDOW_NEAR;
    wide.window.width = static_cast<u16>(width);
    wide.window.height = static_cast<u16>(height);
    wide.window.origin_x = static_cast<u16>(-min_dx);
    wide.window.origin_z = static_cast<u16>(-min_dz);
    return &wide.window;
}

// PC extension: the steep-pitch window (only reachable past the original look
// limit) draws the full-detail mesh for the 7x7 cells around the camera instead
// of 3x3, since looking down shows those cells up close.
static const KfCellWindow *render_steep_cell_window()
{
    static KfCellWindow window;
    static bool built;
    if (!built) {
        window = render_fixed_cell_window;
        for (int row = 0; row < window.height; ++row) {
            for (int col = 0; col < window.width; ++col) {
                auto &cell = window.cells[row * window.width + col];
                if (cell != KF_CELL_WINDOW_HIDDEN && std::abs(row - window.origin_z) <= 3
                    && std::abs(col - window.origin_x) <= 3)
                    cell = KF_CELL_WINDOW_NEAR;
            }
        }
        built = true;
    }
    return &window;
}

void render_map_cells(void)
{
    const KfCellVisibility *cell;
    int row;
    int col_base;
    int col;
    u8 rows;
    u8 cols;

    if ((u16)(game_graphics_runtime.render_state.view_rotation.vx + (KF_ANGLE_EIGHTH_TURN - 1))
        >= 2 * KF_ANGLE_EIGHTH_TURN - 1) {
        game_graphics_runtime.active_cell_window = &render_fixed_cell_window;
        if (kf::player_pitch_limit > KF_PLAYER_CAMERA_PITCH_LIMIT)
            game_graphics_runtime.active_cell_window = render_steep_cell_window();
    } else {
        const int yaw_index = KF_CELL_WINDOW_YAW_COUNT - 1
            - (game_graphics_runtime.render_state.view_rotation.vy >> KF_CELL_WINDOW_YAW_SHIFT);
        game_graphics_runtime.active_cell_window = &render_cell_windows[yaw_index];
        if (kf::host_renderer()->logical_width > kf::render_width || kf::render_smoothing
                || kf::player_pitch_limit > KF_PLAYER_CAMERA_PITCH_LIMIT)
            game_graphics_runtime.active_cell_window = render_widen_cell_window(yaw_index);
    }

    row = (u16)game_graphics_runtime.render_state.view_cell.z - game_graphics_runtime.active_cell_window->origin_z;
    col_base = (u16)game_graphics_runtime.render_state.view_cell.x - game_graphics_runtime.active_cell_window->origin_x;
    cell = game_graphics_runtime.active_cell_window->cells;
    tmd_select(tmd_context(), KF_TMD_SLOT_MAP);

    rows = game_graphics_runtime.active_cell_window->height;
    do {
        if ((u32)row < KF_MAP_ROWS) {
            col = col_base;
            cols = game_graphics_runtime.active_cell_window->width;
            do {
                if ((u32)col < KF_MAP_COLUMNS && *cell != KF_CELL_WINDOW_HIDDEN) {
                    render_map_cell(col, row, *cell);
                }
                cell++;
                col++;
                cols--;
            } while (cols != 0);
        } else {
            cell += game_graphics_runtime.active_cell_window->width;
        }
        row++;
        rows--;
    } while (rows != 0);
}

void render_map_cells_reset_module_state(void)
{
    kf::restore_initial_value<render_fixed_cell_window>();
}
