#include <kf/platform/prelude.h>
#include <kf/game/asset.h>
#include <kf/game/graphics.h>
#include <kf/game/render.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/math.h>
#include <kf/lib/null.h>

enum {
    ACTOR_MODEL_ASSET_MASK = 0xf,
    ACTOR_MODEL_TEXTURE_SHIFT = 4,
    MAP_LIFT_DOOR_DEPTH_BIAS = 180,
    MAP_HINGED_DOOR_DEPTH_BIAS = 15,
    MENU_ITEM_DEPTH_BIAS = 1000
};

void render_actor(KfActor *actor)
{
    SVECTOR relative_position;
    MATRIX rot_y;
    MATRIX model;
    MATRIX light;
    KfTmdObject *object;
    u8 descriptor;
    u16 asset;

    relative_position = VECTOR{
        actor->position.vx - game_graphics_runtime.render_state.view_position.vx,
        actor->position.vy - game_graphics_runtime.render_state.view_position.vy,
        actor->position.vz - game_graphics_runtime.render_state.view_position.vz}.narrowed();
    kf::render_place_model(model, game_graphics_runtime.render_state.view_matrix, relative_position);
    matrix_set_rotation_x(actor->rotation.angles.x, &model);
    matrix_set_rotation_y(-actor->rotation.angles.y, &rot_y);
    kf::matrix_multiply_rotation(rot_y, model, model);
    kf::matrix_multiply_rotation(render_light_matrices[KF_RENDER_LIGHT_ACTOR], model, light);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.view_matrix, model, model);

    descriptor = actor_state.definitions.entries[actor->definition_id].model_and_texture;
    asset = descriptor & ACTOR_MODEL_ASSET_MASK;
    asset_registry_select(asset);
    object = tmd_get_object(tmd_context(), 0);
    if (!render_bind_instance_vertices(
            &actor->animation_cache, asset, actor->animation_clip,
            actor->animation_phase, object->vertex_count)) {
        tmd_select_object_vertices(tmd_context(), 0);
        tmd_project_vertices(tmd_get_object(tmd_context(), 0)->vertex_count, &model, game_graphics_runtime.render_state.projection);
    } else {
        tmd_project_vertices(object->vertex_count, &model, game_graphics_runtime.render_state.projection);
    }

    descriptor >>= ACTOR_MODEL_TEXTURE_SHIFT;
    if (descriptor-- == 0) {
        render_enqueue_tmd(0, 0, &light);
    } else {
        game_graphics_runtime.active_render_material = game_graphics_runtime.effect5_materials[descriptor];
        render_enqueue_tmd_retextured(0, 0, &light);
    }
}

void render_map_object(KfMapObject *object)
{
    SVECTOR relative_position;
    MATRIX rot_x;
    MATRIX model;
    MATRIX light;
    KfObjectId id;
    s16 depth;

    relative_position = VECTOR{
        object->position.vx - game_graphics_runtime.render_state.view_position.vx,
        object->position.vy - game_graphics_runtime.render_state.view_position.vy,
        object->position.vz - game_graphics_runtime.render_state.view_position.vz}.narrowed();
    kf::render_place_model(model, game_graphics_runtime.render_state.view_matrix, relative_position);
    matrix_set_rotation_x(object->rotation.angles.x, &rot_x);
    matrix_set_rotation_y(object->rotation.angles.y, &model);
    kf::matrix_multiply_rotation(model, rot_x, model);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.light_matrix, model, light);
    kf::matrix_multiply_rotation(game_graphics_runtime.render_state.view_matrix, model, model);

    id = object->object_id;
    switch (map_object_state.definitions.entries[kf_enum_encode<u8>(object->object_id)].behavior_type) {
    case KF_MAP_OBJECT_OP_LIFT_DOOR:
    case KF_MAP_OBJECT_OP_03:
        depth = MAP_LIFT_DOOR_DEPTH_BIAS;
        break;
    case KF_MAP_OBJECT_OP_HINGED_DOOR:
    case KF_MAP_OBJECT_OP_HINGED_DOOR_PARTNER:
        depth = MAP_HINGED_DOOR_DEPTH_BIAS;
        break;
    default:
        depth = 0;
        break;
    }
    tmd_select_object_vertices(tmd_context(), kf_enum_encode<u16>(id));
    tmd_project_vertices(tmd_get_object(tmd_context(), kf_enum_encode<u16>(id))->vertex_count, &model, game_graphics_runtime.render_state.projection);
    render_enqueue_tmd(kf_enum_encode<u16>(id), depth, &light);
}

void menu_render_item_model(const MATRIX *lights, const MATRIX *model)
{
    lighting_set_active_color_matrix(KF_GAME_COLOR_DEFAULT);
    tmd_select(tmd_context(), KF_TMD_SLOT_MENU_ITEM);
    tmd_select_object_vertices(tmd_context(), 0);
    tmd_project_vertices(tmd_get_object(tmd_context(), 0)->vertex_count, model, game_graphics_runtime.render_state.projection);
    render_enqueue_tmd(0, MENU_ITEM_DEPTH_BIAS, lights);
}
