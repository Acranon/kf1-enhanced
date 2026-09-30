#include <kf/platform/prelude.h>
#include <kf/game/actor.h>
#include <kf/game/game.h>
#include <kf/lib/codec.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>

#include <array>

KfActorState actor_state;

void actor_pool_update(void)
{
    for (auto &actor : actor_state.actors) {
        actor_bind_current(&actor);
        if (actor.slot_state != KF_ACTOR_SLOT_FREE) {
            actor_update_awareness();
            if (actor.lifecycle == KF_ACTOR_LIFECYCLE_ACTIVE) {
                actor_update_current_action();
            }
        }
    }
    actor_bind_current(NULL);
}

void actor_pool_load_placements(KfResourceChunk chunk)
{
    std::array<KfActorPlacementData, KF_ACTOR_CAPACITY> decoded {};
    std::size_t count;
    if (kf_actor_placements_decode({chunk.data, chunk.size},
            {KF_MAP_COLUMNS, KF_ACTOR_DEFINITION_COUNT, KF_MAP_TILE_SIZE}, decoded, count) != KF_CODEC_OK)
        kf::host_fail("Invalid actor placements.");
    for (std::size_t i = 0; i < std::size(actor_state.actors); ++i) {
        auto &actor = actor_state.actors[i];
        if (i >= count) {
            actor.slot_state = KF_ACTOR_SLOT_FREE;
            actor.lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
            continue;
        }
        const auto *placements = &decoded[i];
        actor.slot_state = kf_enum_decode<KfActorSlotState>(placements->slot_state);
        actor.definition_id = placements->definition_id;
        if (placements->near_square_culling) {
            actor.culling_mode = KF_ACTOR_CULL_NEAR_SQUARE;
        } else {
            actor.culling_mode = KF_ACTOR_CULL_VISIBILITY_GRID;
        }
        actor.heading_quadrant = kf_enum_decode<KfActorHeadingQuadrant>(placements->heading_quadrant);
        actor.tile_z = placements->tile_z;
        actor.tile_x = placements->tile_x;
        actor.spawn_chance = placements->spawn_chance;
        actor.death_drop_object_id = kf_enum_decode<KfObjectId>(placements->death_drop_object_id);
        actor.local_z = placements->local_z;
        actor.local_x = placements->local_x;
        actor.lifecycle = KF_ACTOR_LIFECYCLE_DORMANT;
        actor.position.vz = map_placement_axis_position(actor.tile_z, actor.local_z);
        actor.position.vx = map_placement_axis_position(actor.tile_x, actor.local_x);
        actor.position.vy = map_floor_height_at_position(&actor.position);
        actor.cell_x = actor.tile_x;
        actor.cell_z = actor.tile_z;
    }
}

void actor_definitions_load(const KfActorDefinitionTable *definitions)
{
    actor_state.definitions = *definitions;
}


void actor_pool_reset_module_state(void)
{
    kf::restore_initial_value<actor_state>();
}
