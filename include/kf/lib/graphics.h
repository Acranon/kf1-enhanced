#ifndef KF_LIB_GRAPHICS_H
#define KF_LIB_GRAPHICS_H

#include <kf/lib/item_types.h>
#include <kf/lib/map_types.h>
#include <kf/lib/math.h>
#include <kf/lib/render_types.h>
#include <kf/lib/tmd.h>

#include <array>

typedef struct KfDisplayState {
    kf::FrameStyle frame_style;
    KfDisplayBuffer buffer_index;
    std::array<u8, 3> unknown_01;
    u8 *asset_load_buffer;
    std::size_t asset_load_capacity;
} KfDisplayState;

typedef struct KfRenderState {
    kf::LightingEnvironment lighting;
    kf::Projection projection;
    MATRIX view_matrix;
    MATRIX pitch_matrix;
    MATRIX light_matrix;
    s32 fog_near_distance;
    VECTOR view_position;
    SVECTOR view_rotation;
    struct KfVecXZs view_cell;
    std::array<MATRIX, KF_VIEW_QUADRANT_COUNT> quadrant_matrices;
} KfRenderState;

struct KfFloorItemStorage {
    u16 &count;
    std::span<KfFloorItem, KF_FLOOR_ITEM_CAPACITY> items;
};

using KfSpriteEnqueue = void (*)(KfSpriteQuad *, s16, KfSpriteDepthCueMode,
    const MATRIX *, const MATRIX *, const kf::Projection &);

void display_begin_frame(KfDisplayState &display);
void display_present_frame(const KfDisplayState &display);
void render_set_view_transform(KfRenderState &view,
    const VECTOR *position_or_null, const SVECTOR *rotation_or_null);
void lighting_set_color_matrix(KfRenderState &view,
    const MATRIX *from, const MATRIX *to, s32 blend);
void fog_interpolate_near(KfRenderState &view, s32 start, s32 end, s32 ratio);
void fog_set_near(KfRenderState &view, s32 distance);
void sprite_add_f4(const KfScreenRect *rectangle, const CVECTOR *color, u16 ot_index);
void sprite_add_ft4(const KfScreenRect *rectangle, std::span<const u8, KF_QUAD_TEX_DESCRIPTOR_BYTES> texcoords,
    const kf::FaceMaterial &material, const CVECTOR *color, u16 ot_index);
void item_load_floor_placements(KfFloorItemStorage storage, const KfMapGrid &heights,
    const u8 *data, std::size_t size);
void render_floor_item(const KfRenderState &view, std::span<KfSpriteQuad> sprites,
    KfSpriteEnqueue enqueue_sprite, KfFloorItem *item, const MATRIX *lights);

#endif // KF_LIB_GRAPHICS_H
