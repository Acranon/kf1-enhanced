#include <kf/lib/null.h>

#include <kf/lib/render_face.h>
#include <kf/open/render.h>

CVECTOR tmd_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void render_enqueue_tmd(u16 object_index, s16 depth_bias, const MATRIX *lights)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    KfScreenVertex *vertex0;
    KfScreenVertex *vertex1;
    KfScreenVertex *vertex2;
    KfScreenVertex *vertex3;
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, (vertex0->p2 + vertex1->p2 + vertex2->p2) / 3);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_F4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = tmd_projected_vertex(p.vertices[3]);
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Quad;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            render_face_vertex(&face, 3, vertex3);
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, (vertex0->p2 + vertex1->p2 + vertex2->p2 + vertex3->p2) >> 2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_G3: {
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, vertex0->p2);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, vertex0->p2);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, vertex0->p2);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_G4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = tmd_projected_vertex(p.vertices[3]);
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Quad;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            render_face_vertex(&face, 3, vertex3);
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, vertex0->p2);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, vertex0->p2);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, vertex0->p2);
            colors[3] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[3]), p.color, vertex0->p2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_GT3: {
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, vertex0->p2);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), tmd_textured_primitive_color, vertex0->p2);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), tmd_textured_primitive_color, vertex0->p2);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = tmd_projected_vertex(p.vertices[3]);
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, vertex0->p2);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), tmd_textured_primitive_color, vertex0->p2);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), tmd_textured_primitive_color, vertex0->p2);
            colors[3] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[3]), tmd_textured_primitive_color, vertex0->p2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case (KF_TMD_MODE_G3 | KF_TMD_MODE_SEMITRANS): {
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
            face.transparency = kf::FaceTransparency::Blend;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, 0);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, 0);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, 0);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = tmd_projected_vertex(p.vertices[3]);
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, (vertex0->p2 + vertex1->p2 + vertex2->p2 + vertex3->p2) >> 2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + depth_bias;
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, (vertex0->p2 + vertex1->p2 + vertex2->p2) / 3);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case (KF_TMD_MODE_G4 | KF_TMD_MODE_SEMITRANS): {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = tmd_projected_vertex(p.vertices[3]);
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Quad;
            face.transparency = kf::FaceTransparency::Blend;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            render_face_vertex(&face, 3, vertex3);
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, vertex0->p2);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, vertex0->p2);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, vertex0->p2);
            colors[3] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[3]), p.color, vertex0->p2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case (KF_TMD_MODE_F3 | KF_TMD_MODE_SEMITRANS): {
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
            face.transparency = kf::FaceTransparency::Blend;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, (vertex0->p2 + vertex1->p2 + vertex2->p2) / 3);
            depth = (((vertex0->sz + vertex1->sz + vertex2->sz) / 3)
                >> KF_GTE_DEPTH_TO_OT_SHIFT) + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, KfFaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case (KF_TMD_MODE_F4 | KF_TMD_MODE_SEMITRANS): {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            vertex0 = tmd_projected_vertex(p.vertices[0]);
            vertex1 = tmd_projected_vertex(p.vertices[1]);
            vertex2 = tmd_projected_vertex(p.vertices[2]);
            if (render_face_winding(vertex0, vertex1, vertex2) <= 0) {
                continue;
            }
            vertex3 = tmd_projected_vertex(p.vertices[3]);
            kf::DrawFace face {};
            CVECTOR colors[4] {};
            face.shape = kf::FaceShape::Quad;
            face.transparency = kf::FaceTransparency::Blend;
            render_face_vertex(&face, 0, vertex0);
            render_face_vertex(&face, 1, vertex1);
            render_face_vertex(&face, 2, vertex2);
            render_face_vertex(&face, 3, vertex3);
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, (vertex0->p2 + vertex1->p2 + vertex2->p2 + vertex3->p2) >> 2);
            depth = ((vertex0->sz + vertex1->sz + vertex2->sz + vertex3->sz)
                >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + depth_bias;
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

void render_tmd_reset_module_state(void)
{
    kf::restore_initial_value<tmd_textured_primitive_color>();
}
