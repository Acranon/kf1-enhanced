#include <kf/lib/null.h>
#include <kf/game/graphics.h>

#include <kf/lib/geometry_types.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/game/render.h>
#include <kf/lib/tmd.h>
#include <kf/lib/render_face.h>

CVECTOR tmd_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void render_enqueue_tmd(u16 object_index, s16 depth_bias, const MATRIX *lights)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    const auto *vertices = game_graphics_runtime.tmd_projected_vertices;
    s32 otz;
    const KfScreenVertex *va;
    const KfScreenVertex *vb;
    const KfScreenVertex *vc;
    const KfScreenVertex *vd;

    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, (va->p2 + vb->p2 + vc->p2) / 3);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_F4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, (va->p2 + vb->p2 + vc->p2 + vd->p2) >> 2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_G3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, va->p2);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_G4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, va->p2);
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), p.color, va->p2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_GT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), tmd_textured_primitive_color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), tmd_textured_primitive_color, va->p2);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                render_face_uv(&face, 3, p.uv[3]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), tmd_textured_primitive_color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), tmd_textured_primitive_color, va->p2);
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), tmd_textured_primitive_color, va->p2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case (KF_TMD_MODE_G3 | KF_TMD_MODE_SEMITRANS): {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.transparency = kf::FaceTransparency::Blend;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, 0);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, 0);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, 0);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                render_face_uv(&face, 3, p.uv[3]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, (va->p2 + vb->p2 + vc->p2 + vd->p2) >> 2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_F3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, (va->p2 + vb->p2 + vc->p2) / 3);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case (KF_TMD_MODE_G4 | KF_TMD_MODE_SEMITRANS): {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.transparency = kf::FaceTransparency::Blend;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, va->p2);
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), p.color, va->p2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case (KF_TMD_MODE_F3 | KF_TMD_MODE_SEMITRANS): {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.transparency = kf::FaceTransparency::Blend;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, (va->p2 + vb->p2 + vc->p2) / 3);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case (KF_TMD_MODE_F4 | KF_TMD_MODE_SEMITRANS): {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];

            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.transparency = kf::FaceTransparency::Blend;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, (va->p2 + vb->p2 + vc->p2 + vd->p2) >> 2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        }
    }
}

CVECTOR model_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void render_enqueue_model(u16 object_index, s16 depth_bias, const MATRIX *lights)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    const auto *vertices = game_graphics_runtime.tmd_projected_vertices;
    s32 otz;
    const KfScreenVertex *va;
    const KfScreenVertex *vb;
    const KfScreenVertex *vc;
    const KfScreenVertex *vd;

    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        case KF_TMD_MODE_GT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];
            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.material = game_graphics_runtime.active_render_material;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), model_textured_primitive_color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), model_textured_primitive_color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), model_textured_primitive_color, va->p2);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];
            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.material = game_graphics_runtime.active_render_material;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                render_face_uv(&face, 3, p.uv[3]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), model_textured_primitive_color, va->p2);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), model_textured_primitive_color, va->p2);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), model_textured_primitive_color, va->p2);
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), model_textured_primitive_color, va->p2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];
            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.material = game_graphics_runtime.active_render_material;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), model_textured_primitive_color, (va->p2 + vb->p2 + vc->p2) / 3);
                otz = (va->sz + vb->sz + vc->sz) / 3 >> KF_GTE_DEPTH_TO_OT_SHIFT;
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];
            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.material = game_graphics_runtime.active_render_material;
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                render_face_uv(&face, 3, p.uv[3]);
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), model_textured_primitive_color, (va->p2 + vb->p2 + vc->p2 + vd->p2) >> 2);
                otz = (va->sz + vb->sz + vc->sz + vd->sz) >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2);
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, KfFaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        }
    }
}

CVECTOR map_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

SVECTOR render_sprite_light_normal = {0, 0, KF_FIXED12_ONE, 0};

void render_enqueue_map(u16 object_index, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    const auto *vertices = game_graphics_runtime.tmd_projected_vertices;
    CVECTOR shade;
    const KfScreenVertex *va;
    const KfScreenVertex *vb;
    const KfScreenVertex *vc;
    const KfScreenVertex *vd;

    tmd_project_vertices(object.vertex_count, model, projection);
    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 otz;

            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];
            if (render_face_winding(va, vb, vc) > 0) {
                vd = &vertices[p.vertices[3]];
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Quad;
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_vertex(&face, 3, vd);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                render_face_uv(&face, 3, p.uv[3]);
                shade = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), map_textured_primitive_color, 0);
                colors[0] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, va->p2);
                colors[1] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, vb->p2);
                colors[2] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, vc->p2);
                colors[3] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, vd->p2);
                otz = ((va->sz + vb->sz + vc->sz + vd->sz)
                    >> (KF_GTE_DEPTH_TO_OT_SHIFT + 2)) + KF_MAP_OT_DEPTH_BIAS;
                if (otz < KF_ORDERING_TABLE_LENGTH) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, otz & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 otz;

            va = &vertices[p.vertices[0]];
            vb = &vertices[p.vertices[1]];
            vc = &vertices[p.vertices[2]];
            if (render_face_winding(va, vb, vc) > 0) {
                kf::DrawFace face {};
                CVECTOR colors[4] {};
                face.shape = kf::FaceShape::Triangle;
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_vertex(&face, 0, va);
                render_face_vertex(&face, 1, vb);
                render_face_vertex(&face, 2, vc);
                render_face_uv(&face, 0, p.uv[0]);
                render_face_uv(&face, 1, p.uv[1]);
                render_face_uv(&face, 2, p.uv[2]);
                shade = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), map_textured_primitive_color, 0);
                colors[0] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, va->p2);
                colors[1] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, vb->p2);
                colors[2] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, vc->p2);
                otz = ((va->sz + vb->sz + vc->sz) / 3
                    >> KF_GTE_DEPTH_TO_OT_SHIFT) + KF_MAP_OT_DEPTH_BIAS;
                if (otz < KF_ORDERING_TABLE_LENGTH) {
                    render_face_submit(&face, colors, KfFaceShading::Gouraud, otz & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        }
    }
}

void render_enqueue_sprite(KfSpriteQuad *sprite, s16 depth_bias, KfSpriteDepthCueMode depth_cue_mode, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection)
{
    SVECTOR corners[4];
    SVECTOR anchor;
    s32 depth_cue;
    s32 otz;

    corners[0].vx = corners[2].vx = sprite->x;
    corners[1].vx = corners[3].vx = sprite->x + sprite->w;
    corners[0].vy = corners[1].vy = sprite->y;
    corners[2].vy = corners[3].vy = sprite->y + sprite->h;
    corners[0].vz = corners[1].vz = corners[2].vz = corners[3].vz = 0;
    anchor.vx = anchor.vy = anchor.vz = 0;
    otz = kf::render_project_point(*model, projection, anchor).depth >> KF_GTE_DEPTH_TO_OT_SHIFT;
    kf::ProjectedPoint positions[4];
    for (unsigned i = 0; i < 4; ++i)
        positions[i] = kf::render_project_point(*model, projection, corners[i]);
    depth_cue = positions[3].fog;

    kf::DrawFace face {};
    CVECTOR color {};
    face.shape = kf::FaceShape::Quad;
    face.casts_shadow = false;
    face.material = game_graphics_runtime.active_render_material;
    for (unsigned i = 0; i < 4; ++i) {
        face.vertices[i].x = kf::render_subpixel ? positions[i].fx : positions[i].x;
        face.vertices[i].y = kf::render_subpixel ? positions[i].fy : positions[i].y;
        face.vertices[i].w = kf::render_perspective ? positions[i].w : 0.0f;
        const bool world = !kf::render_camera_fixed && positions[i].cz > 0;
        face.vertices[i].cx = world ? positions[i].cx : 0.0f;
        face.vertices[i].cy = world ? positions[i].cy : 0.0f;
        face.vertices[i].cz = world ? positions[i].cz : 0.0f;
    }
    render_face_uv_rectangle(&face, sprite->u, sprite->v,
        sprite->u + sprite->u_span, sprite->v + sprite->v_span);
    if (depth_cue_mode == KF_SPRITE_DEPTH_CUE_BOOSTED) {
        depth_cue += depth_cue >> 1;
    }
    color = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
        render_sprite_light_normal, game_graphics_runtime.active_render_color, depth_cue);
    if (otz + depth_bias >= KF_SCENE_MIN_OT_DEPTH) {
        render_face_submit(&face, &color, KfFaceShading::Flat,
            (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
    }
}

void render_enqueuers_reset_module_state(void)
{
    kf::restore_initial_value<tmd_textured_primitive_color>();
    kf::restore_initial_value<model_textured_primitive_color>();
    kf::restore_initial_value<map_textured_primitive_color>();
    kf::restore_initial_value<render_sprite_light_normal>();
}
