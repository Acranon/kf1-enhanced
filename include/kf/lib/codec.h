#ifndef KF_CODEC_H
#define KF_CODEC_H

#include <kf/lib/types.h>

#include <array>
#include <span>
#include <vector>

enum class KfCodecResult : s32 {
    KF_CODEC_OK = 0,
    KF_CODEC_END = 1,
    KF_CODEC_INVALID = 2,
    KF_CODEC_OUTPUT_FULL = 3
};
using enum KfCodecResult;

struct KfTimInfo {
    u32 mode, width, height, encoded_bytes;
    s32 image_x, image_y, palette_x, palette_y;
};
// Input spans and output storage must be disjoint; offsets are byte offsets.
KfCodecResult kf_tim_info(std::span<const u8> bytes, std::size_t offset, KfTimInfo &info);
KfCodecResult kf_tim_rgba(std::span<const u8> bytes, std::size_t offset, u32 palette_row,
    std::span<u8> rgba);
// Compose authored TIM rectangles into a temporary 1024x512 word image for
// material conversion. STP is retained; this is not runtime emulated VRAM.
KfCodecResult kf_tim_compose(std::span<const u8> bytes, std::span<u16> words);

struct KfAssetInfo {
    u32 encoded_bytes, tmd_offset, clip_count;
};
struct KfAnimationClipData {
    std::size_t first_keyframe, keyframe_count;
};
struct KfAnimationKeyframe {
    u16 reverse, duration, rest_morph;
    std::size_t first_morph, morph_count;
};
struct KfAnimationMorph {
    u32 base_vertex;
    std::size_t first_delta, delta_count;
};
struct KfAnimationDelta {
    s16 x, y, z;
};
struct KfAnimationData {
    std::vector<KfAnimationClipData> clips;
    std::vector<KfAnimationKeyframe> keyframes;
    std::vector<KfAnimationMorph> morphs;
    std::vector<u16> indices;
    std::vector<KfAnimationDelta> deltas;
    u32 vertex_count = 0;
};
KfCodecResult kf_asset_info(std::span<const u8> bytes, KfAssetInfo &info);
// Decoded data owns its storage; failures leave the destination unchanged.
KfCodecResult kf_animation_decode(std::span<const u8> bytes, u32 vertex_count,
    KfAnimationData &output);

struct KfPlacementLimits {
    u32 map_side, definitions, tile_size;
};
struct KfActorPlacementData {
    u8 slot_state, definition_id, near_square_culling, heading_quadrant;
    u8 tile_z, tile_x, spawn_chance, death_drop_object_id;
    s16 local_z, local_x;
};
struct KfObjectPlacementData {
    u8 object_id, tile_z, tile_x;
    u16 yaw;
    s16 local_z, local_x, local_y;
    std::array<u32, 2> link;
};
struct KfEventPlacementData {
    u8 state, character_id, model_index, cell_z, cell_x;
    std::array<u8, 5> dialogue_pages;
    u8 dialogue_stage_limit, unknown_0b, unknown_0c, behavior;
    s16 position_z_offset, position_x_offset;
    u16 initial_rotation, radius;
};
// Decoded lists have explicit counts; the on-disc sentinel is not returned.
KfCodecResult kf_actor_placements_decode(std::span<const u8> bytes,
    KfPlacementLimits limits, std::span<KfActorPlacementData> output, std::size_t &count);
KfCodecResult kf_object_placements_decode(std::span<const u8> bytes,
    KfPlacementLimits limits, std::span<KfObjectPlacementData> output, std::size_t &count);
KfCodecResult kf_event_placements_decode(std::span<const u8> bytes,
    KfPlacementLimits limits, std::span<KfEventPlacementData> output, std::size_t &count);

#endif // KF_CODEC_H
