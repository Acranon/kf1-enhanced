#ifndef KF_GAME_RENDER_H
#define KF_GAME_RENDER_H

#include <kf/game/actor.h>
#include <kf/game/animation_cache.h>
#include <kf/game/effect.h>
#include <kf/lib/enum.h>
#include <kf/lib/graphics.h>
#include <kf/lib/item.h>
#include <kf/lib/map.h>
#include <kf/lib/render_types.h>
#include <kf/lib/tmd.h>

#include <array>

enum class KfGameColorPreset : s32 {
    KF_GAME_COLOR_DEFAULT = 0,
    KF_GAME_COLOR_DAMAGE = 1,
    KF_GAME_COLOR_DEFENSE_EFFECT = 2,
    KF_GAME_COLOR_WHITE = 3,
    KF_GAME_COLOR_BLACK = 4,
    KF_GAME_COLOR_GREEN = 5,
    KF_GAME_COLOR_BLUE = 6
}; using enum KfGameColorPreset;

enum class KfSystemScreen : s32 {
    KF_SYSTEM_SCREEN_CD_SEARCH_FAILED = 0,
    KF_SYSTEM_SCREEN_CD_READ_FAILED = 1,
    KF_SYSTEM_SCREEN_NO_MEMORY_CARD = 2,
    KF_SYSTEM_SCREEN_PAUSE = 3
}; using enum KfSystemScreen;

enum {
    KF_SYSTEM_SCREEN_LEFT = 32,
    KF_SYSTEM_SCREEN_TOP = 112,
    KF_SYSTEM_SCREEN_RIGHT = 288,
    KF_SYSTEM_SCREEN_BOTTOM = 240,
    KF_SYSTEM_SCREEN_U_SPAN = 255,
    KF_SYSTEM_SCREEN_V_SPAN = 128,
    KF_SYSTEM_SCREEN_TPAGE_X = 960,
    KF_SYSTEM_SCREEN_CLUT_Y = 501
};

enum {
    KF_GAME_COLOR_PRESET_COUNT = 7,
    KF_GAME_TMD_SLOT_COUNT = 8,
    KF_HUD_DEFAULT_BRIGHTNESS = 86
};

enum {
    KF_HUD_HP_GAUGE = 0,
    KF_HUD_MP_GAUGE = 1,
    KF_HUD_ATTACK_GAUGE = 2,
    KF_HUD_MAGIC_GAUGE = 3,
    KF_HUD_POISON_ICON = 4,
    KF_HUD_SLOWED_ICON = 5,
    KF_HUD_DARKNESS_ICON = 6,
    KF_HUD_CURSE_ICON = 7,
    KF_HUD_HP_PANEL = 8,
    KF_HUD_MP_PANEL = 9,
    KF_HUD_ATTACK_PANEL = 10,
    KF_HUD_MAGIC_PANEL = 11,
    KF_HUD_COMPASS = 12,
    KF_HUD_TABLE_ROWS = 14
};

enum class KfSpriteState : u8 {
    KF_SPRITE_HIDDEN = 0,
    KF_SPRITE_VISIBLE = 1,
    KF_SPRITE_END = 0xff
}; using enum KfSpriteState;

enum {
    KF_HUD_MODEL_COMPASS = 0,
    KF_HUD_MODEL_TABLE_ROWS = 2
};

enum {
    KF_RENDER_LIGHT_ACTOR = 0,
    KF_RENDER_LIGHT_FLOOR_ITEM = 1,
    KF_RENDER_LIGHT_EFFECT = 2,
    KF_RENDER_LIGHT_WEAPON = 3,
    KF_RENDER_LIGHT_HUD = 4,
    KF_RENDER_LIGHT_NOTIFICATION = 5,
    KF_RENDER_LIGHT_COUNT = 6
};

typedef struct KfHudSprite {
    KfSpriteState state;
    u8 unknown_01;
    KfSpriteQuad sprite;
} KfHudSprite;

typedef struct KfHudModel {
    KfSpriteState state;
    KfAnimationClip animation_clip;
    u16 animation_phase;
    u16 scale;
    s16 translation_x;
    s16 translation_y;
    s16 translation_z;
    std::array<u8, 2> unknown_0c;
    SVECTOR rotation;
    std::array<u8, 2> unknown_16;
    KfAnimationCacheRecord *animation_cache;
} KfHudModel;

typedef struct KfTmdState {
    std::array<KfTmdResource, KF_GAME_TMD_SLOT_COUNT> slots;
    KfTmdResource current_tmd;
} KfTmdState;

extern std::array<MATRIX, KF_GAME_COLOR_PRESET_COUNT> color_matrix_table;
extern std::array<KfCellWindow, KF_CELL_WINDOW_YAW_COUNT> render_cell_windows;
extern std::array<KfSpriteQuad, KF_FLOOR_ITEM_SPRITE_COUNT> floor_item_sprites;
void render_publish_light(s32 x, s32 y, s32 z, float radius, float r, float g, float b, float flicker);
extern std::array<KfSpriteQuad, KF_EFFECT_BILLBOARD_SPRITE_COUNT> effect_billboard_sprites;
extern std::array<KfHudModel, KF_HUD_MODEL_TABLE_ROWS> hud_models;
extern std::array<KfHudSprite, KF_HUD_TABLE_ROWS> hud_sprites;
extern std::array<MATRIX, KF_RENDER_LIGHT_COUNT> render_light_matrices;

extern void display_flip_buffer_index(void);
extern void display_initialize(void);
extern void display_play_transition(void);
extern void display_show_system_screen(KfSystemScreen screen);
extern void display_present_system_screen(s32 brightness);
extern void render_prepare_actor_textures(KfFloorId floor);
extern void lighting_apply_blue_tint(void);
extern void lighting_apply_illusion_staff_effect(void);
extern void lighting_apply_shadow_blade_environment(void);
extern void lighting_set_active_color_matrix(KfGameColorPreset preset);
extern void menu_render_item_model(const MATRIX *lights, const MATRIX *model);
extern void render_actor(KfActor *actor);
extern void render_effect(KfEffectRecord *effect, const MATRIX *lights);
extern void render_hud_models(const MATRIX *lights);
extern void render_enqueue_map(u16 object_index, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection);
extern void render_enqueue_tmd_retextured(u16 object_index, s16 depth_bias, const MATRIX *lights);
extern void render_enqueue_sprite(KfSpriteQuad *effect, s16 depth_bias, KfSpriteDepthCueMode depth_cue_mode, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection);
extern void render_enqueue_tmd(u16 object_index, s16 depth_bias, const MATRIX *lights);
extern void render_entities(void);
// Advances and presents one world frame, including the shared three-tick wait.
// Callers must not add another gameplay interval; menus/retained frames are separate.
extern void render_frame(
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null);
extern void render_hud_sprites(KfHudSprite *table);
extern void render_initialize(void);
extern void render_map_cell(
    s32 col, s32 row, KfCellVisibility visibility);
extern void render_map_cells(void);
extern void render_map_event(KfMapEvent *event, const MATRIX *lights);
extern void render_map_object(KfMapObject *object);
extern void render_screen_sprite(KfSpriteQuad *sprite);
extern void render_weapon(void);
extern void screen_show_image_until_input(const char *path);

extern void tmd_project_vertices(s32 count, const MATRIX *model, const kf::Projection &projection);
// PC extension: while set (by render_map_cell), tmd_project_vertices computes the
// sub-pixel positions and camera coordinates of map cells exactly, as
// view * (position + quadrant * vertex). Neighbouring cells then share
// bit-identical corners, so distant floors and walls have no pinhole cracks.
struct KfPreciseMapTransform {
    const MATRIX *view;
    const MATRIX *quadrant;
    SVECTOR position;
};
extern const KfPreciseMapTransform *render_precise_map;

#endif // KF_GAME_RENDER_H
