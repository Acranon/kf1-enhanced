#include <kf/platform/prelude.h>
#include <kf/lib/graphics.h>
#include <kf/lib/item_types.h>
#include <kf/lib/map_types.h>
#include <kf/lib/math.h>
#include <kf/lib/random.h>

namespace {
constexpr std::size_t placement_bytes = 12;
constexpr std::size_t placement_terminator_bytes = 2;
constexpr unsigned placement_animation_offset = 2;
constexpr unsigned placement_cell_z_offset = 4;
constexpr unsigned placement_cell_x_offset = 5;
constexpr unsigned placement_local_z_offset = 6;
constexpr unsigned placement_local_x_offset = 8;
constexpr unsigned placement_local_y_offset = 10;
constexpr u8 placement_frame_count_mask = 0x0f;
constexpr u8 placement_facing_mask = 0xf0;
}

static constexpr bool floor_item_sprite_range_is_valid(u16 first, u8 frame_count)
{
    return first < KF_FLOOR_ITEM_SPRITE_COUNT
        && frame_count <= KF_FLOOR_ITEM_SPRITE_COUNT - first;
}

static u16 floor_item_read_u16(const u8 *data)
{
    return static_cast<u16>(data[0] | (static_cast<u16>(data[1]) << 8));
}

void item_load_floor_placements(KfFloorItemStorage storage, const KfMapGrid &heights,
    const u8 *data, std::size_t size)
{
    storage.count = 0;
    for (std::size_t offset = 0; offset <= size && size - offset >= placement_terminator_bytes; offset += placement_bytes) {
        const u8 *placement = data + offset;
        const auto base = floor_item_read_u16(placement);
        if (base == kf_enum_encode<u16>(KF_FLOOR_ITEM_END))
            return;
        if (size - offset < placement_bytes || storage.count == KF_FLOOR_ITEM_CAPACITY)
            kf::host_fail("Truncated or oversized floor-item placement list");
        const u8 frame_count = placement[placement_animation_offset] & placement_frame_count_mask;
        const u8 cell_z = placement[placement_cell_z_offset], cell_x = placement[placement_cell_x_offset];
        const bool cell_in_map = cell_z < KF_MAP_ROWS && cell_x < KF_MAP_COLUMNS;
        if (!floor_item_sprite_range_is_valid(base, frame_count) || !cell_in_map)
            kf::host_fail("Invalid floor-item sprite range or position");

        auto &item = storage.items[storage.count++];
        item.base_sprite_index = static_cast<KfFloorItemSpriteId>(base);
        item.facing = static_cast<KfFloorItemFacing>(placement[placement_animation_offset] & placement_facing_mask);
        item.frame_count = frame_count;
        const auto local_z = static_cast<s16>(floor_item_read_u16(placement + placement_local_z_offset));
        const auto local_x = static_cast<s16>(floor_item_read_u16(placement + placement_local_x_offset));
        const auto local_y = static_cast<s16>(floor_item_read_u16(placement + placement_local_y_offset));
        item.position_x = map_placement_axis_position(cell_x, local_x);
        item.position_z = map_placement_axis_position(cell_z, local_z);
        const s32 height = heights.cells[cell_z][cell_x] * KF_MAP_HEIGHT_STEP;
        item.position_y = local_y - height;
        item.animation_frame = (kf::random_next() * item.frame_count) >> KF_FLOOR_ITEM_INITIAL_FRAME_RANDOM_BITS;
    }
    kf::host_fail("Floor-item placement list has no terminator");
}

void render_floor_item(const KfRenderState &view, std::span<KfSpriteQuad> sprites,
    KfSpriteEnqueue enqueue_sprite, KfFloorItem *item, const MATRIX *lights)
{
    SVECTOR screen;
    MATRIX model;
    KfFloorItemFacing facing;
    s16 depth_bias;

    screen = VECTOR{
        item->position_x - view.view_position.vx,
        item->position_y - view.view_position.vy,
        item->position_z - view.view_position.vz}.narrowed();
    kf::render_place_model(model, view.view_matrix, screen);
    facing = item->facing;
    if (facing != KF_FLOOR_ITEM_FACING_BILLBOARD) {
        matrix_set_rotation_y(
            (kf_enum_encode<u16>(facing) - kf_enum_encode<u8>(KF_FLOOR_ITEM_FACING_ZERO_YAW)) << KF_FLOOR_ITEM_FACING_TO_ANGLE_SHIFT,
            &model);
        kf::matrix_multiply_rotation(view.view_matrix, model, model);
        depth_bias = KF_FLOOR_ITEM_FIXED_FACING_DEPTH_BIAS;
    } else {
        model.m = view.pitch_matrix.m;
        depth_bias = KF_FLOOR_ITEM_BILLBOARD_DEPTH_BIAS;
    }
    enqueue_sprite(&sprites[kf_enum_encode<u16>(item->base_sprite_index) + item->animation_frame], depth_bias, KF_SPRITE_DEPTH_CUE_BOOSTED, lights, &model, view.projection);
    floor_item_advance_frame(item);
}
