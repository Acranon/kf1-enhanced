#include <kf/platform/prelude.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>

#include <array>

typedef struct {
    std::array<u8, KF_QUAD_TEX_DESCRIPTOR_BYTES> v;
} FadeUv;

KfScreenRect fade_screen_rect = {
    KF_TRANSITION_RECT_X, 0, KF_TRANSITION_RECT_WIDTH, KF_TRANSITION_RECT_HEIGHT
};

FadeUv fade_screen_uv = {
    0, 0, 0, 0, KF_TRANSITION_RECT_WIDTH, 0, KF_TRANSITION_RECT_HEIGHT, 0
};

CVECTOR fade_screen_color = {0, 0, 0, 0};

std::array<char, 7> fade_screen_path = {"B0/L0."};

void display_play_transition(void)
{
    KfScreenRect rect = fade_screen_rect;
    FadeUv uv = fade_screen_uv;
    CVECTOR color = fade_screen_color;
    s32 i;

    kf::FaceList blank {};
    blank.style = game_graphics_runtime.display_state.frame_style;
    kf::host_present_faces(&blank);

    std::size_t image_size;
    if (resource_file_load_into(game_graphics_runtime.display_state.asset_load_buffer,
            game_graphics_runtime.display_state.asset_load_capacity, fade_screen_path.data(), &image_size) != KF_RESOURCE_LOADED) {
        return;
    }
    tim_upload_images(game_graphics_runtime.display_state.asset_load_buffer, image_size);
    const auto input_context = kf::host_set_input_context(kf::InputContext::Scripted);

    const kf::FaceMaterial material{kf::SurfaceKind::Texture,
        {KF_TRANSITION_TPAGE_X, KF_TEXTURE_LOWER_PAGE_Y, 0, KF_TRANSITION_CLUT_Y,
         kf::TextureFormat::Indexed4}, kf::BlendMode::average};

    for (i = 0; i < KF_TRANSITION_FADE_FRAMES; i++) {
        display_begin_frame(game_graphics_runtime.display_state);
        transition_advance_color(color);
        sprite_add_ft4(&rect, uv.v, material, &color, KF_TRANSITION_OT_DEPTH);
        display_present_frame(game_graphics_runtime.display_state);
    }
    kf::host_set_input_context(input_context);
}

void display_play_transition_reset_module_state(void)
{
    kf::restore_initial_value<fade_screen_rect>();
    kf::restore_initial_value<fade_screen_uv>();
    kf::restore_initial_value<fade_screen_color>();
    kf::restore_initial_value<fade_screen_path>();
}
