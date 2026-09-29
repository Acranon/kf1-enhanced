#include <kf/game/graphics.h>

#include <kf/lib/map_data.h>
#include <kf/game/render.h>
#include <kf/game/state.h>

enum {
    ACTOR_CULL_SQUARE_HALF_WIDTH = 12,
    ACTOR_CULL_SQUARE_WIDTH = 2 * ACTOR_CULL_SQUARE_HALF_WIDTH
};

struct RenderCellOrigin {
    u16 x;
    u16 z;
};

// PC extension (dynamic lighting): publishes a point light at a world position,
// converted to camera space with the current view rotation.
void render_publish_light(s32 x, s32 y, s32 z, float radius, float r, float g, float b, float flicker)
{
    const auto &view = game_graphics_runtime.render_state;
    const s32 dx = x - view.view_position.vx, dy = y - view.view_position.vy, dz = z - view.view_position.vz;
    constexpr s32 reach = 30000;
    if (dx < -reach || dx > reach || dy < -reach || dy > reach || dz < -reach || dz > reach)
        return;
    MATRIX rotation = view.view_matrix;
    rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
    const VECTOR camera = kf::render_transform_point(rotation, VECTOR{dx, dy, dz}.narrowed());
    kf::host_add_light(static_cast<float>(camera.vx), static_cast<float>(camera.vy), static_cast<float>(camera.vz),
        radius, r, g, b, flicker, static_cast<u32>(x * 73856093) ^ static_cast<u32>(z * 19349663));
}

static bool render_cell_is_visible(s32 cell_x, s32 cell_z, RenderCellOrigin origin)
{
    // Narrow before checking bounds, including cells before the window origin.
    const u16 row = cell_z - origin.z;
    const auto *grid = game_graphics_runtime.active_cell_window;
    if (row >= grid->height) {
        return false;
    }
    const u16 col = cell_x - origin.x;
    return col < grid->width && grid->cells[row * grid->width + col] != KF_CELL_WINDOW_HIDDEN;
}

static bool render_actor_is_visible(const KfActor &actor, RenderCellOrigin origin)
{
    if (actor.culling_mode == KF_ACTOR_CULL_VISIBILITY_GRID) {
        return render_cell_is_visible(actor.cell_x, actor.cell_z, origin);
    }

    u16 row = actor.cell_z + ACTOR_CULL_SQUARE_HALF_WIDTH;
    row -= game_graphics_runtime.render_state.view_cell.z;
    if (row >= ACTOR_CULL_SQUARE_WIDTH) {
        return false;
    }
    u16 col = actor.cell_x + ACTOR_CULL_SQUARE_HALF_WIDTH;
    col -= game_graphics_runtime.render_state.view_cell.x;
    return col < ACTOR_CULL_SQUARE_WIDTH;
}

void render_entities(void)
{
    const auto *grid = game_graphics_runtime.active_cell_window;
    const u16 window_origin_z = game_graphics_runtime.render_state.view_cell.z - grid->origin_z;
    const u16 window_origin_x = game_graphics_runtime.render_state.view_cell.x - grid->origin_x;
    const RenderCellOrigin origin = {window_origin_x, window_origin_z};

    tmd_select(tmd_context(), KF_TMD_SLOT_ENTITIES);

    for (auto &object : map_object_state.objects) {
        if (object.object_id < KF_MAP_OBJECT_RENDER_ID_END
            && render_cell_is_visible(object.cell_x, object.cell_z, origin)) {
            render_map_object(&object);
        }
    }

    for (auto &actor : actor_state.actors) {
        if (actor.lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE
            && render_actor_is_visible(actor, origin)) {
            render_actor(&actor);
        }
    }

    game_graphics_runtime.active_render_color.b = KF_FLOOR_ITEM_RENDER_BRIGHTNESS;
    game_graphics_runtime.active_render_color.g = KF_FLOOR_ITEM_RENDER_BRIGHTNESS;
    game_graphics_runtime.active_render_color.r = KF_FLOOR_ITEM_RENDER_BRIGHTNESS;
    const s16 item_count = game_graphics_runtime.floor_item_count;
    game_graphics_runtime.active_render_material = game_graphics_runtime.floor_item_material;
    auto *items = game_graphics_runtime.floor_items;
    for (s16 index = 0; index < item_count; index++) {
        auto &item = items[index];
        if (render_cell_is_visible(
                item.position_x / KF_MAP_TILE_SIZE, item.position_z / KF_MAP_TILE_SIZE, origin)) {
            render_floor_item(game_graphics_runtime.render_state, floor_item_sprites,
                render_enqueue_sprite, &item, &render_light_matrices[KF_RENDER_LIGHT_FLOOR_ITEM]);
            // Floor items are the torch and brazier flames; light from the flame's middle.
            constexpr s32 flame_centre_height = 480;
            constexpr float torch_radius = 5200.0f;
            render_publish_light(item.position_x, item.position_y - flame_centre_height, item.position_z,
                torch_radius, 1.0f, 0.62f, 0.3f, 1.0f);
        }
    }

    for (auto &sprite : effect_state.records) {
        if (sprite.type == KF_EFFECT_SLOT_FREE || sprite.render_id.model == KF_EFFECT_MODEL_NONE) {
            continue;
        }
        if (render_cell_is_visible(
                sprite.position.vx / KF_MAP_TILE_SIZE, sprite.position.vz / KF_MAP_TILE_SIZE, origin)) {
            render_actor_sprite(&sprite, &render_light_matrices[KF_RENDER_LIGHT_EFFECT]);
        }
    }

    for (auto &event : map_runtime_state.events) {
        if (event.state == KF_MAP_EVENT_ACTIVE
            && render_cell_is_visible(event.cell_x, event.cell_z, origin)) {
            render_map_event(&event, &game_graphics_runtime.light_matrix_copy);
        }
    }
}
