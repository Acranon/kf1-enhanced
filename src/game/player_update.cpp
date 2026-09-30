#include <kf/platform/prelude.h>
#include <kf/game/audio.h>
#include <kf/game/collision.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/player.h>
#include <kf/game/session.h>
#include <kf/lib/null.h>
#include <kf/lib/random.h>
#include <kf/platform/input.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr u16 PLAYER_PITFALL_CUTTING_DAMAGE = 5;
static constexpr u16 PLAYER_PITFALL_STRIKING_DAMAGE = 3;
static constexpr u16 PLAYER_PITFALL_PIERCING_DAMAGE = 5;

enum {
    POISON_DAMAGE_INTERVAL_UPDATES = 20,
    POISON_FLASH_UPDATES = 2
};

enum {
    PLAYER_WEAPON_MAGIC_PHASE_FIRST = 2400,
    PLAYER_WEAPON_MAGIC_PHASE_LAST = 3900,
    PLAYER_COLICHEMARDE_MAGIC_PHASE_FIRST = 900,
    PLAYER_COLICHEMARDE_MAGIC_PHASE_LAST = 2400
};

static constexpr int DARKNESS_FADE_BITS = 5;
static constexpr int DARKNESS_FADE_STEPS = 1 << DARKNESS_FADE_BITS;
static constexpr int PLAYER_NORMAL_MOVEMENT_LIMIT = 180;
static constexpr int PLAYER_SLOWED_MOVEMENT_LIMIT = PLAYER_NORMAL_MOVEMENT_LIMIT / 5;
static constexpr int PLAYER_NORMAL_TURN_LIMIT = 28;
static constexpr int PLAYER_SLOWED_TURN_LIMIT = 5;
static constexpr int PLAYER_DARKNESS_FOG_NEAR = 5000;

enum {
    PLAYER_FORWARD_ACCEL_SHIFT = 2,
    PLAYER_FORWARD_DECEL_SHIFT = 3,
    PLAYER_STRAFE_ACCEL_DECEL_SHIFT = 2,
    PLAYER_YAW_ACCEL_DECEL_SHIFT = 2,
    PLAYER_PITCH_ACCEL = 3,
    PLAYER_PITCH_DECEL = 2,
    PLAYER_PITCH_STEP_LIMIT = 10
};

enum {
    PLAYER_WEAPON_MAGIC_READY = 1,
    PLAYER_TRIPLE_FANG_MAGIC_DELAY = 3,
    PLAYER_FLAME_SWORD_MAGIC_DELAY = 2,
    PLAYER_MOONLIGHT_SWORD_MAGIC_DELAY = 3,
    PLAYER_TRIPLE_FANG_MAGIC_MIN_POWER = 80,
    PLAYER_MOONLIGHT_SWORD_MAGIC_MIN_POWER = 80,
    PLAYER_COLICHEMARDE_MAGIC_MIN_POWER = 60,
    PLAYER_COLICHEMARDE_SHOTS_PER_PHASE = 2,
    PLAYER_WEAPON_MAGIC_SPAWN_X = 200,
    PLAYER_WEAPON_MAGIC_SPAWN_Y = 200,
    PLAYER_WEAPON_MAGIC_SPAWN_Z = 400,
    PLAYER_WEAPON_MAGIC_BURST_CONE = 0x155,
    PLAYER_WEAPON_MAGIC_JITTER_BIAS = 4,
    PLAYER_WEAPON_MAGIC_RANDOM_SHIFT = 9,
    PLAYER_WEAPON_MAGIC_SPEED = 900,
    PLAYER_TRIPLE_FANG_Y_OFFSET = 300,
    PLAYER_TRIPLE_FANG_PITCH_OFFSET = 64
};

static MATRIX player_darkness_color_matrix = {
    .m = {{
        {666, 233, 1333},
        {666, 233, 1333},
        {666, 233, 1333},
    }},
    .t = {},
};

static std::array<SVECTOR, kf_enum_encode<u8>(KF_PLAYER_DAMAGE_FRAME_END)> player_damage_view_rotation_offsets = {
    SVECTOR{0, 0, 0, 0},
    SVECTOR{-32, 0, -32, 0},
    SVECTOR{-64, 0, -64, 0},
    SVECTOR{-48, 0, -32, 0},
    SVECTOR{-32, 0, 0, 0},
    SVECTOR{-16, 0, 32, 0},
    SVECTOR{0, 0, 64, 0},
    SVECTOR{-16, 0, 32, 0}
};

static u32 player_previous_input = 0;

static s32 player_movement_velocity_limit;

static s32 player_turn_step_limit;

static void player_handle_magic_input(u32 input)
{
    s32 cost;
    KfEffectKind magic_id;

    if (player_state.equipped_body_armor_id != KF_ITEM_SKULL_ARMOR) {
        if (kf::button_pressed(input, player_previous_input, kf::Button::Magic)) {
            if (player_state.weapon_attack_fully_charged == KF_WEAPON_ATTACK_FULL_CHARGE) {
                player_state.weapon_attack_fully_charged = KF_WEAPON_ATTACK_NORMAL_CHARGE;
                switch (player_state.equipped_weapon_id) {
                default:
                    // Other weapons do not arm a weapon-magic attack.
                    break;
                case KF_ITEM_FLAME_SWORD:
                    if (player_state.weapon_attack_phase >= PLAYER_WEAPON_MAGIC_PHASE_FIRST
                        && player_state.weapon_attack_phase <= PLAYER_WEAPON_MAGIC_PHASE_LAST) {
                        player_state.weapon_magic_delay = PLAYER_WEAPON_MAGIC_READY;
                        player_state.weapon_magic_shots_remaining =
                            (PLAYER_WEAPON_MAGIC_PHASE_LAST - player_state.weapon_attack_phase)
                            / KF_WEAPON_ATTACK_PHASE_STEP + 1;
                        return;
                    }
                    break;
                case KF_ITEM_TRIPLE_FANG:
                case KF_ITEM_MOONLIGHT_SWORD:
                    if (player_state.weapon_attack_phase >= PLAYER_WEAPON_MAGIC_PHASE_FIRST
                        && player_state.weapon_attack_phase <= PLAYER_WEAPON_MAGIC_PHASE_LAST) {
                        player_state.weapon_magic_shots_remaining = 1;
                        player_state.weapon_magic_delay = PLAYER_WEAPON_MAGIC_READY;
                        return;
                    }
                    break;
                case KF_ITEM_COLICHEMARDE:
                    if (player_state.weapon_attack_phase >= PLAYER_COLICHEMARDE_MAGIC_PHASE_FIRST
                        && player_state.weapon_attack_phase <= PLAYER_COLICHEMARDE_MAGIC_PHASE_LAST) {
                        player_state.weapon_magic_delay = PLAYER_WEAPON_MAGIC_READY;
                        player_state.weapon_magic_shots_remaining =
                            ((PLAYER_WEAPON_MAGIC_PHASE_LAST - player_state.weapon_attack_phase)
                             / KF_WEAPON_ATTACK_PHASE_STEP + 1)
                            * PLAYER_COLICHEMARDE_SHOTS_PER_PHASE;
                        return;
                    }
                    break;
                }
            }
            if (player_state.selected_magic_id != KF_MAGIC_NONE
                && player_state.magic_charge == KF_PLAYER_CHARGE_FULL) {
                player_state.weapon_magic_delay = 0;
                player_state.weapon_magic_shots_remaining = 0;
                if (player_state.equipped_accessory_id == KF_ITEM_WIND_BLADE_BRACELET && player_state.selected_magic_id == KF_MAGIC_WIND_CUTTER) {
                    cost = player_state.selected_magic_record->mp_cost >> 1;
                } else {
                    cost = player_state.selected_magic_record->mp_cost;
                }
                if (player_state.vitals.current_mp >= cost) {
                    player_state.vitals.current_mp -= cost;
                    magic_cast();
                    player_state.magic_charge = 0;
                }
            }
            player_state.weapon_attack_fully_charged = KF_WEAPON_ATTACK_NORMAL_CHARGE;
        } else {
            magic_id = player_state.selected_magic_id;
            if (magic_id != KF_MAGIC_NONE) {
                player_state.magic_charge +=
                    fixed6_ratio_step(player_state.magic, player_state.selected_magic_record->charge_rate)
                    * KF_PLAYER_CHARGE_GAIN_MULTIPLIER;
                player_state.magic_charge =
                    std::clamp<s32>(player_state.magic_charge, 0, KF_PLAYER_CHARGE_FULL);
            }
        }
    }
}

static void player_cancel_weapon_magic()
{
    player_state.weapon_magic_shots_remaining = 0;
    player_state.weapon_magic_delay = PLAYER_WEAPON_MAGIC_READY + 1;
}

static void player_update_weapon_magic()
{
    KfEffectKind effect;
    KfEffectHomingMode homing_target;
    KfMagicRecord *record;
    KfActor *target;
    const VECTOR *origin;
    SVECTOR direction;
    KfRotation effect_rotation;
    SVECTOR *launch_direction;
    SVECTOR spawn_offset;
    VECTOR position;
    MATRIX matrix;
    s32 distance;

    if (player_state.weapon_magic_delay == PLAYER_WEAPON_MAGIC_READY) {
        if (player_state.weapon_magic_shots_remaining != 0) {
            switch (player_state.equipped_weapon_id) {
            case KF_ITEM_TRIPLE_FANG:
                if (player_state.physical_power < PLAYER_TRIPLE_FANG_MAGIC_MIN_POWER
                    || player_state.magic < PLAYER_TRIPLE_FANG_MAGIC_MIN_POWER) {
                    player_cancel_weapon_magic();
                    return;
                }
                effect = KF_EFFECT_KIND_HOMING_PROJECTILE;
                record = &effect_state.magic.entries[kf_enum_encode<u8>(KF_EFFECT_KIND_HOMING_PROJECTILE)];
                player_state.weapon_magic_delay = PLAYER_TRIPLE_FANG_MAGIC_DELAY;
                break;
            case KF_ITEM_FLAME_SWORD:
                if (effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_FIRE_BALL)].learned == KF_MAGIC_UNLEARNED) {
                    player_cancel_weapon_magic();
                    return;
                }
                effect = KF_MAGIC_FIRE_BALL;
                record = &effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_FIRE_BALL)];
                player_state.weapon_magic_delay = PLAYER_FLAME_SWORD_MAGIC_DELAY;
                break;
            case KF_ITEM_MOONLIGHT_SWORD:
                if (player_state.physical_power < PLAYER_MOONLIGHT_SWORD_MAGIC_MIN_POWER
                    || player_state.magic < PLAYER_MOONLIGHT_SWORD_MAGIC_MIN_POWER) {
                    player_cancel_weapon_magic();
                    return;
                }
                effect = KF_EFFECT_KIND_MOONLIGHT_PROJECTILE;
                record = &effect_state.magic.entries[kf_enum_encode<u8>(KF_EFFECT_KIND_RADIAL_BLAST)];
                player_state.weapon_magic_delay = PLAYER_MOONLIGHT_SWORD_MAGIC_DELAY;
                break;
            case KF_ITEM_COLICHEMARDE:
                if (player_state.physical_power < PLAYER_COLICHEMARDE_MAGIC_MIN_POWER
                    || player_state.magic < PLAYER_COLICHEMARDE_MAGIC_MIN_POWER) {
                    player_cancel_weapon_magic();
                    return;
                }
                effect = KF_MAGIC_LIGHT_NEEDLE;
                record = &effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_LIGHT_NEEDLE)];
                player_state.weapon_magic_delay = PLAYER_WEAPON_MAGIC_READY;
                break;
            default:
                player_cancel_weapon_magic();
                return;
            }
            if (player_state.vitals.current_mp >= record->mp_cost) {
                if (player_state.weapon_magic_shots_remaining == 1) {
                    player_state.vitals.current_mp -= record->mp_cost;
                }
                spawn_offset = {
                    PLAYER_WEAPON_MAGIC_SPAWN_X,
                    PLAYER_WEAPON_MAGIC_SPAWN_Y,
                    PLAYER_WEAPON_MAGIC_SPAWN_Z};
                effect_rotation.vector = VECTOR{
                    -player_state.camera_rotation.vx,
                    player_state.camera_rotation.vy,
                    -player_state.camera_rotation.vz}.narrowed();
                matrix_set_rotation_yxz(&effect_rotation.angles, &matrix);
                position = kf::matrix_apply_rotation(matrix, spawn_offset);
                position += player_state.camera_position;
                effect_rotation.vector = player_state.camera_rotation;
                origin = &player_state.camera_position;
                if ((effect == KF_MAGIC_FIRE_BALL || effect == KF_MAGIC_LIGHT_NEEDLE)
                    && player_state.weapon_magic_shots_remaining != 1) {
                    actor_state.player_target = actor_pool_find_target_in_cone(
                        origin, player_state.camera_rotation.vy, KF_EFFECT_ACTOR_TARGET_MAX_DISTANCE,
                        PLAYER_WEAPON_MAGIC_BURST_CONE, &distance);
                    effect_rotation.angles.x -= PLAYER_WEAPON_MAGIC_JITTER_BIAS
                        - (kf::random_next() >> PLAYER_WEAPON_MAGIC_RANDOM_SHIFT);
                    effect_rotation.angles.y -= PLAYER_WEAPON_MAGIC_JITTER_BIAS
                        - (kf::random_next() >> PLAYER_WEAPON_MAGIC_RANDOM_SHIFT);
                    homing_target = kf_enum_decode<KfEffectHomingMode>(player_state.weapon_magic_shots_remaining & 1);
                } else {
                    target = actor_pool_find_target_in_cone(
                        &player_state.camera_position,
                        player_state.camera_rotation.vy, KF_EFFECT_ACTOR_TARGET_MAX_DISTANCE,
                        KF_EFFECT_ACTOR_TARGET_WIDE_CONE, &distance);
                    actor_state.player_target = target;
                    if (target == NULL) {
                        homing_target = KF_EFFECT_HOMING_WANDER;
                    } else {
                        homing_target = kf_enum_decode<KfEffectHomingMode>(target - actor_state.actors.data());
                    }
                }
                launch_direction = &direction;
                pitch_yaw_to_forward_vector(&effect_rotation.angles, launch_direction);
                vector3s_scale_shift12(PLAYER_WEAPON_MAGIC_SPEED, launch_direction);
                effect_pool_construct(
                    KF_PLAYER_DAMAGE_MULTIPLIER_ONE, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_COLLISION_TARGET_ACTORS, effect,
                    &position, launch_direction, KfEffectHomingArguments{&player_state.camera_rotation, homing_target, KF_EFFECT_SOUND_PLAY});
                if (effect == KF_EFFECT_KIND_HOMING_PROJECTILE) {
                    position.vy += PLAYER_TRIPLE_FANG_Y_OFFSET;
                    effect_rotation.vector = VECTOR{
                        player_state.camera_rotation.vx + PLAYER_TRIPLE_FANG_PITCH_OFFSET,
                        player_state.camera_rotation.vy,
                        player_state.camera_rotation.vz}.narrowed();
                    effect_pool_construct(
                        KF_PLAYER_DAMAGE_MULTIPLIER_ONE, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_COLLISION_TARGET_ACTORS,
                        KF_EFFECT_KIND_HOMING_PROJECTILE, &position, launch_direction, KfEffectHomingArguments{&effect_rotation.vector, homing_target, KF_EFFECT_SOUND_SILENT});
                    effect_rotation.angles.x -= 2 * PLAYER_TRIPLE_FANG_PITCH_OFFSET;
                    position.vy -= 2 * PLAYER_TRIPLE_FANG_Y_OFFSET;
                    effect_pool_construct(
                        KF_PLAYER_DAMAGE_MULTIPLIER_ONE, KF_EFFECT_USE_PLAYER_MAGIC | KF_EFFECT_COLLISION_TARGET_ACTORS,
                        KF_EFFECT_KIND_HOMING_PROJECTILE, &position, launch_direction, KfEffectHomingArguments{&effect_rotation.vector, homing_target, KF_EFFECT_SOUND_SILENT});
                }
            }
            player_state.weapon_magic_shots_remaining--;
        }
    } else if (player_state.weapon_magic_delay != 0) {
        player_state.weapon_magic_delay--;
    }
}

static void player_apply_armor_periodic_effects(const KfArmorRecord &armor)
{
    if (armor.hp_regen_interval != 0
        && player_state.equipment_effect_ticks % armor.hp_regen_interval == 0) {
        player_adjust_hp(1);
    }
    if (armor.hp_drain_interval != 0
        && player_state.equipment_effect_ticks % armor.hp_drain_interval == 0) {
        player_adjust_hp(-1);
    }
}

static void player_update_movement_input(u32 input)
{
    auto &motion = player_state.motion_state;
    s16 forward;
    s16 strafe;
    s32 strafe_sq;
    s32 forward_sq;
    s16 magnitude;

    if ((player_state.status_effect_flags & KF_PLAYER_STATUS_SLOWED) != KF_PLAYER_STATUS_NONE) {
        player_movement_velocity_limit = PLAYER_SLOWED_MOVEMENT_LIMIT;
        player_turn_step_limit = PLAYER_SLOWED_TURN_LIMIT;
    } else {
        player_movement_velocity_limit = PLAYER_NORMAL_MOVEMENT_LIMIT;
        if (kf::host_run_held()) {
            player_movement_velocity_limit = PLAYER_NORMAL_MOVEMENT_LIMIT * kf::player_run_speed / 100;
        }
        player_turn_step_limit = PLAYER_NORMAL_TURN_LIMIT;
    }
    if (input & kf::Button::Up) {
        forward = motion.forward_velocity
            + (player_movement_velocity_limit >> PLAYER_FORWARD_ACCEL_SHIFT);
        motion.forward_velocity = std::min<s32>(forward, player_movement_velocity_limit);
    } else if (input & kf::Button::Down) {
        forward = motion.forward_velocity
            - (player_movement_velocity_limit >> PLAYER_FORWARD_ACCEL_SHIFT);
        motion.forward_velocity = std::max<s32>(forward, -player_movement_velocity_limit);
    } else if (motion.forward_velocity > 0) {
        motion.forward_velocity -=
            player_movement_velocity_limit >> PLAYER_FORWARD_DECEL_SHIFT;
        motion.forward_velocity = std::max<s32>(motion.forward_velocity, 0);
    } else if (motion.forward_velocity < 0) {
        motion.forward_velocity +=
            player_movement_velocity_limit >> PLAYER_FORWARD_DECEL_SHIFT;
        motion.forward_velocity = std::min<s32>(motion.forward_velocity, 0);
    }
    if (input & kf::Button::StrafeRight) {
        strafe = motion.strafe_velocity
            + (player_movement_velocity_limit >> PLAYER_STRAFE_ACCEL_DECEL_SHIFT);
        motion.strafe_velocity = std::min<s32>(strafe, player_movement_velocity_limit);
    } else if (input & kf::Button::StrafeLeft) {
        strafe = motion.strafe_velocity
            - (player_movement_velocity_limit >> PLAYER_STRAFE_ACCEL_DECEL_SHIFT);
        motion.strafe_velocity = std::max<s32>(strafe, -player_movement_velocity_limit);
    } else if (motion.strafe_velocity > 0) {
        motion.strafe_velocity -=
            player_movement_velocity_limit >> PLAYER_STRAFE_ACCEL_DECEL_SHIFT;
        motion.strafe_velocity = std::max<s32>(motion.strafe_velocity, 0);
    } else if (motion.strafe_velocity < 0) {
        motion.strafe_velocity +=
            player_movement_velocity_limit >> PLAYER_STRAFE_ACCEL_DECEL_SHIFT;
        motion.strafe_velocity = std::min<s32>(motion.strafe_velocity, 0);
    }
    strafe_sq = motion.strafe_velocity;
    strafe_sq *= strafe_sq;
    forward_sq = motion.forward_velocity;
    forward_sq *= forward_sq;
    magnitude = kf::length_square_root(strafe_sq + forward_sq);
    if (magnitude == 0) {
        forward = 0;
        strafe = 0;
    } else {
        strafe = strafe_sq / magnitude;
        if (motion.strafe_velocity < 0) {
            strafe = -(strafe_sq / magnitude);
        }
        forward = forward_sq / magnitude;
        if (motion.forward_velocity < 0) {
            forward = -(forward_sq / magnitude);
        }
    }
    motion.movement_speed = kf::length_square_root(strafe * strafe + forward * forward);
    if (forward > 0) {
        player_move_horizontal(player_state.camera_rotation.vy, forward);
    } else if (forward < 0) {
        player_move_horizontal(
            (player_state.camera_rotation.vy + KF_ANGLE_HALF_TURN) & KF_ANGLE_WRAP_MASK, -forward);
    }
    if (strafe > 0) {
        player_move_horizontal(
            (player_state.camera_rotation.vy - KF_ANGLE_QUARTER_TURN) & KF_ANGLE_WRAP_MASK, strafe);
    } else if (strafe < 0) {
        player_move_horizontal(
            (player_state.camera_rotation.vy + KF_ANGLE_QUARTER_TURN) & KF_ANGLE_WRAP_MASK, -strafe);
    }
    player_update_view_bob();
}

static void player_update_view_input(u32 input)
{
    auto &motion = player_state.motion_state;

    if (input & kf::Button::Left) {
        motion.yaw_step += player_turn_step_limit >> PLAYER_YAW_ACCEL_DECEL_SHIFT;
        motion.yaw_step = std::min<s32>(motion.yaw_step, player_turn_step_limit);
    } else if (input & kf::Button::Right) {
        motion.yaw_step -= player_turn_step_limit >> PLAYER_YAW_ACCEL_DECEL_SHIFT;
        motion.yaw_step = std::max<s32>(motion.yaw_step, -player_turn_step_limit);
    } else if (motion.yaw_step > 0) {
        motion.yaw_step -= player_turn_step_limit >> PLAYER_YAW_ACCEL_DECEL_SHIFT;
        motion.yaw_step = std::max<s32>(motion.yaw_step, 0);
    } else if (motion.yaw_step < 0) {
        motion.yaw_step += player_turn_step_limit >> PLAYER_YAW_ACCEL_DECEL_SHIFT;
        motion.yaw_step = std::min<s32>(motion.yaw_step, 0);
    }
    player_state.camera_rotation.vy =
        (player_state.camera_rotation.vy + motion.yaw_step) & KF_ANGLE_WRAP_MASK;
    if (input & kf::Button::LookDown) {
        motion.pitch_step += PLAYER_PITCH_ACCEL;
        motion.pitch_step = std::min<s32>(motion.pitch_step, PLAYER_PITCH_STEP_LIMIT);
    } else if (input & kf::Button::LookUp) {
        motion.pitch_step -= PLAYER_PITCH_ACCEL;
        motion.pitch_step = std::max<s32>(motion.pitch_step, -PLAYER_PITCH_STEP_LIMIT);
    } else if (motion.pitch_step > 0) {
        motion.pitch_step -= PLAYER_PITCH_DECEL;
        motion.pitch_step = std::max<s32>(motion.pitch_step, 0);
    } else if (motion.pitch_step < 0) {
        motion.pitch_step += PLAYER_PITCH_DECEL;
        motion.pitch_step = std::min<s32>(motion.pitch_step, 0);
    }
    if (motion.pitch_step > 0) {
        player_state.camera_rotation.vx += motion.pitch_step;
        player_state.camera_rotation.vx =
            std::min<s32>(player_state.camera_rotation.vx, kf::player_pitch_limit);
    } else if (motion.pitch_step < 0) {
        player_state.camera_rotation.vx += motion.pitch_step;
        player_state.camera_rotation.vx =
            std::max<s32>(player_state.camera_rotation.vx, -kf::player_pitch_limit);
    }
}

static void player_update_darkness()
{
    s32 fade;

    if (player_state.darkness_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
        if (!((player_state.status_effect_flags & KF_PLAYER_STATUS_DARKNESS) != KF_PLAYER_STATUS_NONE)
            && player_state.darkness_timer >= DARKNESS_FADE_STEPS + 1) {
            player_state.darkness_timer = DARKNESS_FADE_STEPS;
        }
        player_state.darkness_timer--;
        if (player_state.darkness_timer == KF_PLAYER_STATUS_TIMER_INACTIVE) {
            player_state.status_effect_flags &= ~KF_PLAYER_STATUS_DARKNESS;
        } else {
            fade = (s16)(player_state.darkness_timer
                - (KF_DARKNESS_DURATION_UPDATES - DARKNESS_FADE_STEPS));
            if (fade < 0) {
                fade = (s16)(DARKNESS_FADE_STEPS - player_state.darkness_timer);
            }
            if (fade >= 0) {
                lighting_set_color_matrix(game_graphics_runtime.render_state, &player_darkness_color_matrix, color_matrix_table.data(),
                    fade << (KF_FIXED12_BITS - DARKNESS_FADE_BITS));
                fog_interpolate_near(game_graphics_runtime.render_state, PLAYER_DARKNESS_FOG_NEAR, KF_INITIAL_FOG_NEAR_DISTANCE,
                    fade << (KF_FIXED12_BITS - DARKNESS_FADE_BITS));
            } else {
                game_graphics_runtime.render_state.lighting.color_matrix.m = (player_darkness_color_matrix).m;
                fog_set_near(game_graphics_runtime.render_state, PLAYER_DARKNESS_FOG_NEAR);
            }
        }
    } else {
        fog_set_near(game_graphics_runtime.render_state, KF_INITIAL_FOG_NEAR_DISTANCE);
    }
}

static void player_update_damage_reaction()
{
    if (player_state.update_state != KF_PLAYER_UPDATE_NORMAL
        && player_state.update_state != KF_PLAYER_UPDATE_DYING) {
        if (player_state.update_state >= KF_PLAYER_DAMAGE_FRAME_END) {
            player_state.update_state = KF_PLAYER_UPDATE_NORMAL;
            player_state.view_rotation_offset = player_damage_view_rotation_offsets[kf_enum_encode<u8>(KF_PLAYER_UPDATE_NORMAL)];
            if (player_state.vitals.current_hp == 0) {
                player_death_begin();
            }
        } else {
            player_state.view_rotation_offset = player_damage_view_rotation_offsets[kf_enum_encode<u8>(player_state.update_state)];
            lighting_set_active_color_matrix(KF_GAME_COLOR_DAMAGE);
            player_state.update_state++;
        }
    }
}

static void player_update_equipment_effects()
{
    if (player_state.equipped_weapon_id != KF_OBJECT_NONE) {
        if (player_state.equipped_weapon_record->hp_regen_interval != 0
            && player_state.equipment_effect_ticks % player_state.equipped_weapon_record->hp_regen_interval == 0) {
            player_adjust_hp(1);
        }
        if (player_state.equipped_weapon_record->mp_regen_interval != 0
            && player_state.equipment_effect_ticks % player_state.equipped_weapon_record->mp_regen_interval == 0) {
            player_adjust_mp(1);
        }
    }
    if (player_state.equipped_head_armor_id != KF_OBJECT_NONE) {
        player_apply_armor_periodic_effects(*player_state.equipped_head_armor_record);
    }
    if (player_state.equipped_body_armor_id != KF_OBJECT_NONE) {
        player_apply_armor_periodic_effects(*player_state.equipped_body_armor_record);
    }
    if (player_state.equipped_shield_id != KF_OBJECT_NONE) {
        player_apply_armor_periodic_effects(*player_state.equipped_shield_record);
    }
    if (player_state.equipped_arm_armor_id != KF_OBJECT_NONE) {
        player_apply_armor_periodic_effects(*player_state.equipped_arm_armor_record);
    }
    if (player_state.equipped_leg_armor_id != KF_OBJECT_NONE) {
        player_apply_armor_periodic_effects(*player_state.equipped_leg_armor_record);
    }
    player_state.equipment_effect_ticks++;
}

static void player_apply_current_floor_hazard()
{
    KfMapAttribute attribute;

    attribute = player_current_map_attribute();
    switch (attribute) {
    default:
        // Only these two attributes apply contact damage here.
        break;
    case KF_MAP_ATTRIBUTE_PITFALL:
        if (player_state.update_state == KF_PLAYER_UPDATE_NORMAL) {
            player_apply_damage(PLAYER_PITFALL_CUTTING_DAMAGE, PLAYER_PITFALL_STRIKING_DAMAGE, PLAYER_PITFALL_PIERCING_DAMAGE, KF_PLAYER_STATUS_NONE, 0, 0, KF_FIXED12_ONE, KF_PLAYER_DAMAGE_MULTIPLIER_ONE);
        }
        break;
    case KF_MAP_ATTRIBUTE_POISON_HOLE:
        player_apply_damage(0, 0, 0, KF_PLAYER_STATUS_POISON, 0, 0, KF_FIXED12_ONE, KF_PLAYER_DAMAGE_MULTIPLIER_ONE);
        break;
    }
}

static void player_update_status_effects()
{
    u16 phase;

    if (player_state.slowed_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
        do {
            if (!((player_state.status_effect_flags & KF_PLAYER_STATUS_SLOWED) != KF_PLAYER_STATUS_NONE)) {
                player_state.slowed_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
            } else {
                player_state.slowed_timer--;
                if (player_state.slowed_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
                    break;
                }
            }
            player_state.status_effect_flags &= ~KF_PLAYER_STATUS_SLOWED;
        } while (0);
    }
    if (player_state.poison_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
        if (!((player_state.status_effect_flags & KF_PLAYER_STATUS_POISON) != KF_PLAYER_STATUS_NONE)) {
            player_state.poison_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
        } else {
            player_state.poison_timer--;
        }
        if (player_state.poison_timer == KF_PLAYER_STATUS_TIMER_INACTIVE) {
            player_state.status_effect_flags &= ~KF_PLAYER_STATUS_POISON;
        } else {
            phase = player_state.poison_timer % POISON_DAMAGE_INTERVAL_UPDATES;
            if (phase < POISON_FLASH_UPDATES) {
                lighting_set_active_color_matrix(KF_GAME_COLOR_DAMAGE);
                if (phase == 0) {
                    player_adjust_hp(-1);
                }
            }
        }
    }
    if (player_state.curse_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
        if (!((player_state.status_effect_flags & KF_PLAYER_STATUS_CURSE) != KF_PLAYER_STATUS_NONE)) {
            player_state.curse_timer = 0;
        }
        if (player_state.curse_timer == 0) {
            player_state.status_effect_flags &= ~KF_PLAYER_STATUS_CURSE;
            player_recalculate_combat_stats();
        } else if (player_state.curse_timer == KF_CURSE_DURATION_UPDATES) {
            player_recalculate_combat_stats();
        }
        player_state.curse_timer--;
    }
    if (player_state.fire_defense_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
        if (player_state.fire_defense_timer == 0) {
            player_state.status_effect_flags &= ~KF_PLAYER_STATUS_FIRE_DEFENSE_BOOST;
            player_recalculate_combat_stats();
        } else if (player_state.fire_defense_timer == KF_FIRE_DEFENSE_DURATION_UPDATES) {
            player_recalculate_combat_stats();
        }
        lighting_set_active_color_matrix(KF_GAME_COLOR_DEFENSE_EFFECT);
        player_state.fire_defense_timer--;
    }
    if (player_state.equipped_weapon_id == KF_ITEM_SHADOW_BLADE) {
        lighting_apply_shadow_blade_environment();
    }
    if (player_state.illusion_staff_timer != KF_ILLUSION_STAFF_INACTIVE) {
        player_state.illusion_staff_timer--;
        lighting_apply_illusion_staff_effect();
    }
}

static void player_restore_loaded_game()
{
    animation_cache_release_all();
    audio_close_vab(audio_state);
    map_load_floor_wrapper();
    player_sync_position_to_map();
    player_state.previous_map_cell.x = player_state.motion_state.map_cell.x;
    player_state.previous_map_cell.z = player_state.motion_state.map_cell.z;
    player_equip_weapon(player_state.equipped_weapon_id);
    player_select_magic(player_state.selected_magic_id);
}

// PC extension: F2 quick save / F4 quick load through the game's own save
// record, applied between updates the way the menu's Load returns.
static void player_quick_save()
{
    if (player_state.update_state != KF_PLAYER_UPDATE_NORMAL) {
        kf::host_show_message("Cannot quick save right now");
        return;
    }
    map_world_state_persist();
    kf::host_show_message(save_system_quick_write() ? "Quick saved" : "Quick save failed");
}

static void player_quick_load()
{
    auto &motion = player_state.motion_state;

    if (player_state.update_state == KF_PLAYER_UPDATE_RECOVERY_FADE) {
        return;
    }
    const bool dying = player_state.update_state == KF_PLAYER_UPDATE_DYING;
    collision_adjust_cell_occupancy(motion.map_cell.x, motion.map_cell.z, -1);
    if (!save_system_quick_read()) {
        collision_adjust_cell_occupancy(motion.map_cell.x, motion.map_cell.z, 1);
        kf::host_show_message("No quick save to load (F2 saves)");
        return;
    }
    if (dying) {
        // Undo the death fade's colour, fog and HUD dimming.
        player_death_apply_visual_fade(&player_death_saved_color_matrix, 0);
    }
    player_restore_loaded_game();
    player_clear_motion();
    collision_adjust_cell_occupancy(motion.map_cell.x, motion.map_cell.z, 1);
    kf::host_show_message("Quick loaded");
}

void player_update(void)
{
    auto &motion = player_state.motion_state;
    u32 input;

    input = kf::host_read_buttons();
    const auto look = kf::host_take_look();
    // Classic mode has no quick save or load (the request is still taken).
    const auto quick = kf::host_take_quick_request();
    switch (kf::classic_mode ? kf::QuickRequest::None : quick) {
    case kf::QuickRequest::Save:
        player_quick_save();
        break;
    case kf::QuickRequest::Load:
        player_quick_load();
        break;
    case kf::QuickRequest::None:
        break;
    }
    if (player_state.update_state == KF_PLAYER_UPDATE_DYING) {
        player_death_update();
        return;
    }
    if (player_state.update_state == KF_PLAYER_UPDATE_RECOVERY_FADE) {
        player_death_update_reverse_fade();
        return;
    }
    collision_adjust_cell_occupancy(motion.map_cell.x, motion.map_cell.z, -1);
    player_state.camera_rotation.vy =
        (player_state.camera_rotation.vy + look.yaw) & KF_ANGLE_WRAP_MASK;
    player_state.camera_rotation.vx = std::clamp<s32>(
        player_state.camera_rotation.vx + look.pitch,
        -kf::player_pitch_limit, kf::player_pitch_limit);
    if (input & kf::Button::Select) {
        display_show_system_screen(KF_SYSTEM_SCREEN_PAUSE);
    }
    if (input & kf::Button::Start) {
        input = kf::button_mask(kf::Button::Back);
    }
    if (kf::button_pressed(input, player_previous_input, kf::Button::Back)
        && player_state.weapon_attack_phase == KF_WEAPON_ATTACK_INACTIVE) {
        const auto outcome = menu_open_root();
        if (const auto *item = std::get_if<KfObjectId>(&outcome)) {
            player_use_item(*item);
        } else {
            switch (std::get<KfMenuAction>(outcome)) {
            case KfMenuAction::GameLoaded:
                player_restore_loaded_game();
                break;
            case KfMenuAction::ReturnToIntro:
                game_result = GameResult::ReturnToIntro;
                return;
            case KfMenuAction::Close:
                break;
            }
        }
        player_previous_input = input;
    } else {
        if (kf::button_pressed(input, player_previous_input, kf::Button::Confirm)) {
            map_interaction_dispatch(&player_state.camera_position, &player_state.camera_rotation);
        }
        player_update_movement_input(input);
        player_update_view_input(input);
        if (kf::button_pressed(input, player_previous_input, kf::Button::Attack)) {
            player_begin_weapon_attack();
        }
        player_handle_magic_input(input);
        player_update_weapon_magic();
        player_previous_input = input;
        player_update_vertical_motion();
    }
    collision_adjust_cell_occupancy(motion.map_cell.x, motion.map_cell.z, 1);
    player_update_weapon_attack();
    lighting_set_active_color_matrix(KF_GAME_COLOR_DEFAULT);
    player_update_darkness();
    player_update_damage_reaction();
    player_update_equipment_effects();
    player_apply_current_floor_hazard();
    player_update_status_effects();
}

void player_update_reset_module_state(void)
{
    kf::restore_initial_value<player_darkness_color_matrix>();
    kf::restore_initial_value<player_damage_view_rotation_offsets>();
    kf::restore_initial_value<player_previous_input>();
    kf::restore_initial_value<player_movement_velocity_limit>();
    kf::restore_initial_value<player_turn_step_limit>();
}
