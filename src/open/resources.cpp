#include <kf/lib/null.h>

#include <kf/open/audio.h>
#include <kf/lib/resource_file.h>
#include <kf/lib/item.h>
#include <kf/lib/map_data.h>
#include <kf/lib/memory.h>
#include <kf/open/render.h>
#include <kf/open/resources.h>
#include <kf/lib/geometry_types.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/lib/resources.h>

char opening_ending_sequence_path[8] = "B0/END.";

static u8 *opening_scene1_arena_cursor;

static u8 *opening_ending_arena_cursor;

KfMapOrientationGrid map_cell_orientation_grid;

KfMapGrid map_floor_height_grid;

KfMapCollisionGrid map_collision_grid;

KfMapAttributeGrid map_cell_attribute_grid;

static constexpr auto map_grid_word_count = sizeof map_cell_attribute_grid / sizeof(u32);

void opening_resources_load_scene0(void)
{
    u8 *stream;
    u8 *vab_chunk;
    const u32 *source;

    audio_stop_sequence(KF_AUDIO_STOP_FADE);
    audio_close_vab(audio_state);
    memory_allocation_reset(memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXA0.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    stream = resource_stream_next(stream, resource_end);
    const auto map_grids = resource_chunk_view(stream, resource_end);
    if (map_grids.size < 5 * sizeof map_cell_attribute_grid)
        kf::host_fail("Truncated opening map grids");
    source = resource_stream_copy_words(
        map_cell_attribute_grid.words,
        resource_chunk_data<u32>(map_grids, "opening map grids"),
        map_grid_word_count);
    source = resource_stream_copy_words(
        map_floor_height_grid.words, source, map_grid_word_count);
    source = resource_stream_copy_words(
        map_cell_orientation_grid.words, source, map_grid_word_count);
    source = resource_stream_copy_words(
        opening_cell_storage.scene.collision_flags.words, source, map_grid_word_count);
    resource_stream_copy_words(
        map_collision_grid.words, source, map_grid_word_count);
    stream = resource_stream_next(stream, resource_end);
    const auto floor_items = resource_chunk_view(stream, resource_end);
    item_load_floor_placements(floor_item_storage(), map_floor_height_grid, floor_items.data, floor_items.size);
    stream = resource_stream_next(stream, resource_end);
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_ENTITY_FLOOR_HEIGHT);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(memory_arena);
    memory_arena.allocation.cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    audio_play_sequence_file("B0/OPEN0.");
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXB0.", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    const auto map_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(tmd_context(), KF_TMD_SLOT_MAP,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, map_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    memory_set_allocation_mode(memory_arena, KF_MEMORY_USE_HEAP);
}

void opening_resources_load_scene1(void)
{
    u8 *stream;
    u8 *vab_chunk;

    audio_stop_sequence(KF_AUDIO_STOP_FADE);
    audio_close_vab(audio_state);
    memory_allocation_reset(memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXA1.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(memory_arena);
    opening_scene1_arena_cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    memory_arena.allocation.cursor = opening_scene1_arena_cursor;
    memory_set_allocation_mode(memory_arena, KF_MEMORY_USE_HEAP);
    audio_play_sequence_file("B0/OPEN1.");
}

void opening_resources_load_scene3(void)
{
    u8 *tim_stream;
    std::size_t tim_size;
    u8 *stream;

    memory_allocation_reset(memory_arena);
    memory_arena.allocation.cursor = opening_scene1_arena_cursor;
    audio_play_sequence_file("B0/OPEN3.");
    resource_file_load_allocated(memory_arena, &tim_stream, "B0/MIX3.", &tim_size);
    tim_upload_images(tim_stream, tim_size);
    memory_release_last(memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXA3.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_SCENE_BASE_Y);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(memory_arena);
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXB3.", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(memory_arena);
    memory_set_allocation_mode(memory_arena, KF_MEMORY_USE_HEAP);
}

void opening_resources_load_ending(void)
{
    u8 *tim_stream;
    std::size_t tim_size;
    u8 *stream;
    u8 *vab_chunk;
    u8 **arena_cursor = &memory_arena.allocation.cursor;

    memory_allocation_reset(memory_arena);
    resource_file_load_allocated(memory_arena, &tim_stream, "B0/MIX9.", &tim_size);
    tim_upload_images(tim_stream, tim_size);
    memory_release_last(memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXAE.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    stream = resource_stream_next(stream, resource_end);
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_SCENE_BASE_Y);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(memory_arena);
    *arena_cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    audio_play_sequence_file(opening_ending_sequence_path);
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXBE.", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    opening_ending_arena_cursor = *arena_cursor;
    memory_set_allocation_mode(memory_arena, KF_MEMORY_USE_HEAP);
}

void opening_resources_load_ending_entities(void)
{
    u8 *stream;

    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXAF.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_SCENE_BASE_Y);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(memory_arena);
}

void opening_resources_load_ending_sequence(void)
{
    u8 *stream;
    u8 *vab_chunk;
    u8 **arena_cursor = &memory_arena.allocation.cursor;

    audio_stop_sequence(KF_AUDIO_STOP_IMMEDIATE);
    audio_close_vab(audio_state);
    memory_allocation_reset(memory_arena);
    *arena_cursor = opening_ending_arena_cursor;
    std::size_t resource_size;
    resource_file_load_allocated(memory_arena, &stream, "B0/MIXAG.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    memory_release_last(memory_arena);
    *arena_cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    audio_play_sequence_file("B0/ENDG.");
}

void resources_reset_module_state(void)
{
    kf::restore_initial_value<opening_ending_sequence_path>();
    kf::restore_initial_value<opening_scene1_arena_cursor>();
    kf::restore_initial_value<opening_ending_arena_cursor>();
    kf::restore_initial_value<map_cell_orientation_grid>();
    kf::restore_initial_value<map_floor_height_grid>();
    kf::restore_initial_value<map_collision_grid>();
    kf::restore_initial_value<map_cell_attribute_grid>();
}
