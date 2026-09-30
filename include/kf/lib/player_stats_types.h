#ifndef KF_LIB_PLAYER_STATS_TYPES_H
#define KF_LIB_PLAYER_STATS_TYPES_H

#include <kf/lib/types.h>

enum class KfWeaponAttackCharge : u8 {
    KF_WEAPON_ATTACK_NORMAL_CHARGE = 0,
    KF_WEAPON_ATTACK_FULL_CHARGE = 1
}; using enum KfWeaponAttackCharge;

enum {
    KF_PLAYER_CHARGE_FULL = 5000,
    KF_PLAYER_CHARGE_GAIN_MULTIPLIER = 2,
    KF_WEAPON_ATTACK_INACTIVE = -1,
    KF_WEAPON_ATTACK_PHASE_STEP = 300,
    KF_WEAPON_ATTACK_PHASE_END = 4096,
    KF_PLAYER_POWER_MAX = 999,
    KF_PLAYER_VITAL_MAX = 9999,
    KF_PLAYER_EXPERIENCE_MAX = 99999,
    KF_PLAYER_LEVEL_MAX = 255,
    KF_PLAYER_LEVEL_GROWTH_COUNT = 40,
    KF_PLAYER_TRAINING_POINTS_PER_GAIN = 100
};

typedef struct KfPlayerLevelGrowth {
    u16 maximum_hp;
    u16 maximum_mp;
    u16 physical_power_step;
    u16 magic_step;
    u32 experience_threshold;
} KfPlayerLevelGrowth;

typedef struct KfPlayerVitals {
    u16 maximum_hp;
    u16 current_hp;
    u16 maximum_mp;
    u16 current_mp;
} KfPlayerVitals;

typedef struct KfPlayerAttackChargeState {
    u16 current;
    u16 committed;
} KfPlayerAttackChargeState;

#endif // KF_LIB_PLAYER_STATS_TYPES_H
