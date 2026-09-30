#include "bytes.h"

#include <algorithm>
#include <bitset>

namespace {
using namespace kf::codec;

struct AssetHeader {
    KfAssetInfo info;
    u32 morph_table, clip_table;
};

AssetHeader asset_header(Bytes bytes)
{
    Reader input(bytes);
    AssetHeader header {{input.u32_le(), 0, 0}, 0, 0};
    header.info.clip_count = input.u32_le();
    header.info.tmd_offset = input.u32_le();
    header.morph_table = input.u32_le();
    header.clip_table = input.u32_le();
    require(header.info.encoded_bytes >= 20 && header.info.encoded_bytes <= bytes.size() &&
        header.info.tmd_offset >= 20 && header.info.tmd_offset <= header.info.encoded_bytes &&
        header.info.clip_count <= 256, "invalid asset header");
    return header;
}

Reader table(Bytes bytes, std::size_t at, std::size_t count, std::size_t width,
    std::source_location location = std::source_location::current())
{
    return Reader(slice(bytes, at, product(count, width, location), location));
}

void cell(u8 z, u8 x, KfPlacementLimits limits)
{
    require(z < limits.map_side && x < limits.map_side, "placement cell exceeds map bounds");
}

void actor_position(u8 tile, s16 local, KfPlacementLimits limits)
{
    const std::int64_t position = std::int64_t(tile) * limits.tile_size + local;
    const std::uint64_t extent = std::uint64_t(limits.map_side) * limits.tile_size;
    require(position >= 0 && std::uint64_t(position) < extent, "actor position exceeds map bounds");
}

KfActorPlacementData actor(Reader input, KfPlacementLimits limits)
{
    KfActorPlacementData result {};
    result.slot_state = input.byte();
    const auto definition = input.byte();
    result.definition_id = field<0, 5>(definition);
    result.near_square_culling = field<5, 1>(definition);
    result.heading_quadrant = input.byte();
    result.tile_z = input.byte();
    result.tile_x = input.byte();
    result.spawn_chance = input.byte();
    result.death_drop_object_id = input.byte();
    input.skip(3);
    result.local_z = input.s16_le();
    result.local_x = input.s16_le();
    input.skip(2);
    cell(result.tile_z, result.tile_x, limits);
    actor_position(result.tile_z, result.local_z, limits);
    actor_position(result.tile_x, result.local_x, limits);
    require(result.slot_state <= 3 && result.definition_id < limits.definitions &&
        result.heading_quadrant <= 3, "invalid actor placement");
    return result;
}

KfObjectPlacementData object(Reader input, KfPlacementLimits limits)
{
    KfObjectPlacementData result {};
    result.object_id = input.byte();
    input.skip(1);
    result.tile_z = input.byte();
    result.tile_x = input.byte();
    result.yaw = input.u16_le();
    result.local_z = input.s16_le();
    result.local_x = input.s16_le();
    result.local_y = input.s16_le();
    result.link = {input.u32_le(), input.u32_le()};
    cell(result.tile_z, result.tile_x, limits);
    require(result.object_id < limits.definitions, "object definition exceeds available definitions");
    return result;
}

KfEventPlacementData event(Reader input, KfPlacementLimits limits)
{
    KfEventPlacementData result {};
    result.state = input.byte();
    result.character_id = input.byte();
    result.model_index = input.byte();
    result.cell_z = input.byte();
    result.cell_x = input.byte();
    std::ranges::copy(input.take(result.dialogue_pages.size()), result.dialogue_pages.begin());
    result.dialogue_stage_limit = input.byte();
    result.unknown_0b = input.byte();
    result.unknown_0c = input.byte();
    result.behavior = input.byte();
    result.position_z_offset = input.s16_le();
    result.position_x_offset = input.s16_le();
    result.initial_rotation = input.u16_le();
    result.radius = input.u16_le();
    input.skip(2);
    cell(result.cell_z, result.cell_x, limits);
    require((result.state == 0 || result.state == 1 || result.state == 3) &&
        result.model_index < limits.definitions && result.dialogue_stage_limit <= 5 &&
        result.behavior <= 2, "invalid event placement");
    return result;
}

template<class T, class Decode>
std::size_t placements(Bytes bytes, KfPlacementLimits limits, std::span<T> output,
    std::size_t record_size, Decode record)
{
    Reader input(bytes);
    std::size_t count = 0;
    for (auto &slot : output) {
        auto peek = input;
        if (peek.byte() == 255)
            return count;
        slot = record(Reader(input.take(record_size)), limits);
        ++count;
    }
    return count;
}
} // namespace

KfCodecResult kf_asset_info(std::span<const u8> bytes, KfAssetInfo &info)
{
    return decode([&] { info = asset_header(bytes).info; });
}

KfCodecResult kf_animation_decode(std::span<const u8> bytes, u32 vertex_count, KfAnimationData &output)
{
    return decode([&] {
        const auto header = asset_header(bytes);
        bytes = bytes.first(header.info.encoded_bytes);
        KfAnimationData result;
        result.vertex_count = vertex_count;
        if (header.info.clip_count == 0) {
            output = std::move(result);
            return;
        }
        require(vertex_count != 0, "animated asset has no vertices");
        auto clips = table(bytes, header.clip_table, header.info.clip_count, 4);
        // Morph IDs are 16-bit on disc. Decode each referenced morph once.
        std::bitset<65536> used;
        while (clips.remaining()) {
            auto clip = reader_at(bytes, clips.u32_le());
            const auto count = clip.u16_le();
            clip.skip(2);
            require(count != 0, "animation clip has no keyframes");
            Reader offsets(clip.take(product(count, 4)));
            result.clips.push_back({result.keyframes.size(), count});
            while (offsets.remaining()) {
                auto frame = reader_at(bytes, offsets.u32_le());
                const auto reverse = frame.u16_le(), duration = frame.u16_le();
                const auto rest = frame.u16_le(), morph_count = frame.u16_le();
                Reader indices(frame.take(product(morph_count, 2)));
                // Shared offsets must not amplify a small input into unbounded storage/work.
                require(result.keyframes.size() < bytes.size() &&
                    morph_count <= bytes.size() - result.indices.size(), "animation keyframes exceed input budget");
                result.keyframes.push_back({reverse, duration, rest, result.indices.size(), morph_count});
                used.set(rest);
                while (indices.remaining()) {
                    const auto id = indices.u16_le();
                    used.set(id);
                    result.indices.push_back(id);
                }
            }
        }
        // Slice the table tail first, so 32-bit targets cannot wrap base + id * 4.
        auto morph_table = reader_at(bytes, header.morph_table);
        for (std::size_t id = 0; id < used.size(); ++id) {
            if (!used.test(id))
                continue;
            auto entry = morph_table;
            entry.skip(id * 4);
            auto morph = reader_at(bytes, entry.u32_le());
            morph.skip(4);
            const auto base = morph.u32_le(), count = morph.u32_le();
            require(base <= vertex_count && count <= vertex_count - base, "morph vertices exceed model bounds");
            Reader deltas(morph.take(product(count, 8)));
            require(count <= bytes.size() - result.deltas.size(), "animation deltas exceed input budget");
            result.morphs.resize(id + 1);
            result.morphs[id] = {base, result.deltas.size(), count};
            while (deltas.remaining()) {
                result.deltas.push_back({deltas.s16_le(), deltas.s16_le(), deltas.s16_le()});
                deltas.skip(2);
            }
        }
        output = std::move(result);
    });
}

KfCodecResult kf_actor_placements_decode(std::span<const u8> bytes, KfPlacementLimits limits,
    std::span<KfActorPlacementData> output, std::size_t &count)
{
    return decode([&] { count = placements(bytes, limits, output, 16, actor); });
}

KfCodecResult kf_object_placements_decode(std::span<const u8> bytes, KfPlacementLimits limits,
    std::span<KfObjectPlacementData> output, std::size_t &count)
{
    return decode([&] { count = placements(bytes, limits, output, 20, object); });
}

KfCodecResult kf_event_placements_decode(std::span<const u8> bytes, KfPlacementLimits limits,
    std::span<KfEventPlacementData> output, std::size_t &count)
{
    return decode([&] { count = placements(bytes, limits, output, 24, event); });
}
