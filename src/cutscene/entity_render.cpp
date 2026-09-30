#include <kf/platform/prelude.h>
#include <kf/cutscene/opening_render.h>
#include <kf/cutscene/render.h>
#include <kf/cutscene/resources.h>
#include <kf/cutscene/scene0.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/map_data.h>
#include <kf/lib/math.h>

#include <array>

enum {
    OPENING_MODEL_DEPTH_BIAS = -100,
    OPENING_MODEL_YAW_STEP = 64,
    ENDING_TRANSLATING_MODEL_DEPTH_BIAS = 1000,
    ENDING_ROTATING_MODEL_DEPTH_BIAS = 10000
};

std::array<KfSpriteQuad, KF_FLOOR_ITEM_SPRITE_COUNT> cutscene_floor_item_sprites = {
    KfSpriteQuad{0x90, 0x00, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0xb0, 0x00, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0xd0, 0x00, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0xb0, 0x00, 0x20, 0x20, 0xfe00, 0xfc40, 0x400, 0x400},
    KfSpriteQuad{0x90, 0x20, 0x20, 0x27, 0xfe00, 0xfb40, 0x400, 0x500},
    KfSpriteQuad{0xb0, 0x20, 0x20, 0x27, 0xfe00, 0xfb40, 0x400, 0x500},
    KfSpriteQuad{0xd0, 0x20, 0x20, 0x27, 0xfe00, 0xfb40, 0x400, 0x500},
};

void opening_entity_render(KfOpeningEntity *entity)
{
    VECTOR scale;
    SVECTOR relative_position;
    MATRIX model;
    MATRIX light;
    KfOpeningModelId object_id;
    s16 depth;

    relative_position = VECTOR{
        entity->position.vx - open_graphics_runtime.render_state.view_position.vx,
        entity->position.vy - open_graphics_runtime.render_state.view_position.vy,
        entity->position.vz - open_graphics_runtime.render_state.view_position.vz}.narrowed();

    kf::render_place_model(model, open_graphics_runtime.render_state.view_matrix, relative_position);
    matrix_set_rotation_yxz(&entity->rotation, &model);
    scale = entity->scale.widened();
    kf::matrix_scale_axes(model, scale);
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.light_matrix, model, light);
    kf::matrix_multiply_rotation(open_graphics_runtime.render_state.view_matrix, model, model);

    object_id = entity->object_id;
    depth = 0;
    switch (entity->object_id) {
    case KF_OPENING_SCENE0_DECREASING_YAW_MODEL:
    case KF_OPENING_SCENE0_INCREASING_YAW_MODEL:
    case KF_OPENING_SCENE3_INCREASING_YAW_MODEL:
    case KF_OPENING_SCENE3_DECREASING_YAW_MODEL:
    case KF_OPENING_TAPERED_COLUMN:
        depth = OPENING_MODEL_DEPTH_BIAS;
        break;
    case KF_OPENING_GREEN_CRYSTAL_INCREASING_YAW:
    case KF_OPENING_PINK_CRYSTAL_INCREASING_YAW:
        entity->rotation.y = (entity->rotation.y + OPENING_MODEL_YAW_STEP) & KF_ANGLE_WRAP_MASK;
        break;
    case KF_OPENING_GREEN_CRYSTAL_DECREASING_YAW:
    case KF_OPENING_PINK_CRYSTAL_DECREASING_YAW:
        entity->rotation.y = (entity->rotation.y - OPENING_MODEL_YAW_STEP) & KF_ANGLE_WRAP_MASK;
        break;
    case KF_OPENING_CASTLE_MOUNTAIN_BACKDROP:
        tmd_select_object_vertices(cutscene_tmd_context(), kf_enum_encode<u16>(object_id));
        tmd_project_vertices_perspective_right(tmd_get_object(cutscene_tmd_context(), kf_enum_encode<u16>(object_id))->vertex_count, &model, open_graphics_runtime.render_state.projection);
        cutscene_render_enqueue_tmd(kf_enum_encode<u16>(object_id), 0, &light);
        return;
    case KF_OPENING_ENDING_ORANGE_DISK:
    case KF_OPENING_ENDING_STARFIELD:
        depth = object_id == KF_OPENING_ENDING_ORANGE_DISK
            ? ENDING_TRANSLATING_MODEL_DEPTH_BIAS : ENDING_ROTATING_MODEL_DEPTH_BIAS;
        tmd_select_object_vertices(cutscene_tmd_context(), kf_enum_encode<u16>(object_id));
        cutscene_tmd_project_vertices(tmd_get_object(cutscene_tmd_context(), kf_enum_encode<u16>(object_id))->vertex_count, &model, open_graphics_runtime.render_state.projection);
        render_enqueue_unlit_triangles(kf_enum_encode<u16>(object_id), depth);
        return;
    default:
        depth = 0;
        break;
    }

    tmd_select_object_vertices(cutscene_tmd_context(), kf_enum_encode<u16>(object_id));
    cutscene_tmd_project_vertices(tmd_get_object(cutscene_tmd_context(), kf_enum_encode<u16>(object_id))->vertex_count, &model, open_graphics_runtime.render_state.projection);
    cutscene_render_enqueue_tmd(kf_enum_encode<u16>(object_id), depth, &light);
}

void opening_render_entities_and_items(void)
{
    const KfCellWindow *window = open_graphics_runtime.active_cell_window;
    u16 origin_z = open_graphics_runtime.render_state.view_cell.z - window->origin_z;
    u16 origin_x = open_graphics_runtime.render_state.view_cell.x - window->origin_x;
    KfOpeningEntity *entity;
    KfFloorItem *item;
    u16 row;
    u16 col;
    s16 remaining;

    tmd_select(cutscene_tmd_context(), KF_TMD_SLOT_ENTITIES);
    entity = opening_entity_state.entities.data();
    for (remaining = KF_OPENING_ENTITY_CAPACITY - 1; remaining != -1; remaining--) {
        if (entity->object_id < KF_OPENING_ENTITY_MODEL_LIMIT) {
            const KfCellWindow *grid = open_graphics_runtime.active_cell_window;

            row = entity->cell_z - origin_z;
            if (row < grid->height) {
                col = entity->cell_x - origin_x;

                if (col < grid->width && grid->cells[row * grid->width + col] != KF_CELL_WINDOW_HIDDEN) {
                    opening_entity_render(entity);
                }
            }
        }
        entity++;
    }

    open_graphics_runtime.floor_item_state.material.color.r = open_graphics_runtime.floor_item_state.material.color.g =
        open_graphics_runtime.floor_item_state.material.color.b = KF_FLOOR_ITEM_RENDER_BRIGHTNESS;
    open_graphics_runtime.floor_item_state.material.surface = open_graphics_runtime.floor_item_state.texture;
    item = open_graphics_runtime.floor_item_state.items.data();
    remaining = open_graphics_runtime.floor_item_state.count;
    while (--remaining != -1) {
        const KfCellWindow *grid = open_graphics_runtime.active_cell_window;

        row = item->position_z / KF_MAP_TILE_SIZE - origin_z;
        if (row < grid->height) {
            col = item->position_x / KF_MAP_TILE_SIZE - origin_x;

            if (col < grid->width
                    && grid->cells[row * grid->width + col]
                        != KF_CELL_WINDOW_HIDDEN) {
                render_floor_item(open_graphics_runtime.render_state, cutscene_floor_item_sprites,
                cutscene_render_enqueue_sprite, item, &floor_item_light_matrix);
            }
        }
        item++;
    }
}

void cutscene_entity_render_reset_module_state(void)
{
    kf::restore_initial_value<cutscene_floor_item_sprites>();
}
