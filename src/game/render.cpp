#include <kf/platform/prelude.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/notify.h>
#include <kf/game/render.h>
#include <kf/game/resource_file.h>
#include <kf/game/resources.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>
#include <kf/lib/tmd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum {
    DISPLAY_ASSET_BUFFER_BYTES = 2 * 0x19640,
    INITIAL_BACK_COLOR = 60,
    SYSTEM_SCREEN_BRIGHTNESS = 96,
    ACTOR_TEXTURE_FIRST_PAGE_X = 320,
    ACTOR_TEXTURE_SECOND_PAGE_X = 384,
    ACTOR_TEXTURE_THIRD_PAGE_X = 832,
    ACTOR_TEXTURE_CLUT_Y = 491,
    FLOOR_ITEM_TPAGE_X = 896,
    FLOOR_ITEM_PALETTE_Y = 489,
    HUD_TPAGE_X = 896,
    HUD_PALETTE_Y = 500,
    NOTIFICATION_TPAGE_X = 832,
    NOTIFICATION_DIGIT_TPAGE_X = 768,
    NOTIFICATION_PALETTE_Y = 499
};

std::array<MATRIX, KF_GAME_COLOR_PRESET_COUNT> color_matrix_table = {
    MATRIX{
        .m = {{
            {2000, 700, 4000},
            {2000, 700, 4000},
            {2000, 700, 4000},
        }},
        .t = {},
    },
    MATRIX{
        .m = {{
            {3000, 1000, 4000},
            {200, 70, 400},
            {200, 70, 400},
        }},
        .t = {},
    },
    MATRIX{
        .m = {{
            {1000, 350, 2000},
            {1000, 350, 2000},
            {3000, 1000, 4000},
        }},
        .t = {},
    },
    MATRIX{
        .m = {{
            {4095, 4095, 4095},
            {4095, 4095, 4095},
            {4095, 4095, 4095},
        }},
        .t = {},
    },
    MATRIX{},
    MATRIX{
        .m = {{
            {0, 0, 0},
            {4095, 4095, 4095},
            {0, 0, 0},
        }},
        .t = {},
    },
    MATRIX{
        .m = {{
            {0, 0, 0},
            {0, 0, 0},
            {4095, 4095, 4095},
        }},
        .t = {},
    },
};

KfGraphicsRuntimeGame game_graphics_runtime;

void display_show_system_screen(KfSystemScreen screen)
{
    std::array<char, 4> path = {"E0."};
    std::size_t image_size;
    path[1] = kf_enum_encode<s32>(screen) + '0';
    if (kf::data_file_read_into(path.data(), game_graphics_runtime.display_state.asset_load_buffer,
            game_graphics_runtime.display_state.asset_load_capacity, &image_size) != kf::FileResult::Ok)
        kf::host_fail("Cannot load system screen.");
    tim_upload_images(game_graphics_runtime.display_state.asset_load_buffer, image_size);
    const auto input_context = kf::host_set_input_context(kf::InputContext::Menu);
    display_present_system_screen(SYSTEM_SCREEN_BRIGHTNESS);
    // Fast file loading must not let the key that opened pause dismiss it.
    if (screen == KF_SYSTEM_SCREEN_PAUSE)
        kf::host_wait_buttons_released();
    kf::host_wait_button_press();
    kf::host_wait_buttons_released();
    kf::host_set_input_context(input_context);
}

void display_present_system_screen(s32 color)
{
    kf::DrawFace face {};
    face.shape = kf::FaceShape::Quad;
    face.material = {kf::SurfaceKind::Texture,
        {KF_SYSTEM_SCREEN_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, KF_SYSTEM_SCREEN_CLUT_Y,
         kf::TextureFormat::Indexed4}, kf::BlendMode::average};
    face.transparency = kf::FaceTransparency::Blend;
    const float brightness = color / kf::texture_color_unity;
    constexpr float right_u = static_cast<float>(KF_SYSTEM_SCREEN_U_SPAN) / kf::texture_uv_scale;
    constexpr float bottom_v = static_cast<float>(KF_SYSTEM_SCREEN_V_SPAN) / kf::texture_uv_scale;
    face.vertices[0] = {KF_SYSTEM_SCREEN_LEFT, KF_SYSTEM_SCREEN_TOP, 0, 0,
                       brightness, brightness, brightness, 1};
    face.vertices[1] = {KF_SYSTEM_SCREEN_RIGHT, KF_SYSTEM_SCREEN_TOP, right_u, 0,
                       brightness, brightness, brightness, 1};
    face.vertices[2] = {KF_SYSTEM_SCREEN_LEFT, KF_SYSTEM_SCREEN_BOTTOM, 0, bottom_v,
                       brightness, brightness, brightness, 1};
    face.vertices[3] = {KF_SYSTEM_SCREEN_RIGHT, KF_SYSTEM_SCREEN_BOTTOM, right_u, bottom_v,
                       brightness, brightness, brightness, 1};
    kf::FaceList draws {&face, 1};
    draws.style.clear = kf::FrameClear::Retain;
    kf::host_present_faces(&draws);
}

void render_prepare_actor_textures(KfFloorId floor)
{
    if (floor == KF_FLOOR_5) {
        constexpr std::array<u16, KF_FLOOR5_ACTOR_TEXTURE_COUNT> page_x = {
            ACTOR_TEXTURE_FIRST_PAGE_X, ACTOR_TEXTURE_SECOND_PAGE_X, ACTOR_TEXTURE_THIRD_PAGE_X};
        for (std::size_t i = 0; i < KF_FLOOR5_ACTOR_TEXTURE_COUNT; ++i)
            game_graphics_runtime.effect5_materials[i] = {kf::SurfaceKind::Texture,
                {page_x[i], KF_TEXTURE_LOWER_PAGE_Y, 0, ACTOR_TEXTURE_CLUT_Y,
                 kf::TextureFormat::Indexed8}, kf::BlendMode::average};
    }
}

void display_initialize(void)
{
    game_graphics_runtime.display_state.frame_style = {};
    game_graphics_runtime.render_state.projection = {};
    game_graphics_runtime.render_state.lighting.ambient = {INITIAL_BACK_COLOR, INITIAL_BACK_COLOR, INITIAL_BACK_COLOR};
    lighting_set_active_color_matrix(KF_GAME_COLOR_DEFAULT);
    game_graphics_runtime.render_state.lighting.fog = {0, 0, 0};
    game_graphics_runtime.render_state.fog_near_distance = KF_INITIAL_FOG_NEAR_DISTANCE;
    game_graphics_runtime.render_state.projection.fog_near = KF_INITIAL_FOG_NEAR_DISTANCE;
    render_initialize();
}

void render_initialize(void)
{
    SVECTOR angles;
    u8 *buffer;

    game_graphics_runtime.display_state.buffer_index = KF_DISPLAY_BUFFER_UNINITIALIZED;
    buffer = (u8 *)memory_allocate(memory_arena, DISPLAY_ASSET_BUFFER_BYTES);
    game_graphics_runtime.display_state.asset_load_buffer = buffer;
    game_graphics_runtime.display_state.asset_load_capacity = DISPLAY_ASSET_BUFFER_BYTES;
    game_graphics_runtime.floor_item_count = 0;
    angles = {0, 0, 0};
    kf::matrix_set_rotation_xyz(angles, game_graphics_runtime.render_state.quadrant_matrices[0]);
    angles.vy = KF_ANGLE_THREE_QUARTER_TURN;
    kf::matrix_set_rotation_xyz(angles, game_graphics_runtime.render_state.quadrant_matrices[3]);
    angles.vy = KF_ANGLE_HALF_TURN;
    kf::matrix_set_rotation_xyz(angles, game_graphics_runtime.render_state.quadrant_matrices[2]);
    angles.vy = KF_ANGLE_QUARTER_TURN;
    kf::matrix_set_rotation_xyz(angles, game_graphics_runtime.render_state.quadrant_matrices[1]);
    static constexpr std::array<std::array<s16, 3>, 3> initial_light_directions = {
        std::array<s16, 3>{3800, -2800, 0},
        std::array<s16, 3>{-3000, -3600, -3400},
        std::array<s16, 3>{-1300, 2700, 800},
    };
    game_graphics_runtime.render_state.light_matrix.m = initial_light_directions;
    game_graphics_runtime.map_event_light_matrix = game_graphics_runtime.render_state.light_matrix;
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.light_matrix, game_graphics_runtime.render_state.quadrant_matrices[0], game_graphics_runtime.light_quadrant_matrices[0]);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.light_matrix, game_graphics_runtime.render_state.quadrant_matrices[1], game_graphics_runtime.light_quadrant_matrices[1]);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.light_matrix, game_graphics_runtime.render_state.quadrant_matrices[2], game_graphics_runtime.light_quadrant_matrices[2]);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.light_matrix, game_graphics_runtime.render_state.quadrant_matrices[3], game_graphics_runtime.light_quadrant_matrices[3]);
    game_graphics_runtime.floor_item_material = {kf::SurfaceKind::Texture,
        {FLOOR_ITEM_TPAGE_X, 0, 0, FLOOR_ITEM_PALETTE_Y, kf::TextureFormat::Indexed8},
        kf::BlendMode::average};
    game_graphics_runtime.hud_material = {kf::SurfaceKind::Texture,
        {HUD_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, HUD_PALETTE_Y, kf::TextureFormat::Indexed4},
        kf::BlendMode::average};
    game_graphics_runtime.notification_text_material = {kf::SurfaceKind::Texture,
        {NOTIFICATION_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, NOTIFICATION_PALETTE_Y,
         kf::TextureFormat::Indexed4}, kf::BlendMode::average};
    game_graphics_runtime.notification_digit_material = {kf::SurfaceKind::Texture,
        {NOTIFICATION_DIGIT_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, NOTIFICATION_PALETTE_Y,
         kf::TextureFormat::Indexed4}, kf::BlendMode::average};
    game_graphics_runtime.notification_state.control.effect_phase = KF_NOTIFICATION_IDLE;
    game_graphics_runtime.notification_state.control.queue_tail = 0;
    game_graphics_runtime.notification_state.control.queue_head = 0;
    game_graphics_runtime.notification_message_ids.fill(KF_NOTIFICATION_NONE);
    animation_cache_reset();
}

const KfPreciseMapTransform *render_precise_map;

// Exact map cell transform (see KfPreciseMapTransform); integer results are
// unchanged, only the drawn sub-pixel position and camera coordinates.
static void render_precise_map_point(const KfPreciseMapTransform &cell, const kf::Projection &projection,
    const SVECTOR &vertex, KfScreenVertex &out)
{
    const s32 local[3] = {vertex.vx, vertex.vy, vertex.vz};
    const s32 offset[3] = {cell.position.vx, cell.position.vy, cell.position.vz};
    double world[3];
    for (int r = 0; r < 3; ++r) {
        // Quadrant matrices hold 0 and +-4096, so this is exact (and shared
        // corners of neighbouring cells get identical world positions).
        s32 rotated = 0;
        for (int k = 0; k < 3; ++k)
            rotated += cell.quadrant->m[r][k] * local[k];
        world[r] = rotated / 4096.0 + offset[r];
    }
    double camera[3];
    for (int r = 0; r < 3; ++r)
        camera[r] = (cell.view->m[r][0] * world[0] + cell.view->m[r][1] * world[1] + cell.view->m[r][2] * world[2]) / 4096.0;
    constexpr double minimum_perspective_depth = 16.0;
    const double depth = std::max(camera[2], minimum_perspective_depth);
    const double scale = projection.distance / depth;
    out.fx = static_cast<float>(std::clamp(camera[0] * scale + projection.center_x, -1024.0, 1023.0));
    out.fy = static_cast<float>(std::clamp(camera[1] * scale + projection.center_y, -1024.0, 1023.0));
    out.w = static_cast<float>(depth);
    out.cx = static_cast<float>(camera[0]);
    out.cy = static_cast<float>(camera[1]);
    out.cz = static_cast<float>(camera[2]);
}

void tmd_project_vertices(s32 count, const MATRIX *model, const kf::Projection &projection)
{
    if (count < 0 || count > KF_PROJECTED_VERTEX_CAPACITY)
        kf::host_fail("Model exceeds projected vertex capacity.");
    KfScreenVertex *projected;
    SVECTOR *vertex;

    projected = game_graphics_runtime.tmd_projected_vertices.data();
    vertex = game_graphics_runtime.current_tmd_vertices;
    for (count--; count != -1; count--) {
        const auto point = kf::render_project_point(*model, projection, *vertex);
        projected->position = {point.x, point.y};
        projected->p2 = point.fog << KF_TMD_DEFAULT_PERSPECTIVE_SHIFT;
        projected->sz = point.depth;
        projected->fx = point.fx;
        projected->fy = point.fy;
        projected->w = point.w;
        projected->cx = point.cx;
        projected->cy = point.cy;
        projected->cz = point.cz;
        if (render_precise_map && kf::render_subpixel)
            render_precise_map_point(*render_precise_map, projection, *vertex, *projected);
        projected++;
        vertex++;
    }
}

void render_reset_module_state(void)
{
    kf::restore_initial_value<color_matrix_table>();
    kf::restore_initial_value<game_graphics_runtime>();
}
