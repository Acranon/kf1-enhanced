#include <kf/platform/prelude.h>
#include <kf/lib/graphics.h>
#include <kf/lib/render_face.h>

void sprite_add_ft4(const KfScreenRect *rectangle, std::span<const u8, KF_QUAD_TEX_DESCRIPTOR_BYTES> texcoords, const kf::FaceMaterial &material,
                   const CVECTOR *color, u16 ot_index)
{
    kf::DrawFace face {};
    face.material = material;
    render_face_rectangle(&face, rectangle->x, rectangle->y,
        rectangle->x + rectangle->w, rectangle->y + rectangle->h);
    render_face_uv_rectangle(&face, texcoords[KF_QUAD_TEX_U], texcoords[KF_QUAD_TEX_V],
        texcoords[KF_QUAD_TEX_U] + texcoords[KF_QUAD_TEX_U_SPAN],
        texcoords[KF_QUAD_TEX_V] + texcoords[KF_QUAD_TEX_V_SPAN]);
    render_face_submit(&face, {color, 1}, kf::FaceShading::Flat, ot_index & KF_ORDERING_TABLE_INDEX_MASK);
}

void sprite_add_f4(
    const KfScreenRect *rectangle, const CVECTOR *color, u16 ot_index)
{
    kf::DrawFace face {};
    render_face_rectangle(&face, rectangle->x, rectangle->y,
        rectangle->x + rectangle->w, rectangle->y + rectangle->h);
    render_face_submit(&face, {color, 1}, kf::FaceShading::Flat, ot_index & KF_ORDERING_TABLE_INDEX_MASK);
}
