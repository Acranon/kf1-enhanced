#include <kf/platform/prelude.h>
#include <kf/lib/render_face.h>

std::optional<KfProjectedFace> render_projected_triangle(
    std::span<const KfScreenVertex, KF_PROJECTED_VERTEX_CAPACITY> vertices,
    u16 first, u16 second, u16 third)
{
    const auto &a = vertices[first];
    const auto &b = vertices[second];
    const auto &c = vertices[third];
    if (render_face_winding(&a, &b, &c) <= 0)
        return std::nullopt;
    return KfProjectedFace{{a, b, c, {}}, kf::FaceShape::Triangle};
}

void KfProjectedFace::complete_quad(const KfScreenVertex &fourth)
{
    // Call only after the first triangle survives winding rejection.
    corners[3] = fourth;
    shape = kf::FaceShape::Quad;
}

kf::DrawFace KfProjectedFace::draw_face() const
{
    kf::DrawFace face{};
    face.shape = shape;
    for (std::size_t i = 0; i < static_cast<unsigned>(shape); ++i)
        render_face_vertex(&face, i, &corners[i]);
    return face;
}

s32 KfProjectedFace::ordering_depth() const
{
    const s32 triangle_sum = corners[0].sz + corners[1].sz + corners[2].sz;
    if (shape == kf::FaceShape::Triangle)
        return (triangle_sum / 3) >> KF_GTE_DEPTH_TO_OT_SHIFT;
    return (triangle_sum + corners[3].sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
}

s32 KfProjectedFace::average_fog() const
{
    const s32 triangle_sum = corners[0].p2 + corners[1].p2 + corners[2].p2;
    if (shape == kf::FaceShape::Triangle)
        return triangle_sum / 3;
    return (triangle_sum + corners[3].p2) >> 2;
}

void render_face_uvs(kf::DrawFace &face, std::initializer_list<u16> texcoords)
{
    unsigned corner = 0;
    for (const auto uv : texcoords)
        render_face_uv(&face, corner++, uv);
}
