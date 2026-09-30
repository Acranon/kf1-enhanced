#include <kf/platform/prelude.h>
#include <kf/game/asset.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>

#include <new>
#include <stdexcept>

void asset_registry_load_tmd_archive(u16 first_asset_id, u8 *archive, std::size_t size)
{
    if (!archive || size < KF_ASSET_ARCHIVE_HEADER_BYTES)
        kf::host_fail("Truncated model archive header");
    const u16 count = archive[0] | (static_cast<u16>(archive[1]) << 8);
    if (first_asset_id > KF_ASSET_REGISTRY_KNOWN_ENTRIES ||
        count > KF_ASSET_REGISTRY_KNOWN_ENTRIES - first_asset_id)
        kf::host_fail("Model archive exceeds the asset registry");

    archive += KF_ASSET_ARCHIVE_HEADER_BYTES;
    size -= KF_ASSET_ARCHIVE_HEADER_BYTES;
    for (u16 i = 0; i < count; ++i) {
        asset_registry_set(first_asset_id + i, archive, size);
        const auto asset_size = tmd_read_word(archive);
        archive += asset_size;
        size -= asset_size;
    }
}

void asset_registry_set(u16 asset_id, void *data, std::size_t size) try
{
    KfAssetInfo info;
    if (asset_id >= KF_ASSET_REGISTRY_KNOWN_ENTRIES ||
        kf_asset_info({static_cast<const u8 *>(data), size}, info) != KF_CODEC_OK)
        kf::host_fail("Invalid model asset header");
    auto *bytes = static_cast<u8 *>(data);
    const auto tmd = tmd_resource_view(bytes + info.tmd_offset, info.encoded_bytes - info.tmd_offset);
    game_graphics_runtime.asset_registry_tmds[asset_id] = tmd;
    asset_registry_select(asset_id);
    KfAnimationData animation;
    if (info.clip_count) {
        animation.vertex_count = tmd_read_object(tmd_context(), 0).vertex_count;
        if (animation.vertex_count > KF_PROJECTED_VERTEX_CAPACITY)
            kf::host_fail("Animated model exceeds vertex capacity.");
        tmd_select_object_vertices(tmd_context(), 0);
    }
    if (kf_animation_decode({bytes, info.encoded_bytes}, animation.vertex_count, animation) != KF_CODEC_OK)
        kf::host_fail("Cannot decode model animation resource.");
    for (auto &record : game_graphics_runtime.animation_cache_records)
        if (record.state != KF_ANIMATION_CACHE_FREE && record.asset_index == asset_id)
            animation_cache_release(&record);
    game_graphics_runtime.asset_animations[asset_id] = std::move(animation);
} catch (const std::bad_alloc &) {
    kf::host_fail("Cannot allocate decoded animation data.");
} catch (const std::length_error &) {
    kf::host_fail("Decoded animation exceeds container capacity.");
}

void asset_registry_select(u16 asset_id)
{
    if (asset_id >= KF_ASSET_REGISTRY_KNOWN_ENTRIES ||
        !game_graphics_runtime.asset_registry_tmds[asset_id].data)
        kf::host_fail("Unregistered model asset");
    game_graphics_runtime.tmd_state.current_tmd = game_graphics_runtime.asset_registry_tmds[asset_id];
}
