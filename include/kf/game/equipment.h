#ifndef KF_GAME_EQUIPMENT_H
#define KF_GAME_EQUIPMENT_H

#include <kf/game/combat.h>
#include <kf/lib/enum.h>
#include <kf/lib/equipment_types.h>
#include <kf/lib/item.h>
#include <kf/lib/math.h>
#include <kf/lib/types.h>

typedef struct KfArmorRecord {
    u16 unknown_00;
    u16 cutting_defense;
    u16 striking_defense;
    u16 piercing_defense;
    u16 poison_resistance;
    u16 magic_defense;
    u16 fire_defense;
    u16 hp_regen_interval;
    u16 hp_drain_interval;
    u8 unknown_12[0x0a];
} KfArmorRecord;

typedef struct KfWeaponRecord {
    u8 unknown_00;
    u8 charge_rate;
    u16 attack_components[KF_COMBAT_COMPONENT_COUNT];
    u16 hp_regen_interval;
    u16 mp_regen_interval;
    u16 projection_distance;
    u16 attack_z_offset;
    u8 unknown_14[0x08];
    struct KfVec3s render_translation;
    u16 unknown_22;
    SVECTOR render_rotation;
} KfWeaponRecord;

typedef struct KfWeaponTable {
    KfWeaponRecord entries[KF_WEAPON_RECORD_COUNT];
} KfWeaponTable;

typedef struct KfArmorTable {
    KfArmorRecord entries[KF_ARMOR_RECORD_COUNT];
} KfArmorTable;

extern KfWeaponTable weapon_records;
extern KfArmorTable armor_records;

extern void weapon_records_load_and_mirror_angles(
    const KfWeaponTable *source);
extern void armor_records_load(const KfArmorTable *source);

#endif // KF_GAME_EQUIPMENT_H
