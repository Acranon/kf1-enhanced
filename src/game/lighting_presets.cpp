#include <kf/platform/prelude.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>

enum {
    SHADOW_BLADE_COLOR_BLEND = 2500,
    LIGHTING_EFFECT_BLEND = 0xc00
};

static inline void lighting_blend_current_color(const MATRIX *target, s32 amount)
{
    MATRIX current;

    current = game_graphics_runtime.render_state.lighting.color_matrix;
    lighting_set_color_matrix(game_graphics_runtime.render_state, &current, target, amount);
}

void lighting_apply_shadow_blade_environment(void)
{
    lighting_blend_current_color(&color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_BLACK)], SHADOW_BLADE_COLOR_BLEND);
    game_graphics_runtime.render_state.projection.fog_near = game_graphics_runtime.render_state.fog_near_distance - (game_graphics_runtime.render_state.fog_near_distance >> 1);
}

void lighting_apply_illusion_staff_effect(void)
{
    lighting_blend_current_color(&color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_GREEN)], LIGHTING_EFFECT_BLEND);
}

void lighting_apply_blue_tint(void)
{
    lighting_blend_current_color(&color_matrix_table[kf_enum_encode<s32>(KF_GAME_COLOR_BLUE)], LIGHTING_EFFECT_BLEND);
}

void lighting_set_active_color_matrix(KfGameColorPreset preset)
{
    auto &destination = game_graphics_runtime.render_state.lighting.color_matrix;
    const auto &source = color_matrix_table[kf_enum_encode<s32>(preset)];
    destination.m = source.m;
}
