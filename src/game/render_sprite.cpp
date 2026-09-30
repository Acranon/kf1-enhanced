#include <kf/platform/prelude.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/render_face.h>

enum { HUD_SPRITE_OT_DEPTH = 1 };

void render_screen_sprite(KfSpriteQuad *sprite)
{
    kf::DrawFace face {};
    face.material = game_graphics_runtime.active_render_material;
    render_face_rectangle(&face, sprite->x, sprite->y,
        sprite->x + sprite->w, sprite->y + sprite->h);
    render_face_uv_rectangle(&face, sprite->u, sprite->v,
        sprite->u + sprite->u_span, sprite->v + sprite->v_span);
    render_face_submit(&face, {&game_graphics_runtime.active_render_color, 1},
        kf::FaceShading::Flat, HUD_SPRITE_OT_DEPTH);
}
