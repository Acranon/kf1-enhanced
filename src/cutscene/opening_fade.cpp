#include <kf/platform/prelude.h>
#include <kf/cutscene/render.h>

#include <array>

KfScreenRect opening_fade_rect = {
    KF_TRANSITION_RECT_X, 0, KF_TRANSITION_RECT_WIDTH, KF_TRANSITION_RECT_HEIGHT
};

std::array<u8, KF_QUAD_TEX_DESCRIPTOR_BYTES> opening_fade_uv = {
    0, 0, 0, 0, KF_TRANSITION_RECT_WIDTH, 0, KF_TRANSITION_RECT_HEIGHT, 0
};

CVECTOR opening_fade_color = {0, 0, 0, 0};

void opening_fade_in(void)
{
    CVECTOR color = opening_fade_color;
    s32 frame;

    const kf::FaceMaterial material{kf::SurfaceKind::Texture,
        {KF_TRANSITION_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, KF_TRANSITION_CLUT_Y,
         kf::TextureFormat::Indexed4}, kf::BlendMode::average};
    frame = 0;
    do {
        display_begin_frame(open_graphics_runtime.display_state);
        transition_advance_color(color);
        sprite_add_ft4(
            &opening_fade_rect, opening_fade_uv, material, &color, KF_TRANSITION_OT_DEPTH);
        display_present_frame(open_graphics_runtime.display_state);
        frame++;
    } while (frame < KF_TRANSITION_FADE_FRAMES);
}

void opening_fade_reset_module_state(void)
{
    kf::restore_initial_value<opening_fade_rect>();
    kf::restore_initial_value<opening_fade_uv>();
    kf::restore_initial_value<opening_fade_color>();
}
