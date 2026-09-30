#include <kf/platform/prelude.h>
#include <kf/game/asset.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/null.h>

void render_map_event(KfMapEvent *event, const MATRIX *lights)
{
    SVECTOR relative_position;
    MATRIX model;
    MATRIX composed;
    u16 asset;
    KfTmdObject *object;

    relative_position = VECTOR{
        event->reference_position.vx - game_graphics_runtime.render_state.view_position.vx,
        event->reference_position.vy - game_graphics_runtime.render_state.view_position.vy,
        event->reference_position.vz - game_graphics_runtime.render_state.view_position.vz}.narrowed();
    kf::render_place_model(composed, game_graphics_runtime.render_state.view_matrix, relative_position);
    kf::matrix_set_rotation_xyz(event->rotation, model);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.view_matrix, model, composed);
    asset = event->model_index + KF_ASSET_MAP_EVENT_FIRST;
    asset_registry_select(asset);
    object = tmd_get_object(tmd_context(), 0);
    if (!render_bind_instance_vertices(
            &event->animation_cache, asset, event->animation_clip, event->animation_phase,
            object->vertex_count)) {
        tmd_select_object_vertices(tmd_context(), 0);
        tmd_project_vertices(tmd_get_object(tmd_context(), 0)->vertex_count, &composed, game_graphics_runtime.render_state.projection);
    } else {
        tmd_project_vertices(object->vertex_count, &composed, game_graphics_runtime.render_state.projection);
    }
    render_enqueue_tmd(0, 0, lights);
}
