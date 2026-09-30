#include <kf/lib/null.h>

#include <kf/lib/render_face.h>
#include <kf/open/render.h>

void render_enqueue_unlit_triangles(u16 object_index, s16 depth_bias)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    KfScreenVertex *vertex0;
    KfScreenVertex *vertex1;
    KfScreenVertex *vertex2;
    s32 depth;

    // Bounded reads (upstream sushi-shi #49): a malformed model fails cleanly
    // instead of reading past its resource.
    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Triangle;
            face.material = render_texture_material(p.texture_page, p.palette);
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            render_face_uv(&face, 0, p.uv[0]);
            render_face_uv(&face, 1, p.uv[1]);
            render_face_uv(&face, 2, p.uv[2]);
            colors[0] = {open_graphics_runtime.floor_item_state.material.color.r, open_graphics_runtime.floor_item_state.material.color.g, open_graphics_runtime.floor_item_state.material.color.b, 0};
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_F3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Triangle;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            colors[0] = {p.color.r, p.color.g, p.color.b, 0};
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        default:
            continue;
        }
    }
}
