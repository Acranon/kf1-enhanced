#include <kf/platform/prelude.h>
#include <kf/game/asset.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/game/state.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/math.h>
#include <kf/lib/null.h>

#include <array>

enum { EFFECT_MODEL_DEPTH_BIAS = 100 };

std::array<KfSpriteQuad, KF_FLOOR_ITEM_SPRITE_COUNT> floor_item_sprites = {
    KfSpriteQuad{0x90, 0x0, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0xb0, 0x0, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0xd0, 0x0, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0xb0, 0x0, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0x90, 0x20, 0x20, 0x27, 0xfe00, 0xfb40, 0x400, 0x500},
    KfSpriteQuad{0xb0, 0x20, 0x20, 0x27, 0xfe00, 0xfb40, 0x400, 0x500},
    KfSpriteQuad{0xd0, 0x20, 0x20, 0x27, 0xfe00, 0xfb40, 0x400, 0x500},
};

std::array<KfSpriteQuad, KF_EFFECT_BILLBOARD_SPRITE_COUNT> effect_billboard_sprites = {
    KfSpriteQuad{0x0, 0x0, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0x30, 0x0, 0x2f, 0x2f, 0xfde7, 0xfde7, 0x433, 0x433},
    KfSpriteQuad{0x30, 0x0, 0x2f, 0x2f, 0xfd74, 0xfd74, 0x519, 0x519},
    KfSpriteQuad{0x60, 0x0, 0x2f, 0x2f, 0xfd4d, 0xfd4d, 0x566, 0x566},
    KfSpriteQuad{0x60, 0x0, 0x2f, 0x2f, 0xfd3a, 0xfd3a, 0x58c, 0x58c},
    KfSpriteQuad{0x0, 0x30, 0x2f, 0x2f, 0xfe34, 0xfe34, 0x399, 0x399},
    KfSpriteQuad{0x0, 0x60, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0x30, 0x60, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0x60, 0x30, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0x30, 0x30, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0x60, 0x60, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0xe0, 0x48, 0x17, 0x5c, 0xff38, 0xec78, 0x190, 0x1388},
    KfSpriteQuad{0xe0, 0x76, 0x17, 0x5c, 0xff38, 0xec78, 0x190, 0x1388},
    KfSpriteQuad{0xe0, 0xa2, 0x17, 0x5c, 0xff38, 0xec78, 0x190, 0x1388},
    KfSpriteQuad{0xe0, 0x48, 0x17, 0x5c, 0xfed4, 0xec78, 0x258, 0x1388},
    KfSpriteQuad{0xe0, 0x76, 0x17, 0x5c, 0xfed4, 0xec78, 0x258, 0x1388},
    KfSpriteQuad{0xe0, 0xa2, 0x17, 0x5c, 0xfed4, 0xec78, 0x258, 0x1388},
    KfSpriteQuad{0x0, 0x90, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0x30, 0x90, 0x2f, 0x2f, 0xfe80, 0xfe80, 0x300, 0x300},
    KfSpriteQuad{0xc8, 0x48, 0x17, 0x5c, 0xff38, 0xec78, 0x190, 0x1388},
    KfSpriteQuad{0xc8, 0x76, 0x17, 0x5c, 0xff38, 0xec78, 0x190, 0x1388},
    KfSpriteQuad{0xc8, 0xa2, 0x17, 0x5c, 0xff38, 0xec78, 0x190, 0x1388},
};

void render_effect(KfEffectRecord *effect, const MATRIX *lights)
{
    SVECTOR relative_position;
    VECTOR scale;
    MATRIX model;
    u16 asset;
    KfTmdObject *object;

    if (effect->render_id.model == KF_EFFECT_MODEL_NONE) {
        return;
    }
    // PC extension (dynamic lighting): fire and lightning projectiles light their surroundings.
    const auto billboard = effect->render_id.billboard;
    if (effect->animation_clip == KF_ANIMATION_CLIP_NONE) {
        if (billboard <= KF_EFFECT_BILLBOARD_FIRE_BALL_FRAME_4) {
            render_publish_light(effect->position.vx, effect->position.vy, effect->position.vz,
                6000.0f, 1.2f, 0.55f, 0.2f, 0.6f);
        } else if (billboard == KF_EFFECT_BILLBOARD_LIGHTNING_BOLT || billboard == KF_EFFECT_BILLBOARD_LIGHTNING_BOLT_FRAME_1
                   || billboard == KF_EFFECT_BILLBOARD_LIGHTNING_BOLT_ALTERNATE
                   || billboard == KF_EFFECT_BILLBOARD_LIGHTNING_ALTERNATE_FRAME_1
                   || (billboard >= KF_EFFECT_BILLBOARD_LIGHTNING_IMPACT && billboard <= KF_EFFECT_BILLBOARD_LIGHTNING_IMPACT_FRAME_2)
                   || (billboard >= KF_EFFECT_BILLBOARD_LIGHTNING_IMPACT_ALTERNATE && billboard <= KF_EFFECT_BILLBOARD_IMPACT_ALTERNATE_FRAME_2)) {
            render_publish_light(effect->position.vx, effect->position.vy, effect->position.vz,
                6000.0f, 0.55f, 0.7f, 1.3f, 0.9f);
        }
    }
    relative_position = VECTOR{
        effect->position.vx - game_graphics_runtime.render_state.view_position.vx,
        effect->position.vy - game_graphics_runtime.render_state.view_position.vy,
        effect->position.vz - game_graphics_runtime.render_state.view_position.vz}.narrowed();
    kf::render_place_model(model, game_graphics_runtime.render_state.view_matrix, relative_position);
    matrix_set_rotation_yxz(&effect->rotation.angles, &model);
    scale = {(s16)effect->scale_x, (s16)effect->scale_y, (s16)effect->scale_z};
    kf::matrix_scale_axes(model, scale);
    if (effect->animation_clip == KF_ANIMATION_CLIP_NONE) {
        kf::matrix_multiply_rotation(game_graphics_runtime.render_state.pitch_matrix, model, model);
        render_enqueue_sprite(&effect_billboard_sprites[kf_enum_encode<u8>(effect->render_id.billboard)], 0, KF_SPRITE_DEPTH_CUE_NORMAL, lights, &model, game_graphics_runtime.render_state.projection);
    } else {
        kf::matrix_multiply_rotation(game_graphics_runtime.render_state.view_matrix, model, model);
        asset = kf_enum_encode<u8>(effect->render_id.model) + KF_ASSET_EFFECT_FIRST;
        asset_registry_select(asset);
        object = tmd_get_object(tmd_context(), 0);
        if (!render_bind_instance_vertices(
                &effect->animation_cache, asset, effect->animation_clip, effect->visual.animation_phase,
                object->vertex_count)) {
            tmd_select_object_vertices(tmd_context(), 0);
            tmd_project_vertices(tmd_get_object(tmd_context(), 0)->vertex_count, &model, game_graphics_runtime.render_state.projection);
        } else {
            tmd_project_vertices(object->vertex_count, &model, game_graphics_runtime.render_state.projection);
        }
        render_enqueue_tmd(0, EFFECT_MODEL_DEPTH_BIAS, lights);
    }
}

void entity_render_reset_module_state(void)
{
    kf::restore_initial_value<floor_item_sprites>();
    kf::restore_initial_value<effect_billboard_sprites>();
}
