#include "../src/lib/resources.cpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <string_view>

namespace kf {
[[noreturn]] void host_fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    std::exit(77);
}
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::string_view scenario = argv[1];
    KfMapAttributeGrid attributes {};
    KfMapGrid heights {}, flags {};
    KfMapOrientationGrid orientations {};
    KfMapCollisionGrid collision {};
    constexpr auto size = sizeof attributes + sizeof heights + sizeof orientations
        + sizeof flags + sizeof collision;
    alignas(u32) std::array<u8, size + 1> storage {};
    u8 *source = storage.data() + (scenario == "unaligned");
    for (std::size_t i = 0; i < size; ++i)
        source[i] = static_cast<u8>(i * 13 + i / KF_MAP_CELL_COUNT);
    map_grids_load({source, size - (scenario == "truncated")},
        attributes, heights, orientations, flags, collision);
    auto check = [&](const auto &grid) {
        assert(std::memcmp(&grid, source, sizeof grid) == 0);
        source += sizeof grid;
    };
    check(attributes);
    check(heights);
    check(orientations);
    check(flags);
    check(collision);
}
