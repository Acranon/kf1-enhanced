#include <kf/platform/prelude.h>
#include <kf/cutscene/controller.h>
#include <kf/cutscene/playback.h>
#include <kf/cutscene/resources.h>
#include <kf/lib/memory.h>

void opening_helpers_reset_module_state(void);
void camera_path_reset_module_state(void);
void opening_scenes_reset_module_state(void);
void opening_fade_reset_module_state(void);
void opening_controller_reset_module_state(void);
void cutscene_resources_reset_module_state(void);
void render_init_reset_module_state(void);
void render_tmd_reset_module_state(void);
void render_map_reset_module_state(void);
void render_sprite_reset_module_state(void);
void cutscene_render_map_cells_reset_module_state(void);
void cutscene_entity_render_reset_module_state(void);
void opening_entity_pool_reset_module_state(void);
void cutscene_audio_reset_module_state(void);

static void restore_module_initial_state()
{
    opening_helpers_reset_module_state();
    camera_path_reset_module_state();
    opening_scenes_reset_module_state();
    opening_fade_reset_module_state();
    opening_controller_reset_module_state();
    memory_destroy_arena(cutscene_memory_arena);
    cutscene_resources_reset_module_state();
    render_init_reset_module_state();
    render_tmd_reset_module_state();
    render_map_reset_module_state();
    render_sprite_reset_module_state();
    cutscene_render_map_cells_reset_module_state();
    cutscene_entity_render_reset_module_state();
    opening_entity_pool_reset_module_state();
    cutscene_audio_reset_module_state();
    format_reset_module_state();
}

void cutscene_play(Cutscene scene) {
    kf::host_set_input_context(kf::InputContext::Opening);
    restore_module_initial_state();
    opening_run(scene);
    memory_destroy_arena(cutscene_memory_arena);
}

KfMemoryArena cutscene_memory_arena;
