#include <kf/platform/prelude.h>
#include <kf/game/equipment.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/player.h>
#include <kf/game/render.h>
#include <kf/game/resources.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/map.h>
#include <kf/lib/null.h>
#include <kf/lib/resources.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <new>
#include <kf/game/menu.h>
#include <kf/game/menu_text.h>
#include <kf/renderer/renderer.h>
#include <vector>

enum {
    MAP_VARIANT_ASSET_BUFFER_BYTES = 0x5a000,
    MAP_SEQUENCE_DEFAULT = 0,
    MAP_SEQUENCE_ALTERNATE = 1,
    MAP_FLOOR1_ALTERNATE_MUSIC_LEVEL = 15,
    MAP_FLOOR2_ALTERNATE_MUSIC_LEVEL = 25
};

std::array<char, KF_MAP_RESOURCE_PATH_BYTES> map_resource_path = {"B0/"};

std::array<char, 8> map_mix_tim_filename = {"MIX.TIM"};

std::array<KfCellWindow, KF_CELL_WINDOW_YAW_COUNT> render_cell_windows;

KfMapGrid map_collision_flag_grid;

KfMapOrientationGrid map_cell_orientation_grid;

KfMapGrid map_floor_height_grid;

KfMapCollisionGrid map_collision_grid;

KfMapAttributeGrid map_cell_attribute_grid;

static kf::ByteBuffer common_images_read()
{
    kf::DataFile file{};
    kf::ByteBuffer bytes{};
    if (resource_file_open(&file, "COM/MIX.TIM") != kf::FileResult::Ok)
        kf::host_fail("Cannot reload language graphics.");
    if (file.size > bytes.max_size()) {
        kf::data_file_close(&file);
        kf::host_fail("Cannot reload language graphics.");
    }
    try {
        bytes.resize(file.size);
    } catch (const std::bad_alloc &) {
        kf::data_file_close(&file);
        kf::host_fail("Cannot reload language graphics.");
    }
    const auto result = kf::data_file_read(&file, bytes.data(), bytes.size());
    kf::data_file_close(&file);
    if (result != kf::FileResult::Ok)
        kf::host_fail("Cannot reload language graphics.");
    return bytes;
}

bool game_apply_language(void)
{
    if (kf::language_requested() == kf::game_language())
        return false;
    auto previous = common_images_read();
    if (!kf::language_apply_pending())
        return false;
    auto current = common_images_read();
    if (!kf::texture_store_translate_tim(&kf::host_renderer()->textures,
            previous.data(), previous.size(), current.data(), current.size()))
        kf::host_fail("Cannot reload language graphics.");
    menu_resources_reload();
    kf::host_language_status("Language changed.");
    return true;
}

void common_resources_load(void)
{
    u8 *images;
    std::size_t image_size;
    u8 *stream;
    u8 *block;

    resource_file_load_allocated(memory_arena, &images, "COM/MIX.TIM", &image_size);
    tim_upload_images(images, image_size);
    memory_release_last(memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "COM/COM.DAT", &resource_size);
    const u8 *resource_end = stream + resource_size;
    const auto effect_asset = resource_chunk_view(stream, resource_end);
    asset_registry_set(
        KF_ASSET_HUD_MODELS, stream + KF_RESOURCE_CHUNK_HEADER_BYTES, effect_asset.size);
    block = stream = resource_stream_next(stream, resource_end);
    const auto cell_windows = resource_chunk_view(stream, resource_end);
    if (cell_windows.size < sizeof render_cell_windows)
        kf::host_fail("Truncated cell windows");
    memcpy(render_cell_windows.data(), cell_windows.data, sizeof render_cell_windows);
    stream = resource_stream_next(stream, resource_end);
    weapon_records_load_and_mirror_angles(
        resource_chunk_data<KfWeaponTable>(resource_chunk_view(stream, resource_end)));
    stream = resource_stream_next(stream, resource_end);
    // The original copy includes the next chunk's header and 416 magic bytes.
    armor_records_load(
        resource_chunk_data<KfArmorTable>(resource_stream_tail(stream, resource_end),
                                         "COM/COM.DAT armor table"));
    stream = resource_stream_next(stream, resource_end);
    magic_load_records(
        resource_chunk_data<KfMagicTable>(resource_chunk_view(stream, resource_end)));
    stream = resource_stream_next(stream, resource_end);
    // The original copy includes the next chunk's header and 148 growth bytes.
    map_object_definitions_load(resource_chunk_data<KfMapObjectDefinitionTable>(
        resource_stream_tail(stream, resource_end), "COM/COM.DAT map-object table"));
    stream = resource_stream_next(stream, resource_end);
    const auto level_growth = resource_chunk_view(stream, resource_end);
    if (level_growth.size < sizeof player_level_growth_table)
        kf::host_fail("Truncated player level growth table");
    memcpy(player_level_growth_table.data(), level_growth.data, sizeof player_level_growth_table);
    memory_release_last(memory_arena);
    memory_arena.allocation.cursor = block + KF_RESOURCE_REUSE_PREFIX_BYTES;
}

void map_resource_path_set_floor(KfFloorId floor)
{
    map_resource_path[1] = kf_enum_encode<s32>(floor) + '0';
}

u8 *map_resource_load_file(const char *filename, std::size_t *loaded_size)
{
    u8 *data;

    strcpy(&map_resource_path[3], filename);
    resource_file_load_allocated(memory_arena, &data, map_resource_path.data(), loaded_size);
    return data;
}

void map_variant_assets_load(void)
{
    u8 **asset_buffer = &map_runtime_state.variant_asset_buffer;

    memcpy((void *)(&map_resource_path[3]), (const void *)("CHR0.MIM"), sizeof "CHR0.MIM");
    map_resource_path[6] = kf_enum_encode<u8>(player_state.map_variant) + '0';
    std::size_t loaded_size;
    if (resource_file_load_into(*asset_buffer, MAP_VARIANT_ASSET_BUFFER_BYTES, map_resource_path.data(), &loaded_size) != KF_RESOURCE_LOADED)
        resource_file_fail(map_resource_path.data());
    asset_registry_load_tmd_archive(KF_ASSET_ACTOR_FIRST, *asset_buffer, loaded_size);
}

void audio_play_current_map_sequence(void)
{
    s32 sequence_index = MAP_SEQUENCE_DEFAULT;

    switch (player_state.progress_state.current_floor) {
    case KF_FLOOR_3:
    case KF_FLOOR_4:
    case KF_FLOOR_FORCE_RELOAD:
        // These selectors retain the default map sequence.
        break;
    case KF_FLOOR_1:
        if (player_state.progress_state.level >= MAP_FLOOR1_ALTERNATE_MUSIC_LEVEL) {
            sequence_index = MAP_SEQUENCE_ALTERNATE;
        }
        break;
    case KF_FLOOR_2:
        if (player_state.progress_state.level >= MAP_FLOOR2_ALTERNATE_MUSIC_LEVEL) {
            sequence_index = MAP_SEQUENCE_ALTERNATE;
        }
        break;
    case KF_FLOOR_5:
        if (player_state.map_variant == KF_FLOOR5_ALTERNATE_MUSIC_VARIANT) {
            sequence_index = MAP_SEQUENCE_ALTERNATE;
        }
        break;
    }
    audio_play_map_sequence(sequence_index);
}

void map_resources_load(KfFloorId floor, KfMapVariant map_variant)
{
    u8 *stream;
    u8 *block;

    audio_stop_sequence_fade();
    effect_pool_reset();
    memory_allocation_reset(memory_arena);
    map_resource_path_set_floor(floor);
    std::size_t image_size;
    u8 *images = map_resource_load_file(map_mix_tim_filename.data(), &image_size);
    tim_upload_images(images, image_size);
    memory_release_last(memory_arena);
    std::size_t resource_size;
    stream = map_resource_load_file("MIXA.DAT", &resource_size);
    const u8 *resource_end = stream + resource_size;
    audio_load_vab(audio_bank_resource(stream, resource_size));
    block = stream;
    stream = resource_stream_next(stream, resource_end);
    block = stream;
    stream = resource_stream_next(stream, resource_end);
    audio_play_current_map_sequence();
    map_grids_load(resource_chunk_view(stream, resource_end),
        map_cell_attribute_grid, map_floor_height_grid,
        map_cell_orientation_grid, map_collision_flag_grid,
        map_collision_grid);
    stream = resource_stream_next(stream, resource_end);
    const auto floor_items = resource_chunk_view(stream, resource_end);
    item_load_floor_placements(floor_item_storage(), map_floor_height_grid, floor_items.data, floor_items.size);
    stream = resource_stream_next(stream, resource_end);
    map_object_pool_load(resource_chunk_view(stream, resource_end));
    stream = resource_stream_next(stream, resource_end);
    actor_pool_load_placements(resource_chunk_view(stream, resource_end));
    stream = resource_stream_next(stream, resource_end);
    actor_definitions_load(resource_chunk_data<KfActorDefinitionTable>(
        resource_chunk_view(stream, resource_end), "actor definitions"));
    stream = resource_stream_next(stream, resource_end);
    map_event_pool_load(resource_chunk_view(stream, resource_end));
    memory_release_last(memory_arena);
    memory_arena.allocation.cursor = block + KF_RESOURCE_REUSE_PREFIX_BYTES;
    stream = map_resource_load_file("MIXB.DAT", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    const auto map_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(tmd_context(), KF_TMD_SLOT_MAP,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, map_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    const auto event_models = resource_chunk_view(stream, resource_end);
    asset_registry_load_tmd_archive(KF_ASSET_MAP_EVENT_FIRST,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, event_models.size);
    stream = resource_stream_next(stream, resource_end);
    const auto effect_models = resource_chunk_view(stream, resource_end);
    asset_registry_load_tmd_archive(KF_ASSET_EFFECT_FIRST,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, effect_models.size);
    stream = resource_stream_next(stream, resource_end);
    if (map_variant == KF_MAP_VARIANT_DEFAULT) {
        const auto actor_models = resource_chunk_view(stream, resource_end);
        asset_registry_load_tmd_archive(KF_ASSET_ACTOR_FIRST,
            stream + KF_RESOURCE_CHUNK_HEADER_BYTES, actor_models.size);
    } else {
        map_runtime_state.variant_asset_buffer = (u8 *)memory_allocate(memory_arena, MAP_VARIANT_ASSET_BUFFER_BYTES);
        map_variant_assets_load();
    }
    player_sync_position_to_map();
    memory_set_allocation_mode(memory_arena, KF_MEMORY_USE_HEAP);
}

void resources_reset_module_state(void)
{
    kf::restore_initial_value<map_resource_path>();
    kf::restore_initial_value<map_mix_tim_filename>();
    kf::restore_initial_value<render_cell_windows>();
    kf::restore_initial_value<map_collision_flag_grid>();
    kf::restore_initial_value<map_cell_orientation_grid>();
    kf::restore_initial_value<map_floor_height_grid>();
    kf::restore_initial_value<map_collision_grid>();
    kf::restore_initial_value<map_cell_attribute_grid>();
}
