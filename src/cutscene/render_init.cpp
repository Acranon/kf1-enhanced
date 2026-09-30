#include <kf/platform/prelude.h>
#include <kf/cutscene/playback.h>
#include <kf/cutscene/render.h>
#include <kf/cutscene/resources.h>
#include <kf/lib/item.h>
#include <kf/lib/memory.h>
#include <kf/lib/resource_file.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum {
    DISPLAY_ASSET_BUFFER_BYTES = 2 * 0x26160,
    FLOOR_ITEM_TPAGE_X = 832,
    FLOOR_ITEM_PALETTE_Y = 488
};

std::array<MATRIX, KF_OPEN_COLOR_PRESET_COUNT> cutscene_color_matrix_table = {
    MATRIX{
        .m = {{
            {2000, 700, 4000},
            {2000, 700, 4000},
            {2000, 700, 4000},
        }},
        .t = {},
    },
    MATRIX{},
    MATRIX{
        .m = {{
            {4095, 4095, 4095},
            {4095, 4095, 4095},
            {4095, 4095, 4095},
        }},
        .t = {},
    },
    MATRIX{
        .m = {{
            {170, 682, 682},
            {170, 341, 341},
            {0, 0, 0},
        }},
        .t = {},
    },
    MATRIX{
        .m = {{
            {0, 0, 0},
            {375, 375, 375},
            {0, 0, 0},
        }},
        .t = {},
    },
};

MATRIX floor_item_light_matrix = {
    .m = {{
        {0, 0, KF_FIXED12_ONE},
        {0, 0, KF_FIXED12_ONE},
        {0, 0, 0},
    }},
    .t = {},
};

KfGraphicsRuntimeOpen open_graphics_runtime;

void cutscene_render_initialize(void)
{
    SVECTOR angles;
    u8 *buffer;

    open_graphics_runtime.display_state.buffer_index = KF_DISPLAY_BUFFER_UNINITIALIZED;
    if (resource_file_load_into(opening_cell_storage.rtbl_sectors,
            sizeof opening_cell_storage.rtbl_sectors, "B0/RTBL.") != KF_RESOURCE_LOADED)
        resource_file_fail("B0/RTBL.");
    buffer = (u8 *)memory_allocate(cutscene_memory_arena, DISPLAY_ASSET_BUFFER_BYTES);
    open_graphics_runtime.display_state.asset_load_buffer = buffer;
    open_graphics_runtime.display_state.asset_load_capacity = DISPLAY_ASSET_BUFFER_BYTES;
    open_graphics_runtime.floor_item_state.count = 0;
    angles = {0, 0, 0};
    kf::matrix_set_rotation_xyz(angles, open_graphics_runtime.render_state.quadrant_matrices[0]);
    angles.vy = KF_ANGLE_THREE_QUARTER_TURN;
    kf::matrix_set_rotation_xyz(angles, open_graphics_runtime.render_state.quadrant_matrices[3]);
    angles.vy = KF_ANGLE_HALF_TURN;
    kf::matrix_set_rotation_xyz(angles, open_graphics_runtime.render_state.quadrant_matrices[2]);
    angles.vy = KF_ANGLE_QUARTER_TURN;
    kf::matrix_set_rotation_xyz(angles, open_graphics_runtime.render_state.quadrant_matrices[1]);
    static constexpr std::array<std::array<s16, 3>, 3> initial_light_directions = {
        std::array<s16, 3>{3800, -2800, 0},
        std::array<s16, 3>{-3000, -3600, -3400},
        std::array<s16, 3>{-1300, 2700, 800},
    };
    open_graphics_runtime.render_state.light_matrix.m = initial_light_directions;
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.light_matrix, open_graphics_runtime.render_state.quadrant_matrices[0], open_graphics_runtime.light_quadrant_matrices[0]);
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.light_matrix, open_graphics_runtime.render_state.quadrant_matrices[1], open_graphics_runtime.light_quadrant_matrices[1]);
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.light_matrix, open_graphics_runtime.render_state.quadrant_matrices[2], open_graphics_runtime.light_quadrant_matrices[2]);
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.light_matrix, open_graphics_runtime.render_state.quadrant_matrices[3], open_graphics_runtime.light_quadrant_matrices[3]);
    open_graphics_runtime.floor_item_state.texture = {kf::SurfaceKind::Texture,
        {FLOOR_ITEM_TPAGE_X, 0, 0, FLOOR_ITEM_PALETTE_Y, kf::TextureFormat::Indexed8},
        kf::BlendMode::average};
}

void cutscene_lighting_set_active_color_matrix(KfOpenColorPreset preset)
{
    auto &destination = open_graphics_runtime.render_state.lighting.color_matrix;
    const auto &source = cutscene_color_matrix_table[kf_enum_encode<s32>(preset)];
    destination.m = source.m;
}

void cutscene_display_initialize(Cutscene scene)
{
    open_graphics_runtime.display_state.frame_style = {};
    open_graphics_runtime.render_state.projection = {};
    // A new intro starts black; the ending retains the game's transition image.
    if (scene == Cutscene::Intro) {
        kf::FaceList blank {};
        kf::host_present_faces(&blank);
    }
    open_graphics_runtime.render_state.lighting.ambient = {0, 0, 0};
    cutscene_lighting_set_active_color_matrix(KF_OPEN_COLOR_DEFAULT);
    open_graphics_runtime.render_state.lighting.fog = {0, 0, 0};
    open_graphics_runtime.render_state.fog_near_distance = KF_INITIAL_FOG_NEAR_DISTANCE;
    open_graphics_runtime.render_state.projection.fog_near = KF_INITIAL_FOG_NEAR_DISTANCE;
    open_graphics_runtime.tmd_projection_shift = KF_TMD_DEFAULT_PERSPECTIVE_SHIFT;
    cutscene_render_initialize();
}

void render_init_reset_module_state(void)
{
    kf::restore_initial_value<cutscene_color_matrix_table>();
    kf::restore_initial_value<floor_item_light_matrix>();
    kf::restore_initial_value<open_graphics_runtime>();
}
