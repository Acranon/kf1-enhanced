#include <kf/platform/prelude.h>
#include <kf/cutscene/render.h>
#include <kf/lib/null.h>
#include <kf/lib/render_face.h>

#include <array>

CVECTOR cutscene_tmd_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void cutscene_render_enqueue_tmd(u16 object_index, s16 depth_bias, const MATRIX *lights)
{
    const auto object = tmd_read_object(cutscene_tmd_context(), object_index);
    auto stream = tmd_primitive_stream(cutscene_tmd_context(), object);
    const auto normals = tmd_normal_bytes(cutscene_tmd_context(), object);
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), cutscene_tmd_textured_primitive_color, projected->average_fog());
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_F4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            projected->complete_quad(open_graphics_runtime.tmd_projected_vertices[p.vertices[3]]);
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_G3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->fog(0));
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, projected->fog(0));
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, projected->fog(0));
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_G4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            projected->complete_quad(open_graphics_runtime.tmd_projected_vertices[p.vertices[3]]);
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->fog(0));
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, projected->fog(0));
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, projected->fog(0));
            colors[3] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[3]), p.color, projected->fog(0));
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_GT3: {
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            projected->complete_quad(open_graphics_runtime.tmd_projected_vertices[p.vertices[3]]);
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.material = render_texture_material(p.texture_page, p.palette);
            render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            colors[3] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[3]), cutscene_tmd_textured_primitive_color, projected->fog(0));
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_G3_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.transparency = kf::FaceTransparency::Blend;
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, 0);
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, 0);
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, 0);
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            projected->complete_quad(open_graphics_runtime.tmd_projected_vertices[p.vertices[3]]);
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.material = render_texture_material(p.texture_page, p.palette);
            render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), cutscene_tmd_textured_primitive_color, projected->average_fog());
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
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_G4_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            projected->complete_quad(open_graphics_runtime.tmd_projected_vertices[p.vertices[3]]);
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.transparency = kf::FaceTransparency::Blend;
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->fog(0));
            colors[1] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[1]), p.color, projected->fog(0));
            colors[2] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[2]), p.color, projected->fog(0));
            colors[3] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[3]), p.color, projected->fog(0));
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_F3_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.transparency = kf::FaceTransparency::Blend;
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
            depth = projected->ordering_depth() + depth_bias;
            if (depth >= KF_SCENE_MIN_OT_DEPTH) {
                render_face_submit(&face, colors, kf::FaceShading::Flat, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_F4_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                open_graphics_runtime.tmd_projected_vertices,
                p.vertices[0], p.vertices[1], p.vertices[2]);
            if (!projected) {
                continue;
            }
            projected->complete_quad(open_graphics_runtime.tmd_projected_vertices[p.vertices[3]]);
            auto face = projected->draw_face();
            std::array<CVECTOR, 4> colors {};
            face.transparency = kf::FaceTransparency::Blend;
            colors[0] = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
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

void render_tmd_reset_module_state(void)
{
    kf::restore_initial_value<cutscene_tmd_textured_primitive_color>();
}
