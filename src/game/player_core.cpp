#include <kf/platform/prelude.h>
#include <kf/game/audio.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/player.h>
#include <kf/game/resources.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr unsigned weapon_image_number_offset = 9;
static constexpr unsigned weapon_image_path_capacity = 16;

static constexpr s32 PLAYER_INITIAL_POSITION_X = 31000;
static constexpr s32 PLAYER_INITIAL_POSITION_Z = 4000;
static constexpr s32 PLAYER_ATTRIBUTE_52_FATAL_HEIGHT = -6999;

enum {
    PLAYER_FATAL_DROP_DISTANCE = 3000,
    PLAYER_FALL_ACCELERATION = 40,
    PLAYER_FALL_LANDING_OVERSHOOT = 100,
    PLAYER_STEP_UP_ACCELERATION = 5,
    PLAYER_FAST_STEP_MIN_SPEED = 181,
    PLAYER_FAST_STEP_UP_VELOCITY = -300,
    PLAYER_SLOW_STEP_UP_VELOCITY = -100,
    PLAYER_BOB_PHASE_PER_SPEED = 2,
    PLAYER_BOB_SINE_DOWNSHIFT = 6
};

static constexpr int PLAYER_WEAPON_CHARGE_DELAY_UPDATES = 10;
static constexpr int PLAYER_COLICHEMARDE_HIT_PHASE = 1000;
static constexpr int PLAYER_WEAPON_HIT_PHASE = 3072;
static constexpr int PLAYER_WEAPON_HIT_WINDOW = 300;
static constexpr int PLAYER_WEAPON_HIT_Y_OFFSET = 1000;
static constexpr int PLAYER_WEAPON_HIT_RADIUS = 800;
static constexpr int PLAYER_WEAPON_HIT_HEIGHT = 1000;
static constexpr int PLAYER_COLLISION_SLIDE_CLEARANCE = 100;
static constexpr int PLAYER_COLLISION_DEFLECTION_ANGLE = 64;
static constexpr int PLAYER_MAX_STEP_RISE = 699;
static constexpr int PLAYER_DIAGONAL_COMPONENT_Q12 = 2896;

std::array<char, weapon_image_path_capacity> weapon_asset_path_template = {"WEPON/WEP00.MIM"};

std::array<KfFloorEntryCell, KF_PLAYER_FLOOR_ENTRY_COUNT> floor_entry_cells = {
    KfFloorEntryCell{15, 2}, KfFloorEntryCell{29, 56}, KfFloorEntryCell{28, 18}, KfFloorEntryCell{7, 22}, KfFloorEntryCell{39, 69}
};

SVECTOR player_rotation_snapshot;

VECTOR player_position_snapshot;

std::array<KfPlayerLevelGrowth, KF_PLAYER_LEVEL_GROWTH_COUNT> player_level_growth_table;

KfPlayerState player_state;

void player_set_equipment_slot(KfObjectId item_id, KfEquipmentSlot slot)
{
    switch (slot) {
    case KF_EQUIPMENT_SLOT_REFRESH_ONLY:
        // Rebuild equipment references and stats without changing a slot.
        break;
    case KF_EQUIPMENT_SLOT_HEAD:
        player_state.equipped_head_armor_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_BODY:
        player_state.equipped_body_armor_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_ARM:
        player_state.equipped_arm_armor_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_LEG:
        player_state.equipped_leg_armor_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_SHIELD:
        player_state.equipped_shield_id = item_id;
        break;
    case KF_EQUIPMENT_SLOT_ACCESSORY:
        player_state.equipped_accessory_id = item_id;
        break;
    }
    if (player_state.equipped_head_armor_id != KF_OBJECT_NONE) {
        player_state.equipped_head_armor_record = &armor_records.entries[kf_enum_encode<u8>(player_state.equipped_head_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
    } else {
        player_state.equipped_head_armor_record = NULL;
    }
    if (player_state.equipped_body_armor_id != KF_OBJECT_NONE) {
        player_state.equipped_body_armor_record =
            &armor_records.entries[kf_enum_encode<u8>(player_state.equipped_body_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
    } else {
        player_state.equipped_body_armor_record = NULL;
    }
    if (player_state.equipped_arm_armor_id != KF_OBJECT_NONE) {
        player_state.equipped_arm_armor_record =
            &armor_records.entries[kf_enum_encode<u8>(player_state.equipped_arm_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
    } else {
        player_state.equipped_arm_armor_record = NULL;
    }
    if (player_state.equipped_leg_armor_id != KF_OBJECT_NONE) {
        player_state.equipped_leg_armor_record =
            &armor_records.entries[kf_enum_encode<u8>(player_state.equipped_leg_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
    } else {
        player_state.equipped_leg_armor_record = NULL;
    }
    if (player_state.equipped_shield_id != KF_OBJECT_NONE) {
        player_state.equipped_shield_record =
            &armor_records.entries[kf_enum_encode<u8>(player_state.equipped_shield_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
    } else {
        player_state.equipped_shield_record = NULL;
    }
    player_recalculate_combat_stats();
}

void player_equip_weapon(KfObjectId weapon_id)
{
    player_state.attack_charge_state.current = 0;
    player_state.attack_charge_state.committed = 0;
    player_state.weapon_charge_delay = PLAYER_WEAPON_CHARGE_DELAY_UPDATES;
    player_state.equipped_weapon_id = weapon_id;
    if (weapon_id != KF_OBJECT_NONE) {
        player_state.equipped_weapon_record = &weapon_records.entries[kf_enum_encode<u8>(weapon_id)];
        weapon_asset_path_template[weapon_image_number_offset] = '0' + kf_enum_encode<u32>(weapon_id) / 10;
        weapon_asset_path_template[weapon_image_number_offset + 1] = '0' + kf_enum_encode<u32>(weapon_id) % 10;
        std::size_t loaded_size;
        if (resource_file_load_into(player_state.weapon_asset_buffer, KF_WEAPON_ASSET_BUFFER_BYTES,
                weapon_asset_path_template.data(), &loaded_size) != KF_RESOURCE_LOADED) {
            resource_file_fail(weapon_asset_path_template.data());
        }
        asset_registry_set(KF_ASSET_WEAPON, player_state.weapon_asset_buffer, loaded_size);
    }
    player_state.weapon_attack_phase = KF_WEAPON_ATTACK_INACTIVE;
    player_state.weapon_animation_cache = NULL;
    player_recalculate_combat_stats();
}

void player_begin_weapon_attack(void)
{
    if (player_state.weapon_attack_phase == KF_WEAPON_ATTACK_INACTIVE
        && player_state.equipped_weapon_id != KF_OBJECT_NONE) {
        player_state.weapon_attack_phase = 0;
        sound_ref_play(audio_playback(), &player_sound_refs[KF_PLAYER_SOUND_WEAPON_ATTACK], KF_AUDIO_MAX_VOLUME);
        player_state.attack_charge_state.committed = player_state.attack_charge_state.current;
        if (player_state.attack_charge_state.current == KF_PLAYER_CHARGE_FULL) {
            player_state.weapon_attack_fully_charged = KF_WEAPON_ATTACK_FULL_CHARGE;
        } else {
            player_state.weapon_attack_fully_charged = KF_WEAPON_ATTACK_NORMAL_CHARGE;
        }
        player_state.attack_charge_state.current = 0;
    }
}

void player_update_weapon_attack(void)
{
    SVECTOR offset;
    SVECTOR rotation;
    VECTOR result;
    MATRIX matrix;
    u16 window;
    s32 actor_index;

    if (player_state.equipped_weapon_id == KF_OBJECT_NONE) {
        return;
    }
    if (player_state.weapon_attack_phase != KF_WEAPON_ATTACK_INACTIVE) {
        player_state.weapon_attack_phase += KF_WEAPON_ATTACK_PHASE_STEP;
        window = player_state.weapon_attack_phase;
        if (player_state.equipped_weapon_id == KF_ITEM_COLICHEMARDE
                ? (u16)(window - PLAYER_COLICHEMARDE_HIT_PHASE) < PLAYER_WEAPON_HIT_WINDOW
                : (u16)(window - PLAYER_WEAPON_HIT_PHASE) < PLAYER_WEAPON_HIT_WINDOW) {
            offset = VECTOR{
                0,
                PLAYER_WEAPON_HIT_Y_OFFSET,
                player_state.equipped_weapon_record->attack_z_offset}.narrowed();
            rotation = VECTOR{0, -player_state.camera_rotation.vy, 0}.narrowed();
            kf::matrix_set_rotation_xyz(rotation, matrix);
            result = kf::matrix_apply_rotation(matrix, offset);
            result += player_state.camera_position;
            actor_index = actor_pool_find_overlap(result.vx, result.vy, result.vz,
                PLAYER_WEAPON_HIT_RADIUS, PLAYER_WEAPON_HIT_HEIGHT);
            if (actor_index != -1) {
                actor_apply_damage(
                    actor_index,
                    player_state.physical_power,
                    player_state.cutting_attack,
                    player_state.striking_attack,
                    player_state.piercing_attack,
                    player_state.holy_attack,
                    player_state.fire_attack,
                    player_state.attack_charge_state.committed,
                    KF_ACTOR_DAMAGE_CREDIT_PLAYER);
            }
            player_state.attack_charge_state.committed = 0;
            player_state.attack_charge_state.current = 0;
            player_state.weapon_charge_delay = PLAYER_WEAPON_CHARGE_DELAY_UPDATES;
        }
        if (player_state.weapon_attack_phase >= KF_WEAPON_ATTACK_PHASE_END) {
            player_state.weapon_attack_phase = KF_WEAPON_ATTACK_INACTIVE;
        }
    } else if (player_state.weapon_charge_delay == 0) {
        player_state.attack_charge_state.current +=
            fixed6_ratio_step(
                player_state.physical_power,
                player_state.equipped_weapon_record->charge_rate) * KF_PLAYER_CHARGE_GAIN_MULTIPLIER;
        player_state.attack_charge_state.current =
            std::min<s32>(player_state.attack_charge_state.current, KF_PLAYER_CHARGE_FULL);
    } else {
        player_state.weapon_charge_delay--;
    }
}

void game_initialize_session(void)
{
    player_state.camera_rotation = {};
    player_state.camera_position = {PLAYER_INITIAL_POSITION_X, 0, PLAYER_INITIAL_POSITION_Z};
    player_state.weapon_asset_buffer = static_cast<u8 *>(memory_allocate(memory_arena, KF_WEAPON_ASSET_BUFFER_BYTES));
    game_state_initialize();
    player_state.update_state = KF_PLAYER_UPDATE_NORMAL;
    player_state.audio_effects_enabled = KF_PLAYER_OPTION_ON;
    player_state.audio_music_enabled = KF_PLAYER_OPTION_ON;
    player_state.hud_gauges_enabled = KF_PLAYER_OPTION_ON;
    player_state.compass_enabled = KF_PLAYER_OPTION_ON;
}

void player_get_floor_position(VECTOR &position)
{
    position = {player_state.camera_position.vx, player_state.foot_height,
        player_state.camera_position.vz};
}

bool player_item_is_equipped(KfObjectId item_id)
{
    return item_id == player_state.equipped_weapon_id
        || item_id == player_state.equipped_head_armor_id
        || item_id == player_state.equipped_body_armor_id
        || item_id == player_state.equipped_shield_id
        || item_id == player_state.equipped_arm_armor_id
        || item_id == player_state.equipped_leg_armor_id
        || item_id == player_state.equipped_accessory_id;
}

KfMapAttribute player_current_map_attribute(void)
{
    const auto &cell = player_state.motion_state.map_cell;
    return map_attribute_at_cell(cell.x, cell.z);
}

void player_clear_motion(void)
{
    auto &motion = player_state.motion_state;
    motion.yaw_step = 0;
    motion.pitch_step = 0;
    motion.movement_speed = 0;
    motion.forward_velocity = 0;
    motion.strafe_velocity = 0;
}

void player_sync_position_to_map(void)
{
    auto &cell = player_state.motion_state.map_cell;
    s32 cell_x = player_state.camera_position.vx / KF_MAP_TILE_SIZE;
    s32 cell_z = player_state.camera_position.vz / KF_MAP_TILE_SIZE;
    s32 floor;
    s32 view_offset;
    s32 floor_height;

    player_state.equipment_effect_ticks = 0;
    cell.x = cell_x;
    cell.z = cell_z;
    floor = map_floor_height_grid.cells[cell.z][cell.x];
    player_state.allow_near_actor_spawn = KF_ACTOR_NEAR_SPAWN_ALLOWED;
    floor_height = -(floor * KF_MAP_HEIGHT_STEP);
    view_offset = player_state.view_bob_offset - KF_PLAYER_CAMERA_HEIGHT;
    player_state.foot_height = floor_height;
    player_state.camera_position.vy = view_offset + floor_height;
    player_clear_motion();
    collision_adjust_cell_occupancy(cell.x, cell.z, 1);
    game_graphics_runtime.hud_brightness = KF_HUD_DEFAULT_BRIGHTNESS;
    player_state.vertical_state = KF_PLAYER_VERTICAL_GROUNDED;
    player_state.vertical_velocity = 0;
}

s32 player_distance_to_point_in_cone(
    const VECTOR *point, s16 facing, s32 max_distance, s32 angle_tolerance)
{
    s32 distance;
    s16 delta;

    distance = player_distance_to_point(point->vx, KF_COLLISION_IGNORE_HEIGHT, point->vz, max_distance, 0);
    if (distance != KF_PROXIMITY_NONE) {
        delta = (vector_xz_to_angle(
                     player_state.camera_position.vx - point->vx,
                     point->vz - player_state.camera_position.vz)
                 - facing) & KF_ANGLE_WRAP_MASK;
        delta = angle_error_magnitude(delta);
        if (angle_tolerance < delta) {
            distance = KF_PROXIMITY_NONE;
        }
    }
    return distance;
}

s32 player_distance_to_point(
    s32 point_x, s32 point_y, s32 point_z, s32 max_distance, s32 point_height)
{
    s32 dx;
    s32 dz;
    s32 center;
    s32 dy;
    s32 distance;
    s32 tolerance = point_height;

    dx = player_state.camera_position.vx - point_x;
    if (dx < -max_distance || max_distance < dx) {
        return KF_PROXIMITY_NONE;
    }
    dz = player_state.camera_position.vz - point_z;
    if (dz < -max_distance || max_distance < dz) {
        return KF_PROXIMITY_NONE;
    }
    if (point_y != KF_COLLISION_IGNORE_HEIGHT) {
        tolerance >>= 1;
        center = point_y - tolerance;
        tolerance += KF_COLLISION_PLAYER_HEIGHT / 2;
        center += KF_COLLISION_PLAYER_HEIGHT / 2;
        dy = player_state.foot_height - center;
        if (dy < -tolerance || tolerance < dy) {
            return KF_PROXIMITY_NONE;
        }
    }
    distance = fixed_vector2_length(dx, dz);
    if (max_distance < distance) {
        return KF_PROXIMITY_NONE;
    }
    return distance;
}

s32 player_move_horizontal(s32 heading, s32 distance)
{
    auto &cell = player_state.motion_state.map_cell;
    s32 cell_z0 = cell.z;
    s32 cell_x0 = cell.x;
    s32 dz;
    s32 dx;
    s32 new_z;
    s32 new_x;
    s32 angle;
    s32 radius;
    s32 remainder_z;
    s32 remainder_x;
    s32 half;
    u32 cell_z;
    u32 cell_x;
    SVECTOR delta;
    s16 attempt = 1;
    KfMapCellKind type;

    dz = (kf::angle_cosine(heading) * distance) >> KF_FIXED12_BITS;
    dx = (-kf::angle_sine(heading) * distance) >> KF_FIXED12_BITS;
    new_z = dz + player_state.camera_position.vz;
    new_x = dx + player_state.camera_position.vx;
    for (;;) {
        if (collision_query_world(new_x, player_state.foot_height, new_z,
                KF_COLLISION_PLAYER_RADIUS, KF_COLLISION_PLAYER_HEIGHT,
                KF_COLLISION_SKIP_TERRAIN | KF_COLLISION_SKIP_PLAYER | KF_COLLISION_CAPTURE_TARGET).kind
            == KfCollisionKind::None) {
            break;
        }

        delta.vx = collision_target.position.vx - player_state.camera_position.vx;
        delta.vz = player_state.camera_position.vz - collision_target.position.vz;
        angle = vector_xz_to_angle(delta.vx, delta.vz);
        angle = (angle_mod_delta_le_half_turn(heading, angle) == 0
            ? angle + (KF_ANGLE_HALF_TURN + PLAYER_COLLISION_DEFLECTION_ANGLE)
            : angle + (KF_ANGLE_HALF_TURN - PLAYER_COLLISION_DEFLECTION_ANGLE))
            & KF_ANGLE_WRAP_MASK;
        radius = collision_target.radius
            + (KF_COLLISION_PLAYER_RADIUS + PLAYER_COLLISION_SLIDE_CLEARANCE);
        delta.vz = (kf::angle_cosine(angle) * radius) >> KF_FIXED12_BITS;
        delta.vx = (-kf::angle_sine(angle) * radius) >> KF_FIXED12_BITS;
        attempt--;
        new_z = collision_target.position.vz + delta.vz;
        dz = new_z - player_state.camera_position.vz;
        new_x = collision_target.position.vx + delta.vx;
        dx = new_x - player_state.camera_position.vx;
        if (attempt == -1) {
            return 1;
        }
    }
    cell_z = new_z / KF_MAP_TILE_SIZE;
    if (cell_z < KF_MAP_ROWS && map_collision_grid.cells[cell_z][cell.x] != KF_MAP_CELL_BLOCKED
        && map_base_floor_height(cell.x, cell_z) - player_state.foot_height
               >= -PLAYER_MAX_STEP_RISE) {
        player_state.camera_position.vz = new_z;
        cell.z = cell_z;
    }
    cell_x = new_x / KF_MAP_TILE_SIZE;
    if (cell_x < KF_MAP_COLUMNS && map_collision_grid.cells[cell.z][cell_x] != KF_MAP_CELL_BLOCKED
        && map_base_floor_height(cell_x, cell.z) - player_state.foot_height
               >= -PLAYER_MAX_STEP_RISE) {
        player_state.camera_position.vx = new_x;
        cell.x = cell_x;
    }
    type = map_collision_grid.cells[cell_z0][cell_x0];
    if (type >= KF_MAP_CELL_X_GE_Z && type <= KF_MAP_CELL_SUM_GE_SIZE) {
        if (cell.x == cell_x0 && cell.z == cell_z0) {
            remainder_z = player_state.camera_position.vz % KF_MAP_TILE_SIZE;
            remainder_x = player_state.camera_position.vx % KF_MAP_TILE_SIZE;
            if (type == KF_MAP_CELL_X_GE_Z) {
                if (remainder_x < remainder_z) {
                    half = (remainder_z - remainder_x) / 2;
                    player_state.camera_position.vz -= half;
                    player_state.camera_position.vx += half;
                }
            } else if (type == KF_MAP_CELL_SUM_LE_SIZE) {
                if (remainder_z + remainder_x >= KF_MAP_TILE_SIZE + 1) {
                    half = (remainder_z + remainder_x - KF_MAP_TILE_SIZE) / 2;
                    player_state.camera_position.vz -= half;
                    player_state.camera_position.vx -= half;
                }
            } else if (type == KF_MAP_CELL_Z_GE_X) {
                if (remainder_z < remainder_x) {
                    half = (remainder_x - remainder_z) / 2;
                    player_state.camera_position.vz += half;
                    player_state.camera_position.vx -= half;
                }
            } else if (type == KF_MAP_CELL_SUM_GE_SIZE) {
                if (remainder_z + remainder_x < KF_MAP_TILE_SIZE) {
                    half = (KF_MAP_TILE_SIZE - (remainder_z + remainder_x)) / 2;
                    player_state.camera_position.vz += half;
                    player_state.camera_position.vx += half;
                }
            }
            cell.z = player_state.camera_position.vz / KF_MAP_TILE_SIZE;
            cell.x = player_state.camera_position.vx / KF_MAP_TILE_SIZE;
        }
        if (map_collision_grid.cells[cell_z][cell_x] == KF_MAP_CELL_BLOCKED) {
            if (dz < 0) {
                dz = -dz;
            }
            if (dx < 0) {
                dx = -dx;
            }
            type = map_collision_grid.cells[cell.z][cell.x];
            if (type == KF_MAP_CELL_X_GE_Z) {
                if (dz < dx) {
                    dx = -(distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dz = dx;
                } else {
                    dx = (distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dz = dx;
                }
            } else if (type == KF_MAP_CELL_SUM_LE_SIZE) {
                if (dz < dx) {
                    dz = -(distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dx = (distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                } else {
                    dz = (distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dx = -(distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                }
            } else if (type == KF_MAP_CELL_Z_GE_X) {
                if (dx < dz) {
                    dx = -(distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dz = dx;
                } else {
                    dx = (distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dz = dx;
                }
            } else if (type == KF_MAP_CELL_SUM_GE_SIZE) {
                if (dx < dz) {
                    dz = -(distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dx = (distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                } else {
                    dz = (distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                    dx = -(distance * PLAYER_DIAGONAL_COMPONENT_Q12) >> KF_FIXED12_BITS;
                }
            }
            new_z = dz + player_state.camera_position.vz;
            cell_z = new_z / KF_MAP_TILE_SIZE;
            new_x = dx + player_state.camera_position.vx;
            cell_x = new_x / KF_MAP_TILE_SIZE;
            if (cell_z < KF_MAP_ROWS && cell_x < KF_MAP_COLUMNS && map_collision_grid.cells[cell_z][cell_x] != KF_MAP_CELL_BLOCKED) {
                player_state.camera_position.vz = new_z;
                player_state.camera_position.vx = new_x;
                cell.z = cell_z;
                cell.x = cell_x;
            }
        }
    }
    return 1;
}

void player_update_view_bob(void)
{
    s32 phase;

    if (player_state.vertical_state == KF_PLAYER_VERTICAL_GROUNDED) {
        phase = (player_state.view_bob_phase
            + player_state.motion_state.movement_speed * PLAYER_BOB_PHASE_PER_SPEED)
            & KF_ANGLE_WRAP_MASK;
        player_state.view_bob_phase = phase;
        player_state.view_bob_offset = kf::angle_sine(phase) >> PLAYER_BOB_SINE_DOWNSHIFT;
    }
}

static void player_update_floor_motion(s32 target)
{
    if (player_state.update_state != KF_PLAYER_UPDATE_DYING) {
        if (player_state.foot_height - target < -PLAYER_FATAL_DROP_DISTANCE) {
            if (player_state.equipped_leg_armor_id == KF_ITEM_FEATHER_BOOTS
                && player_current_map_attribute() == KF_MAP_ATTRIBUTE_BOTTOMLESS_PIT) {
                return;
            }
            player_death_begin();
        } else if (target >= PLAYER_ATTRIBUTE_52_FATAL_HEIGHT
                   && player_current_map_attribute() == KF_MAP_ATTRIBUTE_52) {
            player_death_begin();
        }
    }

    if (player_state.vertical_state == KF_PLAYER_VERTICAL_GROUNDED) {
        if (target < player_state.foot_height) {
            player_state.vertical_state = KF_PLAYER_VERTICAL_STEP_UP;
            if ((s16)player_state.motion_state.movement_speed >= PLAYER_FAST_STEP_MIN_SPEED) {
                player_state.vertical_velocity = PLAYER_FAST_STEP_UP_VELOCITY;
            } else {
                player_state.vertical_velocity = PLAYER_SLOW_STEP_UP_VELOCITY;
            }
        } else if (player_state.foot_height < target) {
            player_state.vertical_state = KF_PLAYER_VERTICAL_FALLING;
            player_state.vertical_velocity = 0;
        }
    }

    // A new fall or step begins moving on this same update.
    switch (player_state.vertical_state) {
    case KF_PLAYER_VERTICAL_FALLING:
        player_state.foot_height += player_state.vertical_velocity;
        player_state.vertical_velocity += PLAYER_FALL_ACCELERATION;
        if (target + PLAYER_FALL_LANDING_OVERSHOOT < player_state.foot_height) {
            player_state.foot_height = target;
            player_state.vertical_state = KF_PLAYER_VERTICAL_GROUNDED;
        }
        break;
    case KF_PLAYER_VERTICAL_STEP_UP:
        player_state.foot_height += player_state.vertical_velocity;
        player_state.vertical_velocity += PLAYER_STEP_UP_ACCELERATION;
        if (player_state.foot_height <= target) {
            player_state.foot_height = target;
            player_state.vertical_state = KF_PLAYER_VERTICAL_GROUNDED;
        }
        break;
    case KF_PLAYER_VERTICAL_GROUNDED:
        break;
    }
}

void player_update_vertical_motion(void)
{
    auto &cell = player_state.motion_state.map_cell;
    s32 target;
    s32 view_offset;

    target = map_base_floor_height(cell.x, cell.z);
    player_update_floor_motion(target);
    view_offset = player_state.view_bob_offset - KF_PLAYER_CAMERA_HEIGHT;
    player_state.camera_position.vy = view_offset + player_state.foot_height;
}

void player_warp_to_floor_entry(void)
{
    VECTOR position;
    const KfFloorEntryCell *entry;
    KfFloorId floor;

    player_get_floor_position(position);
    player_warp_shimmer(KF_WARP_SHIMMER_GROW_REMOVE, &position);
    floor = player_state.progress_state.current_floor;
    entry = &floor_entry_cells[kf_enum_encode<u8>(floor) - 1];
    player_state.previous_map_cell.x = entry->x;
    player_state.previous_map_cell.z = entry->z;
    player_state.camera_position.vx = player_state.previous_map_cell.x * KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER;
    position.vx = player_state.camera_position.vx;
    player_state.camera_position.vz = player_state.previous_map_cell.z * KF_MAP_TILE_SIZE + KF_MAP_TILE_CENTER;
    position.vz = player_state.camera_position.vz;
    if (floor == KF_FLOOR_5 && player_state.map_variant != KF_FLOOR5_ENTRY_VARIANT) {
        if (player_state.map_variant == KF_FLOOR5_ALTERNATE_MUSIC_VARIANT) {
            audio_play_current_map_sequence();
        }
        animation_cache_release_all();
        player_state.map_variant = KF_FLOOR5_ENTRY_VARIANT;
        map_variant_assets_load();
    }
    player_sync_position_to_map();
    position.vy = player_state.foot_height;
    player_warp_shimmer(KF_WARP_SHIMMER_SHRINK_REMOVE, &position);
}

void player_update_transform_snapshot(VECTOR *position_out, SVECTOR *rotation_out)
{
    *position_out = player_state.camera_position;
    *rotation_out = player_state.camera_rotation;
    *rotation_out += player_state.view_rotation_offset;
}

void player_core_reset_module_state(void)
{
    kf::restore_initial_value<weapon_asset_path_template>();
    kf::restore_initial_value<floor_entry_cells>();
    kf::restore_initial_value<player_rotation_snapshot>();
    kf::restore_initial_value<player_position_snapshot>();
    kf::restore_initial_value<player_level_growth_table>();
    kf::restore_initial_value<player_state>();
}
