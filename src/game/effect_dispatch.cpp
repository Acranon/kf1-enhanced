#include <kf/platform/prelude.h>
#include <kf/game/collision.h>
#include <kf/game/effect.h>
#include <kf/game/game.h>
#include <kf/lib/map_data.h>
#include <kf/lib/null.h>
#include <kf/lib/random.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum {
    EFFECT_ORBIT_RADIUS = 6500
};

enum {
    PROJECTILE_COLLISION_RADIUS = 100,
    EMERGING_COLLISION_RADIUS = 150,
    PROJECTILE_ROLL_STEP = 200,
    EMERGING_ROLL_STEP = 100,
    PROJECTILE_DEFAULT_ROLL_STEP = 600,
    EFFECT_DISSIPATE_SCALE_STEP = 400,
    EMERGING_FALL_ACCELERATION = 20,
    EMERGING_RETIRE_DEPTH = 3000,
    STATUS_PROJECTILE_SHRINK_STEP = 384,
    SCATTER_FINAL_COUNTDOWN = 15,
    SCATTER_BRANCH_COUNTDOWN = 5,
    SCATTER_PULSE_ANGLE_SHIFT = 9,
    MAP_EMITTER_SOUND_RANDOM_CUTOFF = (kf::random_max + 1) / 4,
    MAP_EMITTER_SOUND_MAX_DISTANCE = 4000,
    MAP_EMITTER_SOUND_ATTENUATION_DISTANCE = 12000
};

enum {
    MOONLIGHT_LENGTH_STEP = KF_FIXED12_ONE / 4,
    MOONLIGHT_LENGTH_MAX = 0x7fff,
    MOONLIGHT_NEAR_TRAIL_ANGLE = 1774,
    MOONLIGHT_FAR_TRAIL_ANGLE = 1824,
    MOONLIGHT_NEAR_TRAIL_DISTANCE = 4000,
    MOONLIGHT_FAR_TRAIL_DISTANCE = 8000,
    GROUND_TRAIL_RENDER_FRAME_COUNT = 3
};

enum {
    RADIAL_BLAST_SCALE_STEP = KF_FIXED12_ONE / 4,
    RADIAL_BLAST_RADIUS_STEP = 333,
    EFFECT_PLAYER_RADIAL_SCALE_Q12 = 5000
};

enum {
    HOMING_PITCH_RANDOM_SHIFT = 6,
    HOMING_PITCH_RANDOM_BIAS = 128,
    HOMING_YAW_RANDOM_SHIFT = 3,
    HOMING_YAW_RANDOM_BIAS = 512,
    HOMING_WANDER_RANDOM_CUTOFF = 3276,
    HOMING_PLAYER_AIM_Y_OFFSET = 800,
    HOMING_TURN_STEP = KF_ANGLE_FULL_TURN / 64,
    HOMING_FORWARD_STEP = 650,
    HOMING_ROLL_STEP = 256
};

enum {
    LIGHTNING_IMPACT_RENDER_FRAME_COUNT = 3,
    LIGHTNING_BLAST_SCALE_STEP = 3583,
    LIGHTNING_BLAST_YAW_STEP = 1300,
    LIGHTNING_BLAST_RADIUS_STEP = 1000
};

enum {
    GROUND_BRANCH_SCALE_STEP = KF_FIXED12_ONE / 16,
    GROUND_BRANCH_DAMAGE_PERIOD = 4,
    GROUND_BRANCH_DAMAGE_RADIUS = 1500,
    GROUND_EFFECT_YAW_STEP = 500,
    GROUND_VISUAL_RADIUS_RANDOM_SCALE = 325,
    GROUND_VISUAL_RADIUS_RANDOM_SHIFT = 13
};

enum {
    GROUND_VISUAL_SCALE_STEP = 4000,
    GROUND_VISUAL_RISE_STEP = 1800
};

enum {
    ACTOR_SPAWNER_SCALE_STEP = 220,
    ACTOR_SPAWNER_COLLISION_RADIUS = 1000,
    ACTOR_SPAWNER_CREATE_Y_OFFSET = 800,
    ACTOR_SPAWNER_SELECTION_RANDOM_CUTOFF = 3000,
    ACTOR_SPAWNER_YAW_STEP = 1700,
    ACTOR_SPAWNER_PITCH_STEP = 1400,
    ACTOR_SPAWNER_ROLL_STEP = 1900
};

enum {
    SHORT_SWING_PROBE_LENGTH = 2500,
    LONG_SWING_PROBE_LENGTH = 4900
};

std::array<SVECTOR, KF_EFFECT_SWING_PROBE_COUNT> effect_swing_probe_offsets = {
    SVECTOR{0, SHORT_SWING_PROBE_LENGTH, 0, 0},
    SVECTOR{0, LONG_SWING_PROBE_LENGTH, 0, 0},
};

static void effect_begin_lightning_impact(KfEffectRecord *effect, const KfMagicRecord *magic)
{
    VECTOR impact_position;

    audio_play_spatial_default_range(
        &magic->sounds[1], &effect->position, KF_AUDIO_MAX_VOLUME);
    effect->phase = KF_EFFECT_PROJECTILE_DISSIPATE_FIRST;
    impact_position.vx = effect->position.vx;
    impact_position.vz = effect->position.vz;
    impact_position.vy =
        -(map_floor_height_grid.cells[effect->position.vz / KF_MAP_TILE_SIZE]
                               [effect->position.vx / KF_MAP_TILE_SIZE] * KF_MAP_HEIGHT_STEP);
    if (effect->base_render_id.billboard == KF_EFFECT_BILLBOARD_LIGHTNING_BOLT) {
        effect_pool_construct(
            effect->id, effect->type, KF_EFFECT_KIND_LIGHTNING_IMPACT,
            &impact_position, &effect->rotation.vector);
    } else {
        effect_pool_construct(
            effect->id, effect->type, KF_EFFECT_KIND_LIGHTNING_IMPACT_ALTERNATE,
            &impact_position, &effect->rotation.vector);
    }
}

// False leaves the projectile traveling, including Wind Cutter's piercing hits.
static bool effect_handle_projectile_collision(KfEffectRecord *effect, KfMagicRecord *magic,
    KfEffectKind kind, u32 radius)
{
    KfCollisionResult collision;
    KfCollisionKind collision_kind;
    KfMagicRecord *impact_magic;

    collision = effect_map_collision(&effect->position, radius);
    if (collision.kind != KfCollisionKind::None) {
        u16 impact_power;

        impact_magic = effect_state.current_magic;
        collision_kind = collision.kind;
        if (kind == KF_MAGIC_LIGHTNING_BOLT) {
            effect_begin_lightning_impact(effect, magic);
            return true;
        }
        impact_power = effect_magic_power(effect);
        if (kind != KF_EFFECT_KIND_SCATTER_PROJECTILE) {
            audio_play_spatial_default_range(
                &impact_magic->sounds[1], &effect->position, KF_AUDIO_MAX_VOLUME);
        }
        if (collision_kind == KfCollisionKind::Actor) {
            if (kind == KF_EFFECT_KIND_MAP_EMITTER_PROJECTILE || kind == KF_EFFECT_KIND_PHYSICAL_PROJECTILE || kind == KF_MAGIC_WIND_CUTTER) {
                actor_apply_damage(
                    collision.detail, impact_power,
                    impact_magic->damage_components[0],
                    impact_magic->damage_components[2],
                    impact_magic->damage_components[1],
                    0, 0, KF_ACTOR_DAMAGE_SCALE_ONE, effect->type);
            } else if (kind != KF_EFFECT_KIND_EMERGING_PROJECTILE) {
                actor_apply_damage(
                    collision.detail, impact_power,
                    0, 0, 0, impact_magic->damage_components[0],
                    impact_magic->damage_components[1],
                    KF_ACTOR_DAMAGE_SCALE_ONE, effect->type);
            }
            if (kind == KF_MAGIC_WIND_CUTTER) {
                return false;
            }
        } else if (collision_kind == KfCollisionKind::Player) {
            if (kind == KF_EFFECT_KIND_EMERGING_PROJECTILE || kind == KF_EFFECT_KIND_MAP_EMITTER_PROJECTILE || kind == KF_EFFECT_KIND_PHYSICAL_PROJECTILE) {
                player_apply_damage(
                    impact_magic->damage_components[0],
                    impact_magic->damage_components[2],
                    impact_magic->damage_components[1],
                    KF_PLAYER_STATUS_NONE, 0, 0, KF_FIXED12_ONE, effect->id);
            } else if (kind == KF_EFFECT_KIND_DARKNESS_PROJECTILE) {
                player_apply_damage(
                    0, 0, 0, KF_PLAYER_STATUS_DARKNESS, 0, 0, KF_FIXED12_ONE, effect->id);
            } else if (kind == KF_EFFECT_KIND_SCATTER_PROJECTILE) {
                player_apply_damage(
                    0, 0, 0, KF_PLAYER_STATUS_POISON, 0, 0, KF_FIXED12_ONE, effect->id);
            } else if (kind == KF_EFFECT_KIND_CURSE_PROJECTILE) {
                player_apply_damage(0, 0, 0, KF_PLAYER_STATUS_CURSE, 0, 0, KF_FIXED12_ONE, effect->id);
            } else {
                player_apply_damage(
                    0, 0, 0, KF_PLAYER_STATUS_NONE,
                    impact_magic->damage_components[0],
                    impact_magic->damage_components[1],
                    KF_FIXED12_ONE, effect->id);
            }
            if (kind == KF_MAGIC_WIND_CUTTER) {
                return false;
            }
        }

        if (kind == KF_EFFECT_KIND_EMERGING_PROJECTILE) {
            effect->direction.words.y = 0;
            effect->phase = KF_EFFECT_PROJECTILE_FALL;
        } else if (kind == KF_EFFECT_KIND_DARKNESS_PROJECTILE
                   || kind == KF_EFFECT_KIND_CURSE_PROJECTILE) {
            effect->phase = KF_EFFECT_PROJECTILE_SHRINK;
        } else {
            effect->phase = KF_EFFECT_PROJECTILE_IMPACT_FIRST;
        }
        return true;
    }
    return false;
}

static void effect_randomize_homing_direction(KfEffectRecord *effect)
{
    effect->direction.words.x =
        (effect->direction.words.x + (kf::random_next() >> HOMING_PITCH_RANDOM_SHIFT) - HOMING_PITCH_RANDOM_BIAS) & KF_ANGLE_WRAP_MASK;
    effect->direction.words.y =
        (effect->direction.words.y + (kf::random_next() >> HOMING_YAW_RANDOM_SHIFT) - HOMING_YAW_RANDOM_BIAS) & KF_ANGLE_WRAP_MASK;
}

static void effect_update_homing_direction(KfEffectRecord *effect, KfEffectPhase phase)
{
    KfActor *target;
    s16 desired_pitch;

    if (phase == KF_EFFECT_PHASE_INIT) {
        effect_randomize_homing_direction(effect);
    } else if (phase > KF_EFFECT_HOMING_INITIAL_PHASE_LAST) {
        if (effect->control.target_mode == KF_EFFECT_HOMING_WANDER) {
            if (kf::random_next() < HOMING_WANDER_RANDOM_CUTOFF) {
                effect_randomize_homing_direction(effect);
                // A wandering turn skips the tracking-phase reset, but not movement.
                return;
            }
        } else if (effect->control.target_mode == KF_EFFECT_HOMING_PLAYER) {
            s32 aim_height;
            // Pitch uses horizontal distance; the height difference is separate.
            const s32 target_distance = fixed_vector2_length(
                player_state.camera_position.vx - effect->position.vx,
                player_state.camera_position.vz - effect->position.vz);

            effect->direction.words.y = vector_xz_to_angle(
                player_state.camera_position.vx - effect->position.vx,
                effect->position.vz - player_state.camera_position.vz);
            aim_height = effect->position.vy - HOMING_PLAYER_AIM_Y_OFFSET;
            desired_pitch = vector_xz_to_angle(
                aim_height - player_state.camera_position.vy,
                -target_distance);
            effect->direction.words.x = -desired_pitch & KF_ANGLE_WRAP_MASK;
        } else {
            s32 target_distance;
            target = actor_pool_find_target_in_cone(
                &effect->position,
                effect->rotation.vector.vy, KF_EFFECT_ACTOR_TARGET_MAX_DISTANCE, KF_EFFECT_ACTOR_TARGET_WIDE_CONE, &target_distance);
            if (target != NULL) {
                KfActorDefinition *definition =
                    &actor_state.definitions.entries[target->definition_id];

                effect->direction.words.y = vector_xz_to_angle(
                    target->position.vx - effect->position.vx,
                    effect->position.vz - target->position.vz);
                desired_pitch = vector_xz_to_angle(
                    effect->position.vy
                        - (target->position.vy
                           - (definition->collision_height >> 1)),
                    -target_distance);
                effect->direction.words.x = -desired_pitch & KF_ANGLE_WRAP_MASK;
            } else {
                effect->direction.words.x = 0;
            }
        }
        effect->phase = KF_EFFECT_HOMING_TRACKING_PHASE;
    }
}

void effect_update_dispatch(void)
{
    KfEffectRecord *effect = effect_state.current_record;
    KfMagicRecord *magic = effect_state.current_magic;
    KfEffectPhase phase;
    KfEffectKind kind;
    u32 radius;
    u16 angle;
    u16 distance;
    u16 power;
    u16 next;
    s32 value;

    kind = effect->kind;
    phase = effect->phase;
    radius = PROJECTILE_COLLISION_RADIUS;

    switch (kind) {
    case KF_EFFECT_KIND_EMERGING_PROJECTILE:
        radius = EMERGING_COLLISION_RADIUS;
    case KF_MAGIC_LIGHTNING_BOLT:
    case KF_MAGIC_FIRE_BALL:
    case KF_MAGIC_WIND_CUTTER:
    case KF_MAGIC_LIGHT_NEEDLE:
    case KF_EFFECT_KIND_SCATTER_PROJECTILE:
    case KF_EFFECT_KIND_DARKNESS_PROJECTILE:
    case KF_EFFECT_KIND_CURSE_PROJECTILE:
    case KF_EFFECT_KIND_MAP_EMITTER_PROJECTILE:
    case KF_EFFECT_KIND_PHYSICAL_PROJECTILE:
        if (phase == KF_EFFECT_PROJECTILE_TRAVEL) {
            if (effect_handle_projectile_collision(effect, magic, kind, radius)) {
                return;
            }

            effect->position += effect->direction.vector;
            if (kind == KF_MAGIC_FIRE_BALL || kind == KF_EFFECT_KIND_DARKNESS_PROJECTILE) {
                effect->rotation.vector.vz = (effect->rotation.vector.vz + PROJECTILE_ROLL_STEP) & KF_ANGLE_WRAP_MASK;
                return;
            }
            if (kind == KF_MAGIC_LIGHTNING_BOLT) {
                s32 remaining;

                effect->rotation.vector.vz = (effect->rotation.vector.vz + PROJECTILE_ROLL_STEP) & KF_ANGLE_WRAP_MASK;
                remaining = effect->control.frames_remaining - 1;
                effect->control.frames_remaining = remaining;
                if ((u16)remaining == 0) {
                    effect_begin_lightning_impact(effect, magic);
                } else {
                    effect->render_id.billboard = kf_enum_decode<KfEffectBillboardId>(kf_enum_encode<u8>(effect->base_render_id.billboard) + (effect->control.frames_remaining & 1));
                }
                return;
            }
            if (kind == KF_EFFECT_KIND_SCATTER_PROJECTILE) {
                KfEffectDirection scatter;
                s32 pulse_angle;

                if (--effect->control.frames_remaining == 0) {
                    if (effect->propagation.generations_remaining != 0) {
                        next = ((s16)effect->scale_x * 3) >> 2;
                        effect->visual.pulse_base_scale = next;
                        effect->scale_z = next;
                        effect->scale_y = next;
                        effect->scale_x = next;
                        effect->propagation.generations_remaining--;
                        scatter.vector = effect->direction.vector;
                        effect_scatter_triple(&scatter.words);
                        if (effect->propagation.generations_remaining == 0) {
                            effect->control.frames_remaining = SCATTER_FINAL_COUNTDOWN;
                        } else {
                            effect->control.frames_remaining = SCATTER_BRANCH_COUNTDOWN;
                        }
                        effect_pool_construct(
                            effect->id, effect->type, kind, &effect->position, &scatter.vector,
                            KfEffectScatterArguments{effect->propagation.generations_remaining, effect->control.frames_remaining, (s16)effect->scale_x});
                        effect_scatter_triple(&effect->direction.words);
                    } else {
                        effect->type = KF_EFFECT_SLOT_FREE;
                    }
                }
                pulse_angle = effect->control.frames_remaining << SCATTER_PULSE_ANGLE_SHIFT;
                value = kf::angle_sine(pulse_angle);
                value = (effect->visual.pulse_base_scale * value) >> (KF_FIXED12_BITS + 1);
                next = effect->visual.pulse_base_scale + value;
                effect->scale_z = next;
                effect->scale_x = next;
                value = kf::angle_cosine(pulse_angle);
                effect->scale_y = effect->visual.pulse_base_scale
                    + ((effect->visual.pulse_base_scale * value) >> (KF_FIXED12_BITS + 1));
                return;
            }
            if (kind == KF_EFFECT_KIND_EMERGING_PROJECTILE) {
                effect->rotation.vector.vz = (effect->rotation.vector.vz + EMERGING_ROLL_STEP) & KF_ANGLE_WRAP_MASK;
                return;
            }
            if (kind == KF_EFFECT_KIND_MAP_EMITTER_PROJECTILE) {
                if (kf::random_next() < MAP_EMITTER_SOUND_RANDOM_CUTOFF && effect->sound_played == KF_AUDIO_NOT_PLAYED) {
                    effect->sound_played = audio_play_spatial_range(
                        &magic->sounds[0], &effect->position, KF_AUDIO_MAX_VOLUME,
                        MAP_EMITTER_SOUND_MAX_DISTANCE, MAP_EMITTER_SOUND_ATTENUATION_DISTANCE);
                }
                return;
            }
            if (kind == KF_MAGIC_LIGHT_NEEDLE || kind == KF_EFFECT_KIND_PHYSICAL_PROJECTILE) {
                return;
            }
            effect->rotation.vector.vz = (effect->rotation.vector.vz + PROJECTILE_DEFAULT_ROLL_STEP) & KF_ANGLE_WRAP_MASK;
            return;
        }

        if (phase < KF_EFFECT_FIRE_BALL_IMPACT_END && kind == KF_MAGIC_FIRE_BALL) {
            effect->render_id.billboard = kf_enum_decode<KfEffectBillboardId>(kf_enum_encode<u8>(effect->base_render_id.billboard) + kf_enum_encode<u8>(phase));
        } else if (phase < KF_EFFECT_PROJECTILE_IMPACT_END) {
            effect->type = KF_EFFECT_SLOT_FREE;
        } else if (phase < KF_EFFECT_PROJECTILE_DISSIPATE_END) {
            s32 scale = effect->scale_x - EFFECT_DISSIPATE_SCALE_STEP;

            effect->scale_x = scale;
            effect->scale_z = scale;
            effect->scale_y = scale;
        } else if (phase == KF_EFFECT_PROJECTILE_DISSIPATE_END) {
            effect->type = KF_EFFECT_SLOT_FREE;
        } else if (phase < KF_EFFECT_PROJECTILE_FALL) {
            effect->position.vy -= KF_EFFECT_EMERGE_Y_STEP;
            if (phase == KF_EFFECT_PROJECTILE_EMERGE_LAST) {
                effect->phase = KF_EFFECT_PROJECTILE_LAUNCH_WRAP;
                audio_play_spatial_default_range(
                    &magic->sounds[0], &effect->position, KF_AUDIO_MAX_VOLUME);
            }
        } else if (phase == KF_EFFECT_PROJECTILE_FALL) {
            effect->direction.words.y += EMERGING_FALL_ACCELERATION;
            effect->position.vy += effect->direction.vector.vy;
            if (effect->position.vy
                > -(map_floor_height_grid.cells[effect->position.vz / KF_MAP_TILE_SIZE]
                                        [effect->position.vx / KF_MAP_TILE_SIZE] * KF_MAP_HEIGHT_STEP)
                    + EMERGING_RETIRE_DEPTH) {
                effect->type = KF_EFFECT_SLOT_FREE;
            }
            return;
        } else if (phase == KF_EFFECT_PROJECTILE_SHRINK) {
            s32 scale = effect->scale_y - STATUS_PROJECTILE_SHRINK_STEP;

            effect->scale_y = scale;
            effect->scale_z = scale;
            if ((s16)effect->scale_y < 0) {
                effect->type = KF_EFFECT_SLOT_FREE;
            }
            return;
        } else {
            effect->type = KF_EFFECT_SLOT_FREE;
        }
        effect->phase++;
        break;

    case KF_EFFECT_KIND_MOONLIGHT_PROJECTILE:
        if (kf_enum_encode<u8>(phase) < kf_enum_encode<u8>(KF_EFFECT_MOONLIGHT_TRAVEL_LAST) + 1) {
            if (effect_map_collision(&effect->position, PROJECTILE_COLLISION_RADIUS).kind != KfCollisionKind::None) {
                effect->animation_clip = KF_ANIMATION_CLIP_NONE;
                effect->base_render_id.model = KF_EFFECT_MODEL_NONE;
                effect->render_id.model = KF_EFFECT_MODEL_NONE;
                effect->phase = KF_EFFECT_MOONLIGHT_IMPACT_FIRST;
                audio_play_spatial_default_range(
                    &effect_state.magic.entries[kf_enum_encode<u8>(KF_EFFECT_KIND_RADIAL_BLAST)].sounds[1], &effect->position, KF_AUDIO_MAX_VOLUME);
                return;
            }
            effect->position += effect->direction.vector;
            effect->scale_z += MOONLIGHT_LENGTH_STEP;
            if ((s16)effect->scale_z < 0) {
                effect->scale_z = MOONLIGHT_LENGTH_MAX;
            }
            if (phase == KF_EFFECT_MOONLIGHT_TRAIL_EMIT_PHASE) {
                effect_spawn_ground_trail(effect->id, effect, MOONLIGHT_NEAR_TRAIL_ANGLE, MOONLIGHT_NEAR_TRAIL_DISTANCE);
                effect_spawn_ground_trail(effect->id, effect, -MOONLIGHT_NEAR_TRAIL_ANGLE, MOONLIGHT_NEAR_TRAIL_DISTANCE);
                effect_spawn_ground_trail(effect->id, effect, MOONLIGHT_FAR_TRAIL_ANGLE, MOONLIGHT_FAR_TRAIL_DISTANCE);
                effect_spawn_ground_trail(effect->id, effect, -MOONLIGHT_FAR_TRAIL_ANGLE, MOONLIGHT_FAR_TRAIL_DISTANCE);
            }
            if (phase == KF_EFFECT_MOONLIGHT_TRAVEL_LAST) {
                return;
            }
        } else {
            if (((kf_enum_encode<u8>(phase) - kf_enum_encode<u8>(KF_EFFECT_MOONLIGHT_IMPACT_FIRST)) & 1) == 0) {
                effect_pool_construct(
                    effect->id, effect->type, KF_EFFECT_KIND_RADIAL_BLAST,
                    &effect->position, &effect->direction.vector, KfEffectSoundArguments{KF_EFFECT_SOUND_PLAY});
            }
            if (phase > KF_EFFECT_MOONLIGHT_IMPACT_LAST) {
                effect->type = KF_EFFECT_SLOT_FREE;
                return;
            }
        }
        effect->phase++;
        break;

    case KF_EFFECT_KIND_GROUND_TRAIL: {
        KfEffectRecord *linked_effect;
        KfCollisionResult collision;
        KfCollisionKind collision_kind;
        u16 power;

        linked_effect = &effect_state.records[effect->control.parent_effect_index];
        collision = effect_map_collision(&effect->position, radius);
        if (collision.kind != KfCollisionKind::None) {
            collision_kind = collision.kind;
            power = effect_magic_power(effect);
            if (collision_kind == KfCollisionKind::Actor) {
                actor_apply_damage(
                    collision.detail, power, 0, 0, 0,
                    magic->damage_components[0], magic->damage_components[1],
                    KF_ACTOR_DAMAGE_SCALE_ONE, effect->type);
            }
        }
        effect->position += effect->direction.vector;
        effect->position.vy =
            -(map_floor_height_grid.cells[effect->position.vz / KF_MAP_TILE_SIZE]
                                   [effect->position.vx / KF_MAP_TILE_SIZE] * KF_MAP_HEIGHT_STEP);
        switch (phase) {
        default:
            // Other phase values have no ground-trail transition.
            break;
        case KF_EFFECT_GROUND_TRAIL_WAIT_FOR_PARENT:
            if (kf_enum_encode<u8>(linked_effect->phase) > kf_enum_encode<u8>(KF_EFFECT_MOONLIGHT_IMPACT_FIRST) - 1) {
                effect->phase = KF_EFFECT_GROUND_TRAIL_SHRINK;
            }
            break;
        case KF_EFFECT_GROUND_TRAIL_SHRINK: {
            s32 scale = effect->scale_x - EFFECT_DISSIPATE_SCALE_STEP;

            effect->scale_x = scale;
            effect->scale_y = effect->scale_z = scale;
            if ((s16)scale <= 0) {
                effect->type = KF_EFFECT_SLOT_FREE;
            }
            break;
        }
        }
        effect->render_id.billboard++;
        if (kf_enum_encode<u8>(effect->render_id.billboard) >= kf_enum_encode<u8>(effect->base_render_id.billboard) + GROUND_TRAIL_RENDER_FRAME_COUNT) {
            effect->render_id.billboard = effect->base_render_id.billboard;
        }
        break;
    }

    case KF_EFFECT_KIND_RADIAL_BLAST:
        if (++effect->phase < KF_EFFECT_RADIAL_BLAST_PHASE_END) {
            u32 damage_radius;

            effect->scale_y = effect->scale_z =
                effect->scale_x += RADIAL_BLAST_SCALE_STEP;
            damage_radius = kf_enum_encode<u8>(phase) * RADIAL_BLAST_RADIUS_STEP;
            power = effect_magic_power(effect);
            if (kf_enum_encode<u8>(effect->phase) & 1) {
                actor_pool_apply_radial_damage(
                    &effect->position,
                    damage_radius, KF_FIXED12_ONE, power,
                    0, 0, 0, magic->damage_components[0],
                    magic->damage_components[1], KF_ACTOR_DAMAGE_SCALE_ONE, effect->type);
                player_apply_radial_damage(
                    &effect->position,
                    damage_radius, KF_FIXED12_ONE,
                    0, 0, 0, magic->damage_components[0],
                    magic->damage_components[1], EFFECT_PLAYER_RADIAL_SCALE_Q12, effect->id);
            }
        } else {
            effect->type = KF_EFFECT_SLOT_FREE;
        }
        break;

    case KF_EFFECT_KIND_HOMING_PROJECTILE: {
        SVECTOR local_motion;
        VECTOR movement;
        MATRIX matrix;

        effect_update_homing_direction(effect, phase);

        effect->rotation.vector.vx = angle_approach(
            effect->rotation.vector.vx, effect->direction.vector.vx, HOMING_TURN_STEP);
        effect->rotation.vector.vy = angle_approach(
            effect->rotation.vector.vy, effect->direction.vector.vy, HOMING_TURN_STEP);
        local_motion = {0, 0, HOMING_FORWARD_STEP};
        matrix_set_rotation_x(effect->rotation.vector.vx, &matrix);
        movement = kf::matrix_apply_rotation(matrix, local_motion);
        local_motion = movement.narrowed();
        matrix_set_rotation_y(effect->rotation.vector.vy, &matrix);
        movement = kf::matrix_apply_rotation(matrix, local_motion);
        effect->position += movement;
        effect->phase++;
        effect->rotation.vector.vz = (effect->rotation.vector.vz + HOMING_ROLL_STEP) & KF_ANGLE_WRAP_MASK;
        if (effect_map_collision(&effect->position, radius).kind != KfCollisionKind::None) {
            if (effect->base_render_id.model == KF_EFFECT_MODEL_HOMING_PROJECTILE_ALTERNATE) {
                effect_pool_construct(
                    effect->id, effect->type, KF_EFFECT_KIND_RADIAL_BLAST_ALTERNATE,
                    &effect->position, &effect->direction.vector, KfEffectSoundArguments{KF_EFFECT_SOUND_PLAY});
            } else {
                effect_pool_construct(
                    effect->id, effect->type, KF_EFFECT_KIND_RADIAL_BLAST,
                    &effect->position, &effect->direction.vector, KfEffectSoundArguments{KF_EFFECT_SOUND_PLAY});
            }
            effect->type = KF_EFFECT_SLOT_FREE;
        }
        break;
    }

    case KF_EFFECT_KIND_LIGHTNING_IMPACT:
        if (phase > KF_EFFECT_LIGHTNING_IMPACT_PHASE_LAST) {
            effect->type = KF_EFFECT_SLOT_FREE;
        } else {
            effect->render_id.billboard++;
            if (kf_enum_encode<u8>(effect->render_id.billboard) >= kf_enum_encode<u8>(effect->base_render_id.billboard) + LIGHTNING_IMPACT_RENDER_FRAME_COUNT) {
                effect->render_id.billboard = effect->base_render_id.billboard;
            }
            if (phase == KF_EFFECT_LIGHTNING_IMPACT_EMIT_FIRST || phase == KF_EFFECT_LIGHTNING_IMPACT_EMIT_SECOND || phase == KF_EFFECT_LIGHTNING_IMPACT_EMIT_LAST) {
                if (effect->base_render_id.billboard == KF_EFFECT_BILLBOARD_LIGHTNING_IMPACT) {
                    effect_pool_construct(
                        effect->id, effect->type, KF_EFFECT_KIND_LIGHTNING_RADIAL_BLAST,
                        &effect->position, &effect->rotation.vector);
                } else {
                    effect_pool_construct(
                        effect->id, effect->type, KF_EFFECT_KIND_LIGHTNING_RADIAL_BLAST_ALTERNATE,
                        &effect->position, &effect->rotation.vector);
                }
                if (phase == KF_EFFECT_LIGHTNING_IMPACT_EMIT_FIRST) {
                    audio_play_spatial_default_range(
                        &effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_LIGHTNING_BOLT)].sounds[1],
                        &effect->position, KF_AUDIO_MAX_VOLUME);
                }
            }
        }
        effect->phase++;
        break;

    case KF_EFFECT_KIND_LIGHTNING_RADIAL_BLAST: {
        VECTOR position;

        if (phase > KF_EFFECT_LIGHTNING_BLAST_PHASE_LAST) {
            effect->type = KF_EFFECT_SLOT_FREE;
        } else {
            effect->scale_y = effect->scale_z =
                effect->scale_x += LIGHTNING_BLAST_SCALE_STEP;
            effect->rotation.vector.vy = (effect->rotation.vector.vy + LIGHTNING_BLAST_YAW_STEP) & KF_ANGLE_WRAP_MASK;
            if (kf_enum_encode<u8>(phase) & 1) {
                u32 damage_radius;
                KfMagicRecord *lightning_magic;

                position = {
                    effect->position.vx,
                    KF_COLLISION_IGNORE_HEIGHT,
                    effect->position.vz};
                damage_radius = kf_enum_encode<u8>(phase) * LIGHTNING_BLAST_RADIUS_STEP;
                power = effect_magic_power(effect);
                lightning_magic = &effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_LIGHTNING_BOLT)];
                actor_pool_apply_radial_damage(
                    &position, damage_radius, KF_FIXED12_ONE, power, 0, 0, 0,
                    lightning_magic->damage_components[0],
                    lightning_magic->damage_components[1], KF_ACTOR_DAMAGE_SCALE_ONE, effect->type);
                player_apply_radial_damage(
                    &position, damage_radius, KF_FIXED12_ONE, 0, 0, 0,
                    lightning_magic->damage_components[0],
                    lightning_magic->damage_components[1], EFFECT_PLAYER_RADIAL_SCALE_Q12, effect->id);
            }
        }
        effect->phase++;
        break;
    }

    case KF_MAGIC_FIRE_WALL:
        if (phase < KF_EFFECT_GROUND_BRANCH_GROW_END) {
            if (effect->control.frames_remaining != KF_EFFECT_GROUND_BRANCH_TIMER_DONE) {
                if (effect->control.frames_remaining-- == 0) {
                    switch (effect->propagation.branch) {
                    case KF_EFFECT_GROUND_BRANCH_LEAF:
                        // A leaf emits no further branches.
                        break;
                    case KF_EFFECT_GROUND_BRANCH_ROOT:
                        effect_spawn_ground_branch(
                            effect->id, effect, KF_ANGLE_QUARTER_TURN, KF_EFFECT_GROUND_BRANCH_QUARTER_TURN);
                        effect_spawn_ground_branch(
                            effect->id, effect, KF_ANGLE_THREE_QUARTER_TURN,
                            KF_EFFECT_GROUND_BRANCH_THREE_QUARTER_TURN);
                        break;
                    case KF_EFFECT_GROUND_BRANCH_QUARTER_TURN:
                        effect_spawn_ground_branch(
                            effect->id, effect, KF_ANGLE_QUARTER_TURN, KF_EFFECT_GROUND_BRANCH_LEAF);
                        break;
                    case KF_EFFECT_GROUND_BRANCH_THREE_QUARTER_TURN:
                        effect_spawn_ground_branch(
                            effect->id, effect, KF_ANGLE_THREE_QUARTER_TURN, KF_EFFECT_GROUND_BRANCH_LEAF);
                        break;
                    }
                    effect->control.frames_remaining = KF_EFFECT_GROUND_BRANCH_TIMER_DONE;
                }
            }
            effect->scale_y += GROUND_BRANCH_SCALE_STEP;
            if (kf_enum_encode<u8>(phase) == kf_enum_encode<u8>(KF_EFFECT_GROUND_BRANCH_GROW_END) - 1) {
                switch (effect->propagation.branch) {
                case KF_EFFECT_GROUND_BRANCH_ROOT:
                    effect->phase = KF_EFFECT_GROUND_BRANCH_ROOT_HOLD_BASE;
                    break;
                case KF_EFFECT_GROUND_BRANCH_QUARTER_TURN:
                case KF_EFFECT_GROUND_BRANCH_THREE_QUARTER_TURN:
                    effect->phase = KF_EFFECT_GROUND_BRANCH_SIDE_HOLD_BASE;
                    break;
                case KF_EFFECT_GROUND_BRANCH_LEAF:
                    effect->phase = KF_EFFECT_GROUND_BRANCH_LEAF_HOLD_BASE;
                    break;
                }
            }
        } else if (phase < KF_EFFECT_GROUND_BRANCH_SHRINK_FIRST) {
            if ((kf_enum_encode<u8>(phase) & (GROUND_BRANCH_DAMAGE_PERIOD - 1)) == 0) {
                VECTOR spawn_position;

                angle = (u32)kf::random_next() >> KF_RANDOM_ANGLE_SHIFT;
                distance = ((u32)kf::random_next() * GROUND_VISUAL_RADIUS_RANDOM_SCALE) >> GROUND_VISUAL_RADIUS_RANDOM_SHIFT;
                spawn_position.vx = effect->position.vx
                    + ((kf::angle_sine(angle) * distance) >> KF_FIXED12_BITS);
                spawn_position.vz = effect->position.vz
                    + ((kf::angle_cosine(angle) * distance) >> KF_FIXED12_BITS);
                spawn_position.vy = effect->position.vy;
                effect_pool_construct(
                    effect->id, effect->type, KF_EFFECT_KIND_GROUND_BRANCH_VISUAL,
                    &spawn_position, &effect->rotation.vector);
                power = effect_magic_power(effect);
                actor_pool_apply_radial_damage(
                    &effect->position,
                    GROUND_BRANCH_DAMAGE_RADIUS, KF_FIXED12_ONE, power, 0, 0, 0,
                    magic->damage_components[0],
                    magic->damage_components[1], KF_ACTOR_DAMAGE_SCALE_ONE, effect->type);
                player_apply_radial_damage(
                    &effect->position,
                    GROUND_BRANCH_DAMAGE_RADIUS, KF_FIXED12_ONE, 0, 0, 0,
                    magic->damage_components[0],
                    magic->damage_components[1], EFFECT_PLAYER_RADIAL_SCALE_Q12, effect->id);
            }
        } else if (phase < KF_EFFECT_GROUND_BRANCH_PHASE_END) {
            effect->scale_y -= GROUND_BRANCH_SCALE_STEP;
        } else {
            effect->type = KF_EFFECT_SLOT_FREE;
        }
        effect->rotation.vector.vy = (effect->rotation.vector.vy + GROUND_EFFECT_YAW_STEP) & KF_ANGLE_WRAP_MASK;
        effect->phase++;
        break;

    case KF_EFFECT_KIND_GROUND_BRANCH_VISUAL:
        if (phase < KF_EFFECT_GROUND_VISUAL_SHRINK_FIRST) {
            effect->scale_y += GROUND_VISUAL_SCALE_STEP;
        } else if (phase < KF_EFFECT_GROUND_VISUAL_PHASE_END) {
            effect->scale_y -= GROUND_VISUAL_SCALE_STEP;
            effect->position.vy -= GROUND_VISUAL_RISE_STEP;
        } else {
            effect->type = KF_EFFECT_SLOT_FREE;
        }
        effect->rotation.vector.vy = (effect->rotation.vector.vy + GROUND_EFFECT_YAW_STEP) & KF_ANGLE_WRAP_MASK;
        effect->phase++;
        break;

    case KF_EFFECT_KIND_ACTOR_SPAWNER: {
        s32 scale;
        KfEffectPhase scale_phase;

        if (phase < KF_EFFECT_ACTOR_SPAWNER_TRAVEL_FIRST) {
            scale = effect->scale_x + ACTOR_SPAWNER_SCALE_STEP;
            scale_phase = effect->phase;
            effect->scale_x = scale;
            effect->scale_z = scale;
            effect->scale_y = scale;
            scale_phase++;
            effect->phase = scale_phase;
        } else if (kf_enum_encode<u8>(phase) < kf_enum_encode<u8>(KF_EFFECT_ACTOR_SPAWNER_TRAVEL_LAST) + 1) {
            VECTOR position;
            KfCollisionResult collision;

            position.vx = effect->position.vx + effect->direction.vector.vx;
            position.vz = effect->position.vz + effect->direction.vector.vz;
            position.vy = effect->position.vy;
            collision = collision_query_world(
                position.vx, position.vy, position.vz, ACTOR_SPAWNER_COLLISION_RADIUS, 0,
                KF_COLLISION_SKIP_MAP_OBJECTS | KF_COLLISION_SKIP_MAP_EVENTS);
            if ((phase == KF_EFFECT_ACTOR_SPAWNER_TRAVEL_LAST && collision.kind != KfCollisionKind::None) || effect->control.frames_remaining == 0) {
                effect->phase = KF_EFFECT_ACTOR_SPAWNER_WAIT_FIRST;
            } else {
                effect->position.vx = position.vx;
                effect->position.vz = position.vz;
                effect->control.frames_remaining--;
            }
            if (phase != KF_EFFECT_ACTOR_SPAWNER_TRAVEL_LAST) {
                effect->phase++;
            }
        } else if (phase < KF_EFFECT_ACTOR_SPAWNER_SHRINK_FIRST) {
            if (phase == KF_EFFECT_ACTOR_SPAWNER_CREATE_PHASE) {
                struct KfVec3s actor_rotation;
                VECTOR position;

                position = {
                    effect->position.vx,
                    effect->position.vy + ACTOR_SPAWNER_CREATE_Y_OFFSET,
                    effect->position.vz};
                actor_rotation.x = 0;
                actor_rotation.z = 0;
                actor_rotation.y = vector_xz_to_angle(
                    player_state.camera_position.vx - position.vx,
                    player_state.camera_position.vz - position.vz);
                value = kf::random_next();
                if (value < ACTOR_SPAWNER_SELECTION_RANDOM_CUTOFF) {
                    actor_pool_spawn(2, &position, &actor_rotation);
                } else if (kf::random_next() < ACTOR_SPAWNER_SELECTION_RANDOM_CUTOFF) {
                    actor_pool_spawn(4, &position, &actor_rotation);
                } else {
                    actor_pool_spawn(0, &position, &actor_rotation);
                }
            }
            effect->phase++;
        } else if (phase < KF_EFFECT_ACTOR_SPAWNER_PHASE_END) {
            scale = effect->scale_x - ACTOR_SPAWNER_SCALE_STEP;
            scale_phase = effect->phase;
            effect->scale_x = scale;
            effect->scale_z = scale;
            effect->scale_y = scale;
            scale_phase++;
            effect->phase = scale_phase;
        } else {
            effect->type = KF_EFFECT_SLOT_FREE;
        }
        effect->rotation.vector.vy = (effect->rotation.vector.vy + ACTOR_SPAWNER_YAW_STEP) & KF_ANGLE_WRAP_MASK;
        effect->rotation.vector.vx = (effect->rotation.vector.vx + ACTOR_SPAWNER_PITCH_STEP) & KF_ANGLE_WRAP_MASK;
        effect->rotation.vector.vz = (effect->rotation.vector.vz + ACTOR_SPAWNER_ROLL_STEP) & KF_ANGLE_WRAP_MASK;
        break;
    }

    case KF_EFFECT_KIND_SWINGING_HAZARD_SHORT:
        effect_update_swinging_hazard(&effect_swing_probe_offsets[KF_EFFECT_SWING_PROBE_SHORT], KF_EFFECT_SHORT_SWING_PHASE_LIMIT);
        break;

    case KF_EFFECT_KIND_SWINGING_HAZARD_LONG:
        effect_update_swinging_hazard(&effect_swing_probe_offsets[KF_EFFECT_SWING_PROBE_LONG], KF_EFFECT_LONG_SWING_PHASE_LIMIT);
        break;

    case KF_EFFECT_KIND_FLOOR_DEFORMATION: {
        s16 column;
        s32 remaining;

        switch (phase) {
        default:
            // Only advance, hold and reverse drive floor deformation.
            break;
        case KF_EFFECT_FLOOR_DEFORM_ADVANCE:
            remaining = effect->direction.words.x - 1;
            effect->direction.words.x = remaining;
            if ((s16)remaining == -1) {
                effect->phase = KF_EFFECT_FLOOR_DEFORM_HOLD;
                return;
            }
            effect->direction.words.z += effect->rotation.vector.vz;
            for (column = 0; column < effect->rotation.vector.vy; column++) {
                effect_floor_deform_line(
                    effect->rotation.vector.vx + column,
                    effect->direction.vector.vz,
                    -effect->direction.vector.vy);
            }
            break;
        case KF_EFFECT_FLOOR_DEFORM_HOLD:
            if (--effect->position.vy != -1) {
                return;
            }
            effect->phase = KF_EFFECT_FLOOR_DEFORM_REVERSE;
            effect->direction.words.z = KF_FIXED12_ONE;
            effect->direction.words.x = effect->position.vx;
            break;
        case KF_EFFECT_FLOOR_DEFORM_REVERSE:
            remaining = effect->direction.words.x - 1;
            effect->direction.words.x = remaining;
            if ((s16)remaining == -1) {
                effect->type = KF_EFFECT_SLOT_FREE;
                return;
            }
            effect->direction.words.z -= effect->rotation.vector.vz;
            for (column = 0; column < effect->rotation.vector.vy; column++) {
                effect_floor_deform_line(
                    effect->rotation.vector.vx + column,
                    effect->direction.vector.vz,
                    effect->direction.vector.vy);
            }
            break;
        }
        break;
    }

    case KF_EFFECT_KIND_ORBITING_PROJECTILE:
        effect_update_orbiting_projectile(EFFECT_ORBIT_RADIUS, KF_EFFECT_ORBIT_PHASE_LIMIT);
        break;

    default:
        break;
    }
}


void effect_dispatch_reset_module_state(void)
{
    kf::restore_initial_value<effect_swing_probe_offsets>();
}
