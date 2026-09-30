#include <kf/platform/prelude.h>
#include <kf/lib/graphics.h>
#include <kf/lib/math.h>
#include <kf/lib/render_types.h>

void matrix_interpolate(
    const MATRIX *from,
    const MATRIX *to,
    MATRIX *output,
    s32 blend)
{
    for (std::size_t row = 0; row < output->m.size(); ++row) {
        for (std::size_t column = 0; column < output->m[row].size(); ++column) {
            const s16 from_value = from->m[row][column];
            const s16 to_value = to->m[row][column];
            output->m[row][column] = from_value
                + (((to_value - from_value) * blend) >> KF_FIXED12_BITS);
        }
    }
}

void lighting_set_color_matrix(KfRenderState &view,
    const MATRIX *from,
    const MATRIX *to,
    s32 blend)
{
    matrix_interpolate(from, to, &view.lighting.color_matrix, blend);
}

void fog_interpolate_near(KfRenderState &view, s32 start, s32 end, s32 ratio)
{
    s32 distance = (((end - start) * ratio) >> KF_FIXED12_BITS) + start;

    view.fog_near_distance = distance;
    view.projection.fog_near = distance;
}

void fog_set_near(KfRenderState &view, s32 distance)
{
    view.fog_near_distance = distance;
    view.projection.fog_near = distance;
}

void color_lerp_cvector(
    const CVECTOR *from,
    const CVECTOR *to,
    CVECTOR *output,
    s32 blend)
{
    output->r = (((to->r - from->r) * blend) >> KF_FIXED12_BITS) + from->r;
    output->g = (((to->g - from->g) * blend) >> KF_FIXED12_BITS) + from->g;
    output->b = (((to->b - from->b) * blend) >> KF_FIXED12_BITS) + from->b;
}

u16 color_lerp_rgb555(u16 from, u16 to, s32 blend)
{
    s32 r0 = from & KF_RGB555_CHANNEL_MASK;
    s32 r1 = to & KF_RGB555_CHANNEL_MASK;
    s32 r = r0 + (((r1 - r0) * blend) >> KF_FIXED12_BITS);
    s32 g0 = (from >> KF_RGB555_GREEN_SHIFT) & KF_RGB555_CHANNEL_MASK;
    s32 g1 = (to >> KF_RGB555_GREEN_SHIFT) & KF_RGB555_CHANNEL_MASK;
    s32 g = g0 + (((g1 - g0) * blend) >> KF_FIXED12_BITS);
    s32 b0 = (from >> KF_RGB555_BLUE_SHIFT) & KF_RGB555_CHANNEL_MASK;
    s32 b1 = (to >> KF_RGB555_BLUE_SHIFT) & KF_RGB555_CHANNEL_MASK;
    s32 b = b0 + (((b1 - b0) * blend) >> KF_FIXED12_BITS);

    return r | ((from & KF_RGB555_STP) | (b << KF_RGB555_BLUE_SHIFT) | (g << KF_RGB555_GREEN_SHIFT));
}
