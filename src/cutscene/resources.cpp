#include <kf/platform/prelude.h>
#include <kf/cutscene/audio.h>
#include <kf/cutscene/render.h>
#include <kf/cutscene/resources.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/item.h>
#include <kf/lib/map_data.h>
#include <kf/lib/memory.h>
#include <kf/lib/null.h>
#include <kf/lib/resource_file.h>
#include <kf/lib/resources.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

std::array<char, 8> opening_ending_sequence_path = {"B0/END."};

static u8 *opening_scene1_arena_cursor;

static u8 *opening_ending_arena_cursor;

KfMapOrientationGrid cutscene_map_cell_orientation_grid;

KfMapGrid cutscene_map_floor_height_grid;

KfMapCollisionGrid cutscene_map_collision_grid;

KfMapAttributeGrid cutscene_map_cell_attribute_grid;

void opening_resources_load_scene0(void)
{
    u8 *stream;
    u8 *vab_chunk;

    audio_stop_sequence(KF_AUDIO_STOP_FADE);
    audio_close_vab(cutscene_audio_state);
    memory_allocation_reset(cutscene_memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXA0.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    cutscene_audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    stream = resource_stream_next(stream, resource_end);
    map_grids_load(resource_chunk_view(stream, resource_end),
        cutscene_map_cell_attribute_grid, cutscene_map_floor_height_grid,
        cutscene_map_cell_orientation_grid, opening_cell_storage.scene.collision_flags,
        cutscene_map_collision_grid);
    stream = resource_stream_next(stream, resource_end);
    const auto floor_items = resource_chunk_view(stream, resource_end);
    item_load_floor_placements(cutscene_floor_item_storage(), cutscene_map_floor_height_grid, floor_items.data, floor_items.size);
    stream = resource_stream_next(stream, resource_end);
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_ENTITY_FLOOR_HEIGHT);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(cutscene_memory_arena);
    cutscene_memory_arena.allocation.cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    audio_play_sequence_file("B0/OPEN0.");
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXB0.", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(cutscene_tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    const auto map_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(cutscene_tmd_context(), KF_TMD_SLOT_MAP,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, map_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    memory_set_allocation_mode(cutscene_memory_arena, KF_MEMORY_USE_HEAP);
}

void opening_resources_load_scene1(void)
{
    u8 *stream;
    u8 *vab_chunk;

    audio_stop_sequence(KF_AUDIO_STOP_FADE);
    audio_close_vab(cutscene_audio_state);
    memory_allocation_reset(cutscene_memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXA1.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    cutscene_audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(cutscene_memory_arena);
    opening_scene1_arena_cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    cutscene_memory_arena.allocation.cursor = opening_scene1_arena_cursor;
    memory_set_allocation_mode(cutscene_memory_arena, KF_MEMORY_USE_HEAP);
    audio_play_sequence_file("B0/OPEN1.");
}

void opening_resources_load_scene3(void)
{
    u8 *tim_stream;
    std::size_t tim_size;
    u8 *stream;

    memory_allocation_reset(cutscene_memory_arena);
    cutscene_memory_arena.allocation.cursor = opening_scene1_arena_cursor;
    audio_play_sequence_file("B0/OPEN3.");
    resource_file_load_allocated(cutscene_memory_arena, &tim_stream, "B0/MIX3.", &tim_size);
    tim_upload_images(tim_stream, tim_size);
    memory_release_last(cutscene_memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXA3.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_SCENE_BASE_Y);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(cutscene_memory_arena);
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXB3.", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(cutscene_tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(cutscene_memory_arena);
    memory_set_allocation_mode(cutscene_memory_arena, KF_MEMORY_USE_HEAP);
}

void opening_resources_load_ending(void)
{
    u8 *tim_stream;
    std::size_t tim_size;
    u8 *stream;
    u8 *vab_chunk;
    u8 **arena_cursor = &cutscene_memory_arena.allocation.cursor;

    memory_allocation_reset(cutscene_memory_arena);
    resource_file_load_allocated(cutscene_memory_arena, &tim_stream, "B0/MIX9.", &tim_size);
    tim_upload_images(tim_stream, tim_size);
    memory_release_last(cutscene_memory_arena);
    std::size_t resource_size;
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXAE.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    cutscene_audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    stream = resource_stream_next(stream, resource_end);
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_SCENE_BASE_Y);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(cutscene_memory_arena);
    *arena_cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    audio_play_sequence_file(opening_ending_sequence_path.data());
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXBE.", &resource_size);
    resource_end = stream + resource_size;
    const auto entity_tmd = resource_chunk_view(stream, resource_end);
    tmd_register(cutscene_tmd_context(), KF_TMD_SLOT_ENTITIES,
        stream + KF_RESOURCE_CHUNK_HEADER_BYTES, entity_tmd.size);
    stream = resource_stream_next(stream, resource_end);
    opening_ending_arena_cursor = *arena_cursor;
    memory_set_allocation_mode(cutscene_memory_arena, KF_MEMORY_USE_HEAP);
}

void opening_resources_load_ending_entities(void)
{
    u8 *stream;

    std::size_t resource_size;
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXAF.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    opening_entity_pool_load_placements(
        resource_chunk_view(stream, resource_end),
        KF_OPENING_SCENE_BASE_Y);
    stream = resource_stream_next(stream, resource_end);
    memory_release_last(cutscene_memory_arena);
}

void opening_resources_load_ending_sequence(void)
{
    u8 *stream;
    u8 *vab_chunk;
    u8 **arena_cursor = &cutscene_memory_arena.allocation.cursor;

    audio_stop_sequence(KF_AUDIO_STOP_IMMEDIATE);
    audio_close_vab(cutscene_audio_state);
    memory_allocation_reset(cutscene_memory_arena);
    *arena_cursor = opening_ending_arena_cursor;
    std::size_t resource_size;
    resource_file_load_allocated(cutscene_memory_arena, &stream, "B0/MIXAG.", &resource_size);
    const u8 *resource_end = stream + resource_size;
    cutscene_audio_load_vab(audio_bank_resource(stream, resource_size));
    stream = resource_stream_next(stream, resource_end);
    vab_chunk = stream;
    memory_release_last(cutscene_memory_arena);
    *arena_cursor = vab_chunk + KF_RESOURCE_REUSE_PREFIX_BYTES;
    audio_play_sequence_file("B0/ENDG.");
}

void cutscene_resources_reset_module_state(void)
{
    kf::restore_initial_value<opening_ending_sequence_path>();
    kf::restore_initial_value<opening_scene1_arena_cursor>();
    kf::restore_initial_value<opening_ending_arena_cursor>();
    kf::restore_initial_value<cutscene_map_cell_orientation_grid>();
    kf::restore_initial_value<cutscene_map_floor_height_grid>();
    kf::restore_initial_value<cutscene_map_collision_grid>();
    kf::restore_initial_value<cutscene_map_cell_attribute_grid>();
}
