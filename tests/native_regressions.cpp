#include <kf/platform/prelude.h>
#include <cassert>
#include <array>

// Compile the actual loader; replace only its file, graphics and audio services.
#include "../src/game/resources.cpp"
#include "../src/game/equipment.cpp"

constexpr std::size_t armor_offset = 0x1134;
constexpr std::size_t object_offset = 0x1610;
constexpr std::size_t growth_offset = 0x1a7c;
alignas(4) static std::array<u8, growth_offset + 600> common_data;
static std::size_t file_size = common_data.size();
KfMemoryArena memory_arena {};
std::array<KfPlayerLevelGrowth, KF_PLAYER_LEVEL_GROWTH_COUNT> player_level_growth_table;
static KfMapObjectDefinitionTable loaded_objects;
static KfMagicTable loaded_magic;

void asset_registry_set(u16, void *, std::size_t) {}
void magic_load_records(const KfMagicTable *source) { loaded_magic = *source; }
void map_object_definitions_load(const KfMapObjectDefinitionTable *source) { loaded_objects = *source; }

void prepare_common_data()
{
    for (std::size_t i = 0; i < common_data.size(); ++i)
        common_data[i] = static_cast<u8>((i * 37 + 11) % 251);
    std::size_t offset = 0;
    for (u32 size : {420, 3264, 704, 756, 480, 1128, 600}) {
        for (unsigned byte = 0; byte < 4; ++byte)
            common_data[offset + byte] = static_cast<u8>(size >> (8 * byte));
        offset += 4 + size;
    }
    assert(offset == common_data.size());
}

void resource_file_load_allocated(KfMemoryArena &, u8 **destination, const char *path, std::size_t *size)
{
    assert(std::strcmp(path, "COM/COM.DAT") == 0 || std::strcmp(path, "COM/MIX.TIM") == 0);
    *destination = common_data.data();
    *size = file_size;
}
void tim_upload_images(const u8 *, std::size_t) {}
void memory_release_last(KfMemoryArena &) {}

namespace kf {
[[noreturn]] void host_fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}
}

static int scalar = 7;
static std::array<std::array<int, 3>, 2> rows = {{{1, 2, 3}, {4, 5, 6}}};
struct State { int *pointer; std::array<int, 2> values; };
static std::array<State, 2> states = {{{&scalar, {8, 9}}, {&rows[1][2], {10, 11}}}};

int main(int argc, char **argv)
{
    assert(argc == 2);
    prepare_common_data();
    if (std::strcmp(argv[1], "reset") == 0) {
        kf::restore_initial_value<scalar>();
        kf::restore_initial_value<rows>();
        kf::restore_initial_value<states>();
        for (int reentry = 0; reentry < 2; ++reentry) {
            scalar = 99;
            rows[1][2] = 88;
            states[0] = {nullptr, {0, 0}};
            states[1].values[1] = 0;
            kf::restore_initial_value<scalar>();
            kf::restore_initial_value<rows>();
            kf::restore_initial_value<states>();
            assert(scalar == 7 && rows[0][0] == 1 && rows[1][2] == 6);
            assert(states[0].pointer == &scalar && states[0].values[1] == 9);
            assert(states[1].pointer == &rows[1][2] && states[1].values[1] == 11);
        }
        return 0;
    }
    if (std::strcmp(argv[1], "armor-truncated") == 0)
        file_size = armor_offset + sizeof(KfArmorTable) - 1;
    else if (std::strcmp(argv[1], "objects-truncated") == 0)
        file_size = object_offset + sizeof(KfMapObjectDefinitionTable) - 1;
    else if (std::strcmp(argv[1], "chunk-only") == 0)
        resource_chunk_data<KfArmorTable>(
            resource_chunk_view(common_data.data() + armor_offset - 4,
                                common_data.data() + file_size), "armor chunk");
    else if (std::strcmp(argv[1], "unaligned") == 0)
        resource_chunk_data<u32>({common_data.data() + 1, 4}, "test record");
    else if (std::strcmp(argv[1], "invalid-header") == 0) {
        common_data[armor_offset - 4] = 0xff;
        common_data[armor_offset - 3] = 0xff;
        resource_stream_tail(common_data.data() + armor_offset - 4,
                             common_data.data() + file_size);
    }
    common_resources_load();
    static_assert(sizeof(KfArmorTable) == 1176);
    static_assert(sizeof(KfMapObjectDefinitionTable) == 1280);
    // Check the complete original copy extents, including subsequent headers/data.
    assert(std::memcmp(&armor_records, common_data.data() + armor_offset, sizeof armor_records) == 0);
    assert(std::memcmp(&loaded_objects, common_data.data() + object_offset, sizeof loaded_objects) == 0);
    assert(std::memcmp(&loaded_magic, common_data.data() + 0x142c, sizeof loaded_magic) == 0);
    assert(std::memcmp(player_level_growth_table.data(), common_data.data() + growth_offset,
                       sizeof player_level_growth_table) == 0);
    return 0;
}
