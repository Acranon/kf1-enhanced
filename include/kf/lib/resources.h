#ifndef KF_RESOURCES_H
#define KF_RESOURCES_H

#include <kf/lib/geometry_types.h>
#include <kf/lib/map_types.h>
#include <kf/lib/types.h>
#include <kf/platform/host.h>

#include <array>
#include <cstdio>
#include <type_traits>

enum {
    KF_RESOURCE_CHUNK_HEADER_BYTES = 4,
    KF_RESOURCE_REUSE_PREFIX_BYTES = 16
};

struct KfResourceChunk {
    const u8 *data;
    std::size_t size;
};

template <typename T>
inline const T *resource_chunk_data(const KfResourceChunk &chunk,
                                    const char *name = "resource record")
{
    static_assert(std::is_trivially_copyable_v<T>);
    std::array<char, 256> message;
    if (chunk.size < sizeof(T)) {
        std::snprintf(message.data(), message.size(), "Truncated %s: need %zu bytes, have %zu.",
                      name, sizeof(T), chunk.size);
        kf::host_fail(message.data());
    }
    if (reinterpret_cast<std::uintptr_t>(chunk.data) % alignof(T)) {
        std::snprintf(message.data(), message.size(), "Unaligned %s: requires %zu-byte alignment.",
                      name, alignof(T));
        kf::host_fail(message.data());
    }
    return reinterpret_cast<const T *>(chunk.data);
}

inline KfResourceChunk resource_chunk_view(const u8 *chunk, const u8 *end)
{
    if (chunk > end || end - chunk < KF_RESOURCE_CHUNK_HEADER_BYTES)
        kf::host_fail("Truncated resource chunk header");
    const std::size_t size = static_cast<u32>(chunk[0]) | (static_cast<u32>(chunk[1]) << 8)
        | (static_cast<u32>(chunk[2]) << 16) | (static_cast<u32>(chunk[3]) << 24);
    if (size > static_cast<std::size_t>(end - chunk - KF_RESOURCE_CHUNK_HEADER_BYTES))
        kf::host_fail("Truncated resource chunk payload");
    return {chunk + KF_RESOURCE_CHUNK_HEADER_BYTES, size};
}

// Some original fixed-size table copies include bytes from subsequent chunks.
// Validate the chunk itself, but bound those copies by the remaining file.
inline KfResourceChunk resource_stream_tail(const u8 *chunk, const u8 *end)
{
    const auto payload = resource_chunk_view(chunk, end);
    return {payload.data, static_cast<std::size_t>(end - payload.data)};
}

inline u8 *resource_stream_next(u8 *chunk, const u8 *end)
{
    const auto payload = resource_chunk_view(chunk, end);
    return chunk + KF_RESOURCE_CHUNK_HEADER_BYTES + payload.size;
}

extern void tim_upload_images(const u8 *tim_data, std::size_t size);

void map_grids_load(KfResourceChunk chunk, KfMapAttributeGrid &attributes,
    KfMapGrid &heights, KfMapOrientationGrid &orientations,
    KfMapGrid &flags, KfMapCollisionGrid &collision);

#endif // KF_RESOURCES_H
