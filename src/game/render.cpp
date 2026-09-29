#include <kf/game/resources.h>
#include <kf/lib/null.h>
#include <kf/game/graphics.h>

#include <kf/lib/map_data.h>
#include <kf/game/resource_file.h>
#include <kf/game/render.h>
#include <kf/game/notify.h>
#include <kf/lib/geometry_types.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/game/game.h>
#include <kf/lib/tmd.h>

enum {
    DISPLAY_ASSET_BUFFER_BYTES = 2 * 0x19640,
    INITIAL_BACK_COLOR = 60,
    SYSTEM_SCREEN_BRIGHTNESS = 96,
    EFFECT_TEXTURE_FIRST_PAGE_X = 320,
    EFFECT_TEXTURE_SECOND_PAGE_X = 384,
    EFFECT_TEXTURE_THIRD_PAGE_X = 832,
    EFFECT_TEXTURE_CLUT_Y = 491,
    FLOOR_ITEM_TPAGE_X = 896,
    FLOOR_ITEM_PALETTE_Y = 489,
    HUD_TPAGE_X = 896,
    HUD_PALETTE_Y = 500,
    NOTIFICATION_TPAGE_X = 832,
    NOTIFICATION_DIGIT_TPAGE_X = 768,
    NOTIFICATION_PALETTE_Y = 499
};

MATRIX color_matrix_table[KF_GAME_COLOR_PRESET_COUNT] = {
    {{{2000, 700, 4000}, {2000, 700, 4000}, {2000, 700, 4000}}, {0, 0, 0}},
    {{{3000, 1000, 4000}, {200, 70, 400}, {200, 70, 400}}, {0, 0, 0}},
    {{{1000, 350, 2000}, {1000, 350, 2000}, {3000, 1000, 4000}}, {0, 0, 0}},
    {{{4095, 4095, 4095}, {4095, 4095, 4095}, {4095, 4095, 4095}}, {0, 0, 0}},
    {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, {0, 0, 0}},
    {{{0, 0, 0}, {4095, 4095, 4095}, {0, 0, 0}}, {0, 0, 0}},
    {{{0, 0, 0}, {0, 0, 0}, {4095, 4095, 4095}}, {0, 0, 0}},
};

KfGraphicsRuntimeGame game_graphics_runtime;

void display_show_system_screen(KfSystemScreen screen)
{
    char path[] = "E0.";
    std::size_t image_size;
    path[1] = kf_enum_encode<s32>(screen) + '0';
    if (kf::data_file_read_into(path, game_graphics_runtime.display_state.asset_load_buffer,
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

void effect5_texture_cache_prepare(KfFloorId floor)
{
    if (floor == KF_FLOOR_5) {
        constexpr u16 page_x[KF_FLOOR5_ACTOR_TEXTURE_COUNT] = {
            EFFECT_TEXTURE_FIRST_PAGE_X, EFFECT_TEXTURE_SECOND_PAGE_X, EFFECT_TEXTURE_THIRD_PAGE_X};
        for (unsigned i = 0; i < KF_FLOOR5_ACTOR_TEXTURE_COUNT; ++i)
            game_graphics_runtime.effect5_materials[i] = {kf::SurfaceKind::Texture,
                {page_x[i], KF_TEXTURE_LOWER_PAGE_Y, 0, EFFECT_TEXTURE_CLUT_Y,
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
    KfNotificationId *flag;
    u8 count;
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
    static constexpr s16 initial_light_directions[3][3] = {
        {3800, -2800, 0},
        {-3000, -3600, -3400},
        {-1300, 2700, 800},
    };
    memcpy(game_graphics_runtime.render_state.light_matrix.m,
        initial_light_directions, sizeof initial_light_directions);
    game_graphics_runtime.light_matrix_copy = game_graphics_runtime.render_state.light_matrix;
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
    flag = game_graphics_runtime.notification_message_ids;
    count = KF_NOTIFICATION_CAPACITY - 1;
    do {
        *flag++ = KF_NOTIFICATION_NONE;
    } while (count-- != 0);
    pool_reset();
}

void tmd_project_vertices(s32 count, const MATRIX *model, const kf::Projection &projection)
{
    if (count < 0 || count > KF_PROJECTED_VERTEX_CAPACITY)
        kf::host_fail("Model exceeds projected vertex capacity.");
    KfScreenVertex *projected;
    SVECTOR *vertex;

    projected = game_graphics_runtime.tmd_projected_vertices;
    vertex = game_graphics_runtime.current_tmd_vertices;
    for (count--; count != -1; count--) {
        const auto point = kf::render_project_point(*model, projection, *vertex);
        projected->sxy.vector = {point.x, point.y};
        projected->p2 = point.fog << KF_TMD_DEFAULT_PERSPECTIVE_SHIFT;
        projected->sz = point.depth;
        projected->fx = point.fx;
        projected->fy = point.fy;
        projected->w = point.w;
        projected->cx = point.cx;
        projected->cy = point.cy;
        projected->cz = point.cz;
        projected++;
        vertex++;
    }
}

void render_reset_module_state(void)
{
    kf::restore_initial_value<color_matrix_table>();
    kf::restore_initial_value<game_graphics_runtime>();
}
