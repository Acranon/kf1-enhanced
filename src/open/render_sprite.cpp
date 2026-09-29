#include <kf/lib/render_face.h>
#include <kf/open/render.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

SVECTOR render_sprite_light_normal = {0, 0, KF_FIXED12_ONE, 0};

void render_enqueue_sprite(KfSpriteQuad *sprite, s16 depth_bias, KfSpriteDepthCueMode depth_cue_mode, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection)
{
    SVECTOR corners[4];
    SVECTOR anchor{};
    s32 depth_cue;
    s32 otz;

    corners[0].vx = corners[2].vx = sprite->x;
    corners[1].vx = corners[3].vx = sprite->x + sprite->w;
    corners[0].vy = corners[1].vy = sprite->y;
    corners[2].vy = corners[3].vy = sprite->y + sprite->h;
    corners[0].vz = corners[1].vz = corners[2].vz = corners[3].vz = 0;
    otz = kf::render_project_point(*model, projection, anchor).depth >> KF_GTE_DEPTH_TO_OT_SHIFT;
    kf::ProjectedPoint positions[4];
    for (unsigned i = 0; i < 4; ++i)
        positions[i] = kf::render_project_point(*model, projection, corners[i]);
    depth_cue = positions[3].fog;

    kf::DrawFace face {};
    CVECTOR color {};
    face.shape = kf::FaceShape::Quad;
    face.casts_shadow = false;
    face.material = open_graphics_runtime.floor_item_state.material.surface;
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
    color = kf::render_light_normal(open_graphics_runtime.render_state.lighting, *lights,
        render_sprite_light_normal, open_graphics_runtime.floor_item_state.material.color, depth_cue);
    if (otz + depth_bias >= KF_SCENE_MIN_OT_DEPTH) {
        render_face_submit(&face, &color, KfFaceShading::Flat,
            (otz + depth_bias) & KF_ORDERING_TABLE_INDEX_MASK);
    }
}


void render_sprite_reset_module_state(void)
{
    kf::restore_initial_value<render_sprite_light_normal>();
}
