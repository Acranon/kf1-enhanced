#ifndef KF_LIB_RENDER_FACE_H
#define KF_LIB_RENDER_FACE_H
#include <kf/renderer/renderer.hpp>
#include <kf/platform/host.hpp>
#include <kf/lib/tmd.h>

using KfFaceShading = kf::FaceShading;

// PC extension: faces that reach behind the camera, or whose corners project past
// the screen-coordinate range, get saturated coordinates that no longer show
// which way they face (the PlayStation dropped them; widescreen and steep looks
// show the holes). Those faces are decided in camera space, where the sign of
// a . (b x c) equals the sign of the projected winding; the host then clips
// them at its near plane.
inline bool render_face_needs_camera_test(const KfScreenVertex *a, const KfScreenVertex *b, const KfScreenVertex *c)
{
    // Orthographic vertices (HUD models such as the compass needle) have no
    // camera-space position (w = 0): they keep the screen test.
    if (a->w == 0.0f || b->w == 0.0f || c->w == 0.0f)
        return false;
    for (const KfScreenVertex *v : {a, b, c}) {
        if (v->cz < kf::render_near_depth || v->fx <= -1024.0f || v->fx >= 1023.0f || v->fy <= -1024.0f || v->fy >= 1023.0f)
            return true;
    }
    return false;
}

inline std::int64_t render_face_winding(const KfScreenVertex *a,
    const KfScreenVertex *b, const KfScreenVertex *c)
{
    if (render_face_needs_camera_test(a, b, c)) {
        const double bx = b->cx, by = b->cy, bz = b->cz, cx = c->cx, cy = c->cy, cz = c->cz;
        const double triple = a->cx * (by * cz - bz * cy) + a->cy * (bz * cx - bx * cz) + a->cz * (bx * cy - by * cx);
        return triple > 0 ? 1 : triple < 0 ? -1 : 0;
    }
    // Sub-pixel vertices: test the positions actually drawn. Whole-pixel
    // corners can flip small distant faces' winding, which culls them for a
    // frame and makes models and wall joins flicker as the camera moves.
    if (kf::render_subpixel) {
        const double area = (static_cast<double>(b->fx) - a->fx) * (static_cast<double>(c->fy) - a->fy)
            - (static_cast<double>(b->fy) - a->fy) * (static_cast<double>(c->fx) - a->fx);
        return area > 0 ? 1 : area < 0 ? -1 : 0;
    }
    // Retail tests the projected coordinates, including their saturation.
    const std::int64_t ab_x = b->sxy.vector.vx - a->sxy.vector.vx;
    const std::int64_t ab_y = b->sxy.vector.vy - a->sxy.vector.vy;
    const std::int64_t ac_x = c->sxy.vector.vx - a->sxy.vector.vx;
    const std::int64_t ac_y = c->sxy.vector.vy - a->sxy.vector.vy;
    return ab_x * ac_y - ab_y * ac_x;
}

inline kf::BlendMode render_texture_blend(u16 page)
{
    constexpr kf::BlendMode modes[] = {kf::BlendMode::average, kf::BlendMode::add,
        kf::BlendMode::subtract, kf::BlendMode::add_quarter};
    return modes[(page >> kf::texture_blend_shift) & kf::texture_blend_mask];
}

inline kf::FaceMaterial render_texture_material(u16 page, u16 palette)
{
    return {kf::SurfaceKind::Texture,
        {static_cast<u16>((page & kf::texture_page_x_mask) * kf::texture_page_x_stride), static_cast<u16>((page & kf::texture_page_y_mask) * kf::texture_page_y_scale),
         static_cast<u16>((palette & kf::palette_x_mask) * kf::palette_x_stride), static_cast<u16>((palette >> kf::palette_y_shift) & (kf::texture_store_height - 1)),
         static_cast<kf::TextureFormat>((page >> kf::texture_format_shift) & kf::texture_format_mask)}, render_texture_blend(page)};
}

inline void render_face_vertex(kf::DrawFace *face, unsigned index, const KfScreenVertex *vertex)
{
    auto &out = face->vertices[index];
    if (kf::render_subpixel) {
        out.x = vertex->fx;
        out.y = vertex->fy;
    } else {
        out.x = vertex->sxy.vector.vx;
        out.y = vertex->sxy.vector.vy;
    }
    out.w = kf::render_perspective ? vertex->w : 0.0f;
    // cz = 0 marks camera-fixed and orthographic vertices (w = 0: HUD models such
    // as the compass needle); world vertices may lie behind the camera (negative
    // cz) and are clipped by the host.
    const bool world = !kf::render_camera_fixed && vertex->w > 0.0f;
    out.cx = world ? vertex->cx : 0.0f;
    out.cy = world ? vertex->cy : 0.0f;
    out.cz = world ? (vertex->cz == 0.0f ? -0.001f : vertex->cz) : 0.0f;
}

inline void render_face_uv(kf::DrawFace *face, unsigned index, u16 uv)
{
    face->vertices[index].u = (uv & kf::packed_uv_component_mask) / kf::texture_uv_scale;
    face->vertices[index].v = (uv >> kf::packed_uv_component_bits) / kf::texture_uv_scale;
}

inline void render_face_submit(kf::DrawFace *face, const CVECTOR *colors,
    KfFaceShading shading, s32 depth)
{
    const float divisor = face->material.kind == kf::SurfaceKind::Texture ? kf::texture_color_unity : kf::color8_scale;
    for (unsigned i = 0; i < static_cast<unsigned>(face->shape); ++i) {
        const auto &color = colors[shading == KfFaceShading::Flat ? 0 : i];
        face->vertices[i].r = color.r / divisor;
        face->vertices[i].g = color.g / divisor;
        face->vertices[i].b = color.b / divisor;
        face->vertices[i].a = 1;
    }
    face->depth = depth;
    face->shading = shading;
    kf::host_enqueue_face(*face);
}

inline void render_face_rectangle(kf::DrawFace *face, s16 x, s16 y, s16 right, s16 bottom)
{
    face->shape = kf::FaceShape::Quad;
    face->vertices[0].x = face->vertices[2].x = x;
    face->vertices[1].x = face->vertices[3].x = right;
    face->vertices[0].y = face->vertices[1].y = y;
    face->vertices[2].y = face->vertices[3].y = bottom;
}

inline void render_face_uv_rectangle(kf::DrawFace *face, u8 u, u8 v, u8 right, u8 bottom)
{
    face->vertices[0].u = face->vertices[2].u = u / kf::texture_uv_scale;
    face->vertices[1].u = face->vertices[3].u = right / kf::texture_uv_scale;
    face->vertices[0].v = face->vertices[1].v = v / kf::texture_uv_scale;
    face->vertices[2].v = face->vertices[3].v = bottom / kf::texture_uv_scale;
}
#endif
