#include <kf/platform/prelude.h>
#include <kf/lib/resources.h>
#include <kf/platform/host.h>
#include <kf/renderer/renderer.h>

#include <cstring>

void map_grids_load(KfResourceChunk chunk, KfMapAttributeGrid &attributes,
    KfMapGrid &heights, KfMapOrientationGrid &orientations,
    KfMapGrid &flags, KfMapCollisionGrid &collision)
{
    const auto size = sizeof attributes + sizeof heights + sizeof orientations
        + sizeof flags + sizeof collision;
    if (chunk.size < size)
        kf::host_fail("Truncated map grids");
    const u8 *source = chunk.data;
    auto copy = [&](auto &grid) {
        std::memcpy(&grid, source, sizeof grid);
        source += sizeof grid;
    };
    copy(attributes);
    copy(heights);
    copy(orientations);
    copy(flags);
    copy(collision);
}

void tim_upload_images(const u8 *tim_data, std::size_t size)
{
    if (!kf::texture_store_upload_tim(&kf::host_renderer()->textures, tim_data, size))
        kf::host_fail("Invalid TIM resource or texture allocation failure.");
}
