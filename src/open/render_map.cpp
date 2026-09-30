#include <kf/lib/render_face.h>
#include <kf/open/render.h>

CVECTOR map_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void render_enqueue_map(u16 object_index, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    KfScreenVertex *vertex0;
    KfScreenVertex *vertex1;
    KfScreenVertex *vertex2;
    KfScreenVertex *vertex3;
    CVECTOR shade;

    tmd_project_vertices(object.vertex_count, model, projection);
    // Bounded reads (upstream sushi-shi #49): a malformed model fails cleanly
    // instead of reading past its resource.
    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        KfScreenVertex *vertices = open_graphics_runtime.tmd_projected_vertices;

        switch (packet.mode) {
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 depth;

            vertex0 = &vertices[p.vertices[0]];
            vertex1 = &vertices[p.vertices[1]];
            vertex2 = &vertices[p.vertices[2]];
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = &vertices[p.vertices[3]];
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Quad;
            face.material = render_texture_material(p.texture_page, p.palette);
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            render_face_vertex(&face, 3, vertex3);
            render_face_uv(&face, 0, p.uv[0]);
            render_face_uv(&face, 1, p.uv[1]);
            render_face_uv(&face, 2, p.uv[2]);
            render_face_uv(&face, 3, p.uv[3]);
            shade = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), map_textured_primitive_color, 0);
            colors[0] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex0->p2);
            colors[1] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex1->p2);
            colors[2] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex2->p2);
            colors[3] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex3->p2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2))
                + KF_MAP_OT_DEPTH_BIAS;
            if (depth < KF_ORDERING_TABLE_LENGTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 depth;

            vertex0 = &vertices[p.vertices[0]];
            vertex1 = &vertices[p.vertices[1]];
            vertex2 = &vertices[p.vertices[2]];
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
            shade = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), map_textured_primitive_color, 0);
            colors[0] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex0->p2);
            colors[1] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex1->p2);
            colors[2] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, vertex2->p2);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + KF_MAP_OT_DEPTH_BIAS;
            if (depth < KF_ORDERING_TABLE_LENGTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        default:
            continue;
        }
    }
}

void render_map_reset_module_state(void)
{
    kf::restore_initial_value<map_textured_primitive_color>();
}
