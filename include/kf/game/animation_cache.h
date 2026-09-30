#ifndef KF_ANIMATION_CACHE_H
#define KF_ANIMATION_CACHE_H

#include <kf/lib/animation.h>
#include <kf/lib/enum.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/tmd.h>
#include <kf/lib/types.h>

#include <vector>

enum class KfAnimationCacheState : s16 {
    KF_ANIMATION_CACHE_FREE = 0,
    KF_ANIMATION_CACHE_STALE = 1,
    KF_ANIMATION_CACHE_LIVE = 2
}; using enum KfAnimationCacheState;

enum {
    KF_ANIMATION_CACHE_CAPACITY = 12
};

typedef struct KfAnimationCacheRecord {
    KfAnimationCacheState state;
    u16 asset_index;
    KfAnimationClip clip_index;
    u16 keyframe_index;
    u16 rest_morph;
    std::vector<SVECTOR> cached_vertices;
    struct KfAnimationCacheRecord **owner_slot;
} KfAnimationCacheRecord;

extern bool render_bind_instance_vertices(
    KfAnimationCacheRecord **owner_slot, u16 asset_index, KfAnimationClip clip_index, u16 phase,
    u32 vertex_count);
extern void animation_cache_reset(void);
extern void animation_cache_mark_stale(void);
extern void animation_cache_release(KfAnimationCacheRecord *record);
extern void animation_cache_release_all(void);
extern void animation_cache_release_stale(void);
extern KfAnimationCacheRecord *animation_cache_allocate(void);

#endif // KF_ANIMATION_CACHE_H
