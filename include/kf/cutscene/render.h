#ifndef KF_OPEN_RENDER_H
#define KF_OPEN_RENDER_H

#include <kf/cutscene/playback.h>
#include <kf/lib/enum.h>
#include <kf/lib/graphics.h>
#include <kf/lib/item.h>
#include <kf/lib/map_data.h>
#include <kf/lib/math.h>
#include <kf/lib/render_types.h>
#include <kf/lib/resource_file.h>
#include <kf/lib/tmd.h>

#include <array>

enum class KfOpenColorPreset : s32 {
    KF_OPEN_COLOR_DEFAULT = 0,
    KF_OPEN_COLOR_BLACK = 1,
    KF_OPEN_COLOR_WHITE = 2,
    KF_OPEN_COLOR_ENDING_MIDPOINT = 3,
    KF_OPEN_COLOR_ENDING_GREEN = 4
}; using enum KfOpenColorPreset;

enum {
    KF_OPEN_COLOR_PRESET_COUNT = 5,
    KF_OPEN_TMD_SLOT_COUNT = 2
};

typedef struct KfTmdStateOpen {
    std::array<KfTmdResource, KF_OPEN_TMD_SLOT_COUNT> slots;
    KfTmdResource current_tmd;
} KfTmdStateOpen;

typedef struct KfSpriteMaterial {
    kf::FaceMaterial surface;
    CVECTOR color;
} KfSpriteMaterial;

typedef struct KfFloorItemStateOpen {
    KfSpriteMaterial material;
    std::array<u8, 6> unknown_08;
    kf::FaceMaterial texture;
    u16 count;
    std::array<u8, 4> unknown_14;
    std::array<KfFloorItem, KF_FLOOR_ITEM_CAPACITY> items;
} KfFloorItemStateOpen;

typedef struct KfGraphicsRuntimeOpen {
    KfDisplayState display_state;
    std::array<u8, 8> unknown_20108;
    KfTmdStateOpen tmd_state;
    std::array<u8, 4> unknown_2011c;
    SVECTOR *current_tmd_vertices;
    std::array<u8, 0x14> unknown_20124;
    std::array<KfScreenVertex, KF_PROJECTED_VERTEX_CAPACITY> tmd_projected_vertices;
    std::array<u8, 0x1f68> unknown_22078;
    KfFloorItemStateOpen floor_item_state;
    KfRenderState render_state;
    std::array<MATRIX, KF_VIEW_QUADRANT_COUNT> light_quadrant_matrices;
    const KfCellWindow *active_cell_window;
    s16 tmd_projection_shift;
    std::array<u8, 2> unknown_24786;
} KfGraphicsRuntimeOpen;
extern KfGraphicsRuntimeOpen open_graphics_runtime;

extern std::array<MATRIX, KF_OPEN_COLOR_PRESET_COUNT> cutscene_color_matrix_table;
extern std::array<KfSpriteQuad, KF_FLOOR_ITEM_SPRITE_COUNT> cutscene_floor_item_sprites;
extern MATRIX floor_item_light_matrix;
extern SVECTOR cutscene_render_sprite_light_normal;
extern CVECTOR cutscene_map_textured_primitive_color;

typedef struct KfOpeningSceneCells {
    KfCellWindow windows[KF_CELL_WINDOW_YAW_COUNT];
    u8 unknown_cc0[0x30];
    KfMapGrid collision_flags;
} KfOpeningSceneCells;
typedef union KfOpeningCellStorage {
    u32 rtbl_sectors[2][512];
    KfOpeningSceneCells scene;
} KfOpeningCellStorage;
extern KfOpeningCellStorage opening_cell_storage;

extern void cutscene_display_initialize(Cutscene scene);
extern void cutscene_lighting_set_active_color_matrix(KfOpenColorPreset preset);
extern void cutscene_render_initialize(void);
extern void opening_fade_in(void);
extern void tmd_project_vertices_perspective_right(s32 count, const MATRIX *model, const kf::Projection &projection);
extern void cutscene_render_enqueue_tmd(u16 object_index, s16 depth_bias, const MATRIX *lights);
extern void render_enqueue_unlit_triangles(u16 object_index, s16 depth_bias);
extern void cutscene_render_enqueue_sprite(KfSpriteQuad *sprite, s16 depth_bias, KfSpriteDepthCueMode depth_cue_mode, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection);
extern void cutscene_render_map_cell(s32 col, s32 row, KfCellVisibility visibility);
extern void cutscene_render_enqueue_map(u16 object_index, const MATRIX *lights, const MATRIX *model, const kf::Projection &projection);

extern void cutscene_tmd_project_vertices(s32 count, const MATRIX *model, const kf::Projection &projection);

inline KfTmdContext cutscene_tmd_context()
{
    return {open_graphics_runtime.tmd_state.slots, open_graphics_runtime.tmd_state.current_tmd,
        open_graphics_runtime.current_tmd_vertices, open_graphics_runtime.tmd_projected_vertices};
}

inline KfFloorItemStorage cutscene_floor_item_storage()
{
    return {open_graphics_runtime.floor_item_state.count, open_graphics_runtime.floor_item_state.items};
}

#endif // KF_OPEN_RENDER_H
