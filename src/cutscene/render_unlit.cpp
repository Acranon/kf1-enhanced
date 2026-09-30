#include <kf/platform/prelude.h>
#include <kf/cutscene/render.h>
#include <kf/lib/null.h>
#include <kf/lib/render_face.h>

#include <array>

void render_enqueue_unlit_triangles(u16 object_index, s16 depth_bias)
{
    const auto object = tmd_read_object(cutscene_tmd_context(), object_index);
    auto stream = tmd_primitive_stream(cutscene_tmd_context(), object);
    s32 depth;

    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.material = render_texture_material(p.texture_page, p.palette);
            render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2]});
            colors[0] = {open_graphics_runtime.floor_item_state.material.color.r, open_graphics_runtime.floor_item_state.material.color.g, open_graphics_runtime.floor_item_state.material.color.b, 0};
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_F3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            colors[0] = {p.color.r, p.color.g, p.color.b, 0};
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        default:
            continue;
        }
    }
}
