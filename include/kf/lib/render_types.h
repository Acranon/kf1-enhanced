#ifndef KF_RENDER_TYPES_H
#define KF_RENDER_TYPES_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>
#include <kf/renderer/lighting.h>
#include <kf/renderer/projection.h>
#include <kf/renderer/renderer.h>

enum { KF_PROJECTED_VERTEX_CAPACITY = 1000 };

enum {
    KF_CYLINDER_TRANSITION_COUNT = 4,
    KF_CYLINDER_TRANSITION_TALL_SCALE = 0x2000,
    KF_CYLINDER_TRANSITION_SCALE_STEP = 0x100,
    KF_CYLINDER_TRANSITION_YAW_STEP = 0x200,
    KF_CYLINDER_TRANSITION_FRAMES = 48,
    KF_CYLINDER_TRANSITION_STAGGER_SHIFT = 3,
    KF_CYLINDER_TRANSITION_STAGGER_FRAMES = 1 << KF_CYLINDER_TRANSITION_STAGGER_SHIFT
};

enum class KfSpriteDepthCueMode : s32 {
    KF_SPRITE_DEPTH_CUE_NORMAL = 0,
    KF_SPRITE_DEPTH_CUE_BOOSTED = 1
}; using enum KfSpriteDepthCueMode;

enum class KfDisplayBuffer : u8 {
    KF_DISPLAY_BUFFER_FIRST = 0,
    KF_DISPLAY_BUFFER_SECOND = 1,
    KF_DISPLAY_BUFFER_UNINITIALIZED = 0xff
}; using enum KfDisplayBuffer;

constexpr KfDisplayBuffer display_next_buffer(KfDisplayBuffer current)
{
    return static_cast<KfDisplayBuffer>(current == KF_DISPLAY_BUFFER_FIRST);
}

enum {
    KF_VRAM_WIDTH = 1024,
    KF_VRAM_HEIGHT = 512,
    KF_DISPLAY_WIDTH = 320,
    KF_DISPLAY_HEIGHT = 240,
    KF_DISPLAY_BUFFER_COUNT = 2,
    KF_ORDERING_TABLE_LENGTH = 0x4000,
    KF_ORDERING_TABLE_INDEX_MASK = KF_ORDERING_TABLE_LENGTH - 1,
    KF_SCENE_MIN_OT_DEPTH = 5,
    KF_MAP_OT_DEPTH_BIAS = 200,
    KF_GTE_DEPTH_TO_OT_SHIFT = 2,
    KF_TEXTURE_BASE_BRIGHTNESS = 128,
    KF_VIEW_QUADRANT_COUNT = 4,
    KF_CELL_WINDOW_YAW_COUNT = 16,
    KF_CELL_WINDOW_CELL_CAPACITY = 196,
    KF_CELL_WINDOW_YAW_SHIFT = 8,
    KF_DEFAULT_PROJECTION_DISTANCE = 200,
    KF_INITIAL_FOG_NEAR_DISTANCE = 11000,
    KF_RGB555_CHANNEL_MASK = 0x1f,
    KF_RGB555_GREEN_SHIFT = 5,
    KF_RGB555_BLUE_SHIFT = 10,
    KF_RGB555_STP = 0x8000
};

enum {
    KF_TEXTURE_LOWER_PAGE_Y = 256
};

enum {
    KF_TRANSITION_RECT_X = 32,
    KF_TRANSITION_RECT_WIDTH = 255,
    KF_TRANSITION_RECT_HEIGHT = 240,
    KF_TRANSITION_TPAGE_X = 576,
    KF_TRANSITION_CLUT_Y = 495,
    KF_TRANSITION_FADE_FRAMES = 18,
    KF_TRANSITION_FADE_STEP = 6,
    KF_TRANSITION_FADE_LIMIT = 255,
    KF_TRANSITION_OT_DEPTH = 4
};

typedef struct KfScreenRect {
    u16 x;
    s16 y;
    u16 w;
    u16 h;
} KfScreenRect;

enum {
    KF_QUAD_TEX_U = 0,
    KF_QUAD_TEX_V = 2,
    KF_QUAD_TEX_U_SPAN = 4,
    KF_QUAD_TEX_V_SPAN = 6,
    KF_QUAD_TEX_DESCRIPTOR_BYTES = 8
};

typedef struct KfSpriteQuad {
    u8 u;
    u8 v;
    u8 u_span;
    u8 v_span;
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} KfSpriteQuad;

enum class KfCellVisibility : u8 {
    KF_CELL_WINDOW_HIDDEN = 0,
    KF_CELL_WINDOW_DISTANT = 1,
    KF_CELL_WINDOW_NEAR = 2
}; using enum KfCellVisibility;

typedef struct KfCellWindow {
    u16 width;
    u16 height;
    u16 origin_x;
    u16 origin_z;
    KfCellVisibility cells[KF_CELL_WINDOW_CELL_CAPACITY];
} KfCellWindow;

inline void transition_advance_color(CVECTOR &color)
{
    if (color.r < KF_TRANSITION_FADE_LIMIT) {
        color.r += KF_TRANSITION_FADE_STEP;
    } else {
        color.r = KF_TRANSITION_FADE_LIMIT;
    }
    color.b = color.r;
    color.g = color.r;
}

#endif // KF_RENDER_TYPES_H
