#include <kf/platform/prelude.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/null.h>
#include <kf/lib/render_face.h>
#include <kf/lib/tmd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

CVECTOR tmd_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void render_enqueue_tmd(u16 object_index, s16 depth_bias, const MATRIX *lights)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    const auto &vertices = game_graphics_runtime.tmd_projected_vertices;
    s32 otz;

    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        default:
            // Preserve the original enqueuer's supported packet subset.
            break;
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_F4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_G3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_G4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, projected->fog(0));
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), p.color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_GT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), tmd_textured_primitive_color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), tmd_textured_primitive_color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), tmd_textured_primitive_color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), tmd_textured_primitive_color, projected->fog(0));
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), tmd_textured_primitive_color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_G3_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.transparency = kf::FaceTransparency::Blend;
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, 0);
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, 0);
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, 0);
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), tmd_textured_primitive_color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_F3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_G4_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.transparency = kf::FaceTransparency::Blend;
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), p.color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), p.color, projected->fog(0));
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), p.color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_F3_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.transparency = kf::FaceTransparency::Blend;
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_F4_SEMITRANS: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.transparency = kf::FaceTransparency::Blend;
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), p.color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        }
    }
}

CVECTOR retextured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void render_enqueue_tmd_retextured(u16 object_index, s16 depth_bias, const MATRIX *lights)
{
    const auto object = tmd_read_object(tmd_context(), object_index);
    auto stream = tmd_primitive_stream(tmd_context(), object);
    const auto normals = tmd_normal_bytes(tmd_context(), object);
    const auto &vertices = game_graphics_runtime.tmd_projected_vertices;
    s32 otz;

    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        default:
            // This pass only draws opaque textured faces.
            break;
        case KF_TMD_MODE_GT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = game_graphics_runtime.active_render_material;
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), retextured_primitive_color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), retextured_primitive_color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), retextured_primitive_color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_GT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = game_graphics_runtime.active_render_material;
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), retextured_primitive_color, projected->fog(0));
                colors[1] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[1]), retextured_primitive_color, projected->fog(0));
                colors[2] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[2]), retextured_primitive_color, projected->fog(0));
                colors[3] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[3]), retextured_primitive_color, projected->fog(0));
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = game_graphics_runtime.active_render_material;
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), retextured_primitive_color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = game_graphics_runtime.active_render_material;
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
                colors[0] = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), retextured_primitive_color, projected->average_fog());
                otz = projected->ordering_depth();
                if (otz + depth_bias > (KF_SCENE_MIN_OT_DEPTH - 1)) {
                    render_face_submit(&face, colors, kf::FaceShading::Flat, (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
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
    const auto &vertices = game_graphics_runtime.tmd_projected_vertices;
    CVECTOR shade;

    tmd_project_vertices(object.vertex_count, model, projection);
    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        default:
            // Map cells only submit flat-shaded opaque textured faces.
            break;
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 otz;

            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                projected->complete_quad(vertices[p.vertices[3]]);
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2], p.uv[3]});
                shade = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), map_textured_primitive_color, 0);
                colors[0] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(0));
                colors[1] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(1));
                colors[2] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(2));
                colors[3] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(3));
                otz = projected->ordering_depth() + KF_MAP_OT_DEPTH_BIAS;
                if (otz < KF_ORDERING_TABLE_LENGTH) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, otz & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 otz;

            auto projected = render_projected_triangle(
                vertices, p.vertices[0], p.vertices[1], p.vertices[2]);
            if (projected) {
                auto face = projected->draw_face();
                std::array<CVECTOR, 4> colors {};
                face.material = render_texture_material(p.texture_page, p.palette);
                render_face_uvs(face, {p.uv[0], p.uv[1], p.uv[2]});
                shade = kf::render_light_normal(game_graphics_runtime.render_state.lighting, *lights,
                    tmd_read_normal(normals, p.normals[0]), map_textured_primitive_color, 0);
                colors[0] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(0));
                colors[1] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(1));
                colors[2] = kf::render_fog_color(game_graphics_runtime.render_state.lighting, shade, projected->fog(2));
                otz = projected->ordering_depth() + KF_MAP_OT_DEPTH_BIAS;
                if (otz < KF_ORDERING_TABLE_LENGTH) {
                    render_face_submit(&face, colors, kf::FaceShading::Gouraud, otz & KF_ORDERING_TABLE_INDEX_MASK);
                }
            }
            break;
        }
        }
    }
}

void render_enqueue_sprite(KfSpriteQuad *sprite, s16 depth_bias, KfSpriteDepthCueMode depth_cue_mode, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection)
{
    std::array<SVECTOR, 4> corners;
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
    std::array<kf::ProjectedPoint, 4> positions;
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
        render_face_submit(&face, {&color, 1}, kf::FaceShading::Flat,
            (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
    }
}

void render_enqueuers_reset_module_state(void)
{
    kf::restore_initial_value<tmd_textured_primitive_color>();
    kf::restore_initial_value<retextured_primitive_color>();
    kf::restore_initial_value<map_textured_primitive_color>();
    kf::restore_initial_value<render_sprite_light_normal>();
}
