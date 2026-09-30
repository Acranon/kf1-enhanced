#include <kf/platform/prelude.h>
#include <kf/cutscene/render.h>
#include <kf/lib/render_face.h>

#include <array>

CVECTOR cutscene_map_textured_primitive_color = {
    KF_TEXTURE_BASE_BRIGHTNESS, KF_TEXTURE_BASE_BRIGHTNESS,
    KF_TEXTURE_BASE_BRIGHTNESS, 0
};

void cutscene_render_enqueue_map(u16 object_index, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection)
{
    const auto object = tmd_read_object(cutscene_tmd_context(), object_index);
    auto stream = tmd_primitive_stream(cutscene_tmd_context(), object);
    const auto normals = tmd_normal_bytes(cutscene_tmd_context(), object);
    CVECTOR shade;

    cutscene_tmd_project_vertices(object.vertex_count, model, projection);

    while (stream.remaining != 0) {
        const auto packet = tmd_next_packet(stream);
        switch (packet.mode) {
        case KF_TMD_MODE_FT4: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 depth;

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
            shade = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), cutscene_map_textured_primitive_color, 0);
            colors[0] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(0));
            colors[1] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(1));
            colors[2] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(2));
            colors[3] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(3));
            depth = projected->ordering_depth()
                + KF_MAP_OT_DEPTH_BIAS;
            if (depth < KF_ORDERING_TABLE_LENGTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
            }
            break;
        }
        case KF_TMD_MODE_FT3: {
            const auto p = tmd_decode_face(packet, object.vertex_count);
            s32 depth;

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
            shade = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
                tmd_read_normal(normals, p.normals[0]), cutscene_map_textured_primitive_color, 0);
            colors[0] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(0));
            colors[1] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(1));
            colors[2] = kf::render_fog_color(open_graphics_runtime.render_state.lighting, shade, projected->fog(2));
            depth = projected->ordering_depth() + KF_MAP_OT_DEPTH_BIAS;
            if (depth < KF_ORDERING_TABLE_LENGTH) {
                render_face_submit(&face, colors, kf::FaceShading::Gouraud, depth & KF_ORDERING_TABLE_INDEX_MASK);
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
    kf::restore_initial_value<cutscene_map_textured_primitive_color>();
}
