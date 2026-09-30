#include <kf/lib/codec.h>
#include <kf/audio/codec.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <vector>

static void word(std::vector<uint8_t> &bytes, size_t at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = value >> (8 * i);
}

static void animation()
{
    std::vector<uint8_t> bytes(80);
    word(bytes, 0, bytes.size()); word(bytes, 4, 1); word(bytes, 8, 68);
    word(bytes, 12, 24); word(bytes, 16, 20);
    word(bytes, 20, 28); word(bytes, 24, 48);
    word(bytes, 28, 1); word(bytes, 32, 36);
    word(bytes, 36, 4096u << 16); word(bytes, 40, 1u << 16);
    word(bytes, 56, 1); word(bytes, 60, 0x1234ffff); word(bytes, 64, 0x8000);
    KfAnimationData output;
    assert(kf_animation_decode(bytes, 1, output) == KF_CODEC_OK);
    assert(output.clips.size() == 1 && output.keyframes.size() == 1 && output.morphs.size() == 1);
    assert(output.indices.size() == 1 && output.deltas.size() == 1);
    const auto &clip = output.clips[0];
    const auto &frame = output.keyframes[0];
    const auto &morph = output.morphs[0];
    const auto &delta = output.deltas[0];
    assert(clip.first_keyframe == 0 && clip.keyframe_count == 1);
    assert(frame.duration == 4096 && frame.rest_morph == 0 && frame.morph_count == 1);
    assert(morph.base_vertex == 0 && morph.delta_count == 1 && output.indices[0] == 0);
    assert(delta.x == -1 && delta.y == 0x1234 && delta.z == -32768);
    for (size_t n = 0; n < bytes.size(); ++n)
        assert(kf_animation_decode({bytes.data(), n}, 1, output) == KF_CODEC_INVALID);
    for (size_t n = 20; n < 68; ++n) {
        auto truncated = bytes;
        word(truncated, 0, n); word(truncated, 8, 20);
        assert(kf_animation_decode({truncated.data(), n}, 1, output) == KF_CODEC_INVALID);
    }
    for (size_t at : {12, 16, 20, 24, 32, 52, 56}) {
        auto bad = bytes; word(bad, at, 0xffffffff);
        assert(kf_animation_decode(bad, 1, output) == KF_CODEC_INVALID);
    }
    auto empty = bytes; word(empty, 28, 0);
    assert(kf_animation_decode(empty, 1, output) == KF_CODEC_INVALID);
    // Failed decoding leaves the last successful owned result intact.
    assert(output.clips.size() == 1 && output.deltas[0].y == 0x1234);
    // Explicit little-endian decoding must also accept an unaligned source.
    bytes.insert(bytes.begin(), 0);
    assert(kf_animation_decode({bytes.data() + 1, bytes.size() - 1}, 1, output) == KF_CODEC_OK);
}

static void placements()
{
    KfPlacementLimits limits {100, 12, 2000};
    std::array<KfActorPlacementData, 2> actors {};
    std::array<KfObjectPlacementData, 2> objects {};
    std::array<KfEventPlacementData, 2> events {};
    size_t count = 99;
    std::vector<uint8_t> bytes(25);
    // A single-byte sentinel needs no complete record behind it.
    bytes[0] = 255;
    assert(kf_actor_placements_decode({bytes.data(), 1}, limits, {actors.data(), 2}, count) == KF_CODEC_OK && count == 0);
    assert(kf_object_placements_decode({bytes.data(), 1}, limits, {objects.data(), 2}, count) == KF_CODEC_OK && count == 0);
    assert(kf_event_placements_decode({bytes.data(), 1}, limits, {events.data(), 2}, count) == KF_CODEC_OK && count == 0);
    bytes[0] = 1; bytes[1] = 0x23; bytes[2] = 2; bytes[3] = 99;
    bytes[10] = 0xff; bytes[11] = 0xff; bytes[16] = 255;
    assert(kf_actor_placements_decode({bytes.data(), 17}, limits, {actors.data(), 2}, count) == KF_CODEC_OK && count == 1);
    assert(actors[0].definition_id == 3 && actors[0].near_square_culling == 1 && actors[0].local_z == -1);
    for (size_t n = 0; n < 17; ++n)
        assert(kf_actor_placements_decode({bytes.data(), n}, limits, {actors.data(), 2}, count) == KF_CODEC_INVALID);
    for (size_t at : {1, 2, 3, 4}) {
        auto bad = bytes; bad[at] = 127;
        assert(kf_actor_placements_decode({bad.data(), 17}, limits, {actors.data(), 2}, count) == KF_CODEC_INVALID);
    }
    auto outside = bytes; outside[3] = 0;
    assert(kf_actor_placements_decode({outside.data(), 17}, limits, {actors.data(), 2}, count) == KF_CODEC_INVALID);
    std::fill(bytes.begin(), bytes.end(), 0); bytes[20] = 255;
    assert(kf_object_placements_decode({bytes.data(), 21}, limits, {objects.data(), 2}, count) == KF_CODEC_OK && count == 1);
    for (size_t n = 0; n < 21; ++n)
        assert(kf_object_placements_decode({bytes.data(), n}, limits, {objects.data(), 2}, count) == KF_CODEC_INVALID);
    bytes[0] = 12;
    assert(kf_object_placements_decode({bytes.data(), 21}, limits, {objects.data(), 2}, count) == KF_CODEC_INVALID);
    std::fill(bytes.begin(), bytes.end(), 0); bytes[24] = 255;
    assert(kf_event_placements_decode({bytes.data(), 25}, limits, {events.data(), 2}, count) == KF_CODEC_OK && count == 1);
    for (size_t n = 0; n < 25; ++n)
        assert(kf_event_placements_decode({bytes.data(), n}, limits, {events.data(), 2}, count) == KF_CODEC_INVALID);
    for (size_t at : {2, 3, 4, 10, 13}) {
        auto bad = bytes; bad[at] = 100;
        assert(kf_event_placements_decode({bad.data(), 25}, limits, {events.data(), 2}, count) == KF_CODEC_INVALID);
    }
    // Filling the pool exactly does not require a sentinel beyond its capacity.
    assert(kf_event_placements_decode({bytes.data(), 24}, limits, {events.data(), 1}, count) == KF_CODEC_OK && count == 1);
}

int main()
{
    animation();
    placements();
}
