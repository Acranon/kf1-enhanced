#include <kf/platform/prelude.h>
#include <kf/game/animation_cache.h>
#include <kf/game/asset.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/math.h>
#include <kf/lib/memory.h>
#include <kf/lib/null.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static void morph_add_deltas(SVECTOR *vertices, const KfAnimationData &animation,
    u16 morph_index, u16 blend)
{
    const auto &morph = animation.morphs[morph_index];
    const auto add = [blend](s16 value, s16 delta) {
        const s32 scaled = (s32(delta) * s16(blend)) >> KF_FIXED12_BITS;
        // Delta scaling saturates first; adding it to the base vertex wraps.
        return static_cast<s16>(value + std::clamp<s32>(scaled, std::numeric_limits<s16>::min(), std::numeric_limits<s16>::max()));
    };
    for (std::size_t i = 0; i < morph.delta_count; ++i) {
        SVECTOR &vertex = vertices[morph.base_vertex + i];
        const auto &delta = animation.deltas[morph.first_delta + i];
        vertex.vx = add(vertex.vx, delta.x);
        vertex.vy = add(vertex.vy, delta.y);
        vertex.vz = add(vertex.vz, delta.z);
    }
}

static inline void copy_vertices(
    SVECTOR *output, const SVECTOR *input, u32 count)
{
    memcpy(output, input, std::size_t(count) * sizeof *output);
}

struct KfAnimationSample {
    const KfAnimationKeyframe &keyframe;
    u16 keyframe_index;
    u16 blend_fraction;
};

static KfAnimationSample animation_sample_keyframe(const KfAnimationData &animation,
    const KfAnimationClipData &clip, u16 phase)
{
    u16 phase_end = 0;
    for (std::size_t i = 0; i < clip.keyframe_count; ++i) {
        const auto &keyframe = animation.keyframes[clip.first_keyframe + i];
        const u16 phase_start = phase_end;
        phase_end += keyframe.duration;
        if (phase < phase_end) {
            const u16 forward = ((u32)(u16)(phase - phase_start) << KF_FIXED12_BITS)
                / keyframe.duration;
            return {keyframe, static_cast<u16>(i), static_cast<u16>(
                keyframe.reverse ? KF_FIXED12_ONE - forward : forward)};
        }
    }
    const auto last = clip.keyframe_count - 1;
    return {animation.keyframes[clip.first_keyframe + last], static_cast<u16>(last), KF_FIXED12_ONE};
}

static void animation_allocate_vertex_cache(KfAnimationCacheRecord *record, KfAnimationCacheRecord **owner_slot,
    u16 asset_index, u32 vertex_count)
{
    record->asset_index = asset_index;
    record->owner_slot = owner_slot;
    try {
        record->cached_vertices.resize(vertex_count);
    } catch (const std::bad_alloc &) {
        animation_cache_release_all();
        try {
            record->cached_vertices.resize(vertex_count);
        } catch (const std::bad_alloc &) {
            kf::host_fail("Cannot allocate animation vertices.");
        }
    }
    *owner_slot = record;
}

bool render_bind_instance_vertices(
    KfAnimationCacheRecord **owner_slot, u16 asset_index, KfAnimationClip clip_index, u16 phase,
    u32 vertex_count)
{
    if (vertex_count > KF_PROJECTED_VERTEX_CAPACITY)
        kf::host_fail("Animated model exceeds vertex capacity.");
    KfAnimationCacheRecord *record = *owner_slot;
    asset_registry_select(asset_index);
    const auto &animation = game_graphics_runtime.asset_animations[asset_index];

    if (animation.clips.empty()) {
        if (record != NULL) {
            animation_cache_release(record);
        }
        asset_registry_select(asset_index);
        tmd_select_object_vertices(tmd_context(), 0);
        return true;
    }
    if (vertex_count != animation.vertex_count)
        kf::host_fail("Animation vertex count differs from its model.");
    const auto clip_id = kf_enum_encode<u16>(clip_index);
    if (clip_id >= animation.clips.size())
        kf::host_fail("Animation clip index exceeds its resource.");

    if (record == NULL) {
        record = animation_cache_allocate();
        if (record == NULL) {
            return false;
        }
        animation_allocate_vertex_cache(record, owner_slot, asset_index, vertex_count);
    } else if (record->asset_index != asset_index) {
        animation_cache_release(record);
        record->clip_index = KF_ANIMATION_CLIP_NONE;
        animation_allocate_vertex_cache(record, owner_slot, asset_index, vertex_count);
    }

    const KfAnimationSample sample = animation_sample_keyframe(animation, animation.clips[clip_id], phase);
    const auto &keyframe = sample.keyframe;
    const u16 keyframe_index = sample.keyframe_index;
    const u16 blend_fraction = sample.blend_fraction;

    if (record->clip_index != clip_index || record->keyframe_index != keyframe_index) {
        tmd_select_object_vertices(tmd_context(), 0);
        copy_vertices(record->cached_vertices.data(), game_graphics_runtime.current_tmd_vertices, vertex_count);
        for (std::size_t i = 0; i < keyframe.morph_count; ++i)
            morph_add_deltas(record->cached_vertices.data(), animation,
                animation.indices[keyframe.first_morph + i], KF_FIXED12_ONE);
        record->rest_morph = keyframe.rest_morph;
    }

    record->clip_index = clip_index;
    record->keyframe_index = keyframe_index;

    copy_vertices(game_graphics_runtime.morph_scratch.data(), record->cached_vertices.data(), vertex_count);
    morph_add_deltas(game_graphics_runtime.morph_scratch.data(), animation, record->rest_morph, blend_fraction);
    tmd_set_current_vertices(tmd_context(), game_graphics_runtime.morph_scratch.data());
    record->state = KF_ANIMATION_CACHE_LIVE;
    return true;
}

void animation_cache_reset(void)
{
    KfAnimationCacheRecord *record = game_graphics_runtime.animation_cache_records.data();
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        record->state = KF_ANIMATION_CACHE_FREE;
        record->cached_vertices.clear();
        record++;
    } while (--records_left != 0);
}

void animation_cache_mark_stale(void)
{
    KfAnimationCacheRecord *record = game_graphics_runtime.animation_cache_records.data();
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        if (record->state != KF_ANIMATION_CACHE_FREE) {
            record->state = KF_ANIMATION_CACHE_STALE;
        }
        record++;
    } while (--records_left != 0);
}

void animation_cache_release(KfAnimationCacheRecord *record)
{
    record->state = KF_ANIMATION_CACHE_FREE;
    *record->owner_slot = NULL;
    std::vector<SVECTOR>().swap(record->cached_vertices);
}

void animation_cache_release_all(void)
{
    KfAnimationCacheRecord *record = game_graphics_runtime.animation_cache_records.data();
    s16 records_left;

    for (records_left = KF_ANIMATION_CACHE_CAPACITY - 1; records_left != -1; records_left--) {
        if (record->state != KF_ANIMATION_CACHE_FREE) {
            animation_cache_release(record);
        }
        record++;
    }
}

void animation_cache_release_stale(void)
{
    KfAnimationCacheRecord *record = game_graphics_runtime.animation_cache_records.data();
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        if (record->state == KF_ANIMATION_CACHE_STALE) {
            animation_cache_release(record);
        }
        record++;
    } while (--records_left != 0);
}

KfAnimationCacheRecord *animation_cache_allocate(void)
{
    KfAnimationCacheRecord *record = game_graphics_runtime.animation_cache_records.data();
    u16 records_left = KF_ANIMATION_CACHE_CAPACITY;

    do {
        if (record->state == KF_ANIMATION_CACHE_FREE) {
            record->clip_index = KF_ANIMATION_CLIP_NONE;
            return record;
        }
        record++;
    } while (--records_left != 0);
    return NULL;
}
