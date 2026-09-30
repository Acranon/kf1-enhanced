#ifndef KF_GAME_GRAPHICS_H
#define KF_GAME_GRAPHICS_H

#include <kf/game/asset.h>
#include <kf/game/notify.h>
#include <kf/game/render.h>

#include <array>

enum {
    KF_FLOOR5_ACTOR_TEXTURE_COUNT = 3,

    KF_MORPH_SCRATCH_CAPACITY = KF_PROJECTED_VERTEX_CAPACITY
};

typedef struct KfGraphicsRuntimeGame {
    KfDisplayState display_state;
    std::array<u8, 8> unknown_20108;
    KfTmdState tmd_state;
    std::array<KfAnimationData, KF_ASSET_REGISTRY_KNOWN_ENTRIES> asset_animations;
    std::array<KfTmdResource, KF_ASSET_REGISTRY_KNOWN_ENTRIES> asset_registry_tmds;
    std::array<u8, 0x30> unknown_201f4;
    SVECTOR *current_tmd_vertices;
    std::array<KfAnimationCacheRecord, KF_ANIMATION_CACHE_CAPACITY> animation_cache_records;
    std::array<KfScreenVertex, KF_PROJECTED_VERTEX_CAPACITY> tmd_projected_vertices;
    std::array<SVECTOR, KF_MORPH_SCRATCH_CAPACITY> morph_scratch;
    std::array<kf::FaceMaterial, KF_FLOOR5_ACTOR_TEXTURE_COUNT> effect5_materials;
    kf::FaceMaterial active_render_material;
    CVECTOR active_render_color;
    kf::FaceMaterial hud_material;
    u8 hud_brightness;
    u8 unknown_241cd;
    kf::FaceMaterial notification_text_material;
    kf::FaceMaterial notification_digit_material;
    std::array<KfNotificationId, KF_NOTIFICATION_CAPACITY> notification_message_ids;
    KfNotificationState notification_state;
    kf::FaceMaterial floor_item_material;
    u16 floor_item_count;
    std::array<u8, 6> unknown_241fa;
    std::array<KfFloorItem, KF_FLOOR_ITEM_CAPACITY> floor_items;
    KfRenderState render_state;
    MATRIX map_event_light_matrix;
    MATRIX hud_model_color_matrix;
    std::array<MATRIX, KF_VIEW_QUADRANT_COUNT> light_quadrant_matrices;
    const KfCellWindow *active_cell_window;
} KfGraphicsRuntimeGame;

extern KfGraphicsRuntimeGame game_graphics_runtime;

inline KfTmdContext tmd_context()
{
    return {game_graphics_runtime.tmd_state.slots, game_graphics_runtime.tmd_state.current_tmd,
        game_graphics_runtime.current_tmd_vertices, game_graphics_runtime.tmd_projected_vertices};
}

inline KfFloorItemStorage floor_item_storage()
{
    return {game_graphics_runtime.floor_item_count, game_graphics_runtime.floor_items};
}

#endif // KF_GAME_GRAPHICS_H
