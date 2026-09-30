#include <kf/platform/prelude.h>
#include <kf/cutscene/resources.h>
#include <kf/lib/codec.h>
#include <kf/lib/map_data.h>
#include <kf/lib/math.h>
#include <kf/lib/null.h>

#include <limits>
#include <span>

KfOpeningEntityState opening_entity_state;

void opening_entity_pool_reset(void)
{
    for (auto &entity : opening_entity_state.entities) {
        entity.object_id = KF_OPENING_ENTITY_FREE;
    }

    opening_entity_state.unknown_control_50e = 0;
    opening_entity_state.unknown_control_50c = 0;
    opening_entity_state.unknown_control_50a = 0;
}

KfOpeningEntity *opening_entity_find_by_object_id(
    std::span<KfOpeningEntity> entities, KfOpeningModelId object_id)
{
    for (auto &entity : entities) {
        if (entity.object_id == KF_OPENING_ENTITY_FREE)
            break;
        if (entity.object_id == object_id) {
            return &entity;
        }
    }
    return NULL;
}

void opening_entity_pool_load_placements(
    KfResourceChunk placements, s32 base_y)
{
    std::array<KfObjectPlacementData, KF_OPENING_ENTITY_CAPACITY> decoded {};
    constexpr u32 byte_values = std::numeric_limits<u8>::max() + 1u;
    const KfPlacementLimits limits {
        base_y == KF_OPENING_ENTITY_FLOOR_HEIGHT ? KF_MAP_COLUMNS : byte_values,
        byte_values, KF_MAP_TILE_SIZE,
    };
    std::size_t count;
    if (kf_object_placements_decode({placements.data, placements.size}, limits, decoded, count) != KF_CODEC_OK)
        kf::host_fail("Invalid opening entity placements");

    for (std::size_t i = 0; i < opening_entity_state.entities.size(); ++i) {
        auto &entity = opening_entity_state.entities[i];
        if (i >= count) {
            entity.object_id = KF_OPENING_ENTITY_FREE;
            continue;
        }
        const auto &placement = decoded[i];
        entity.object_id = kf_enum_decode<KfOpeningModelId>(placement.object_id);
        entity.cell_x = placement.tile_x;
        entity.cell_z = placement.tile_z;
        entity.rotation.z = 0;
        entity.rotation.x = 0;
        entity.rotation.y = placement.yaw & KF_ANGLE_WRAP_MASK;
        entity.position.vx = map_placement_axis_position(placement.tile_x, placement.local_x);
        entity.position.vz = map_placement_axis_position(placement.tile_z, placement.local_z);
        entity.scale = {KF_FIXED12_ONE, KF_FIXED12_ONE, KF_FIXED12_ONE};
        if (base_y == KF_OPENING_ENTITY_FLOOR_HEIGHT) {
            entity.position.vy = placement.local_y -
                cutscene_map_floor_height_grid.cells[placement.tile_z][placement.tile_x] *
                    KF_MAP_HEIGHT_STEP;
        } else {
            entity.position.vy = base_y + placement.local_y;
        }
    }
}


void opening_entity_pool_reset_module_state(void)
{
    kf::restore_initial_value<opening_entity_state>();
}
