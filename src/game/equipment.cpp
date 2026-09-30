#include <kf/platform/prelude.h>
#include <kf/game/equipment.h>
#include <kf/lib/math.h>

KfWeaponTable weapon_records;

KfArmorTable armor_records;

void weapon_records_load_and_mirror_angles(const KfWeaponTable *source)
{
    weapon_records = *source;
    for (auto &record : weapon_records.entries) {
        record.render_rotation.vy = -record.render_rotation.vy;
    }
}

void armor_records_load(const KfArmorTable *source)
{
    armor_records = *source;
}

s32 fixed6_ratio_step(s32 value, s32 span)
{
    return (value << KF_FIXED6_BITS) / (span + 1) + 1;
}


void equipment_reset_module_state(void)
{
    kf::restore_initial_value<weapon_records>();
    kf::restore_initial_value<armor_records>();
}
