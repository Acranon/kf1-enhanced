#include <kf/platform/prelude.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/math.h>
#include <kf/lib/null.h>

enum {
    LIGHTING_COLOR_BLEND_STEP = 0x400,
    VITAL_RESTORE_COLOR_LEVEL = 0xfff
};

void lighting_transition_color_matrix(const MATRIX *from, const MATRIX *to)
{
    const auto input_context = kf::host_set_input_context(kf::InputContext::Scripted);
    s32 blend = 0;

    do {
        lighting_set_color_matrix(game_graphics_runtime.render_state, from, to, blend);
        render_frame(NULL, NULL);
        blend += LIGHTING_COLOR_BLEND_STEP;
    } while (blend <= KF_FIXED12_ONE);
    kf::host_set_input_context(input_context);
}

void color_matrix_set_rgb(s16 red, s16 green, s16 blue, MATRIX *matrix)
{
    matrix->m[0][2] = red;
    matrix->m[0][1] = red;
    matrix->m[0][0] = red;
    matrix->m[1][2] = green;
    matrix->m[1][1] = green;
    matrix->m[1][0] = green;
    matrix->m[2][2] = blue;
    matrix->m[2][1] = blue;
    matrix->m[2][0] = blue;
}

void player_restore_vitals_with_color_cycle(void)
{
    MATRIX saved;
    MATRIX first;
    MATRIX second;

    saved = game_graphics_runtime.render_state.lighting.color_matrix;
    color_matrix_set_rgb(0, VITAL_RESTORE_COLOR_LEVEL, 0, &first);
    lighting_transition_color_matrix(&saved, &first);
    color_matrix_set_rgb(
        0, VITAL_RESTORE_COLOR_LEVEL, VITAL_RESTORE_COLOR_LEVEL, &second);
    lighting_transition_color_matrix(&first, &second);
    color_matrix_set_rgb(VITAL_RESTORE_COLOR_LEVEL, VITAL_RESTORE_COLOR_LEVEL,
        VITAL_RESTORE_COLOR_LEVEL, &first);
    lighting_transition_color_matrix(&second, &first);
    lighting_transition_color_matrix(&first, &saved);
    player_state.vitals.current_hp = player_state.vitals.maximum_hp;
    player_state.vitals.current_mp = player_state.vitals.maximum_mp;
    player_state.status_effect_flags &= KF_PLAYER_STATUS_KEEP_UPPER;
}
