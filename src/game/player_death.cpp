#include <kf/platform/prelude.h>
#include <kf/game/audio.h>
#include <kf/game/game.h>
#include <kf/game/graphics.h>
#include <kf/game/player.h>
#include <kf/lib/null.h>
#include <kf/lib/random.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr s32 PLAYER_REVIVAL_POSITION_X = 64000;
static constexpr s32 PLAYER_REVIVAL_POSITION_Z = 20000;
static constexpr s32 PLAYER_RESTART_POSITION_X = 31000;
static constexpr s32 PLAYER_RESTART_POSITION_Z = 5000;

enum {
    CURSE_PHYSICAL_POWER_PENALTY = 20,
    FIRE_DEFENSE_STATUS_BONUS = 10,
    DISPOISON_REQUIRED_BASE_MAGIC = 37,
    FIRE_WALL_REQUIRED_BASE_MAGIC = 70,
    LIGHTNING_BOLT_REQUIRED_BASE_MAGIC = 75,
    PLAYER_POISON_ROLL_BUCKETS = 100,
    PLAYER_POISON_ROLL_SHIFT = 15
};

enum {
    PLAYER_STARTING_GOLD = 150,
    PLAYER_STARTING_DEFENSE = 5,
    LIGHT_RING_HOLY_ATTACK_BONUS = 5,
    MOON_AMULET_MAGIC_DEFENSE_BONUS = 7,
    WIND_BLADE_BRACELET_FIRE_DEFENSE_BONUS = 7,
    TWO_HEADED_DRAGON_RING_MAGIC_BONUS = 8,
    VERDITE_EQUIPPED_MAGIC_BONUS = 1,
    GOLD_CROSS_HOLY_ATTACK_BONUS = 3,
    BLACK_MASK_PHYSICAL_POWER_PENALTY = 8,
    PLAYER_DAMAGE_POWER_DIVISOR = 5,
    PLAYER_DAMAGE_THRESHOLD_MULTIPLIER = 2
};

std::array<SoundRef, KF_PLAYER_SOUND_COUNT> player_sound_refs = {
    SoundRef{7, 0, 80},
    SoundRef{7, 1, 89},
    SoundRef{13, 0, 67}
};

s32 player_death_saved_fog_near;

MATRIX player_death_saved_color_matrix;

std::array<std::array<u8, KF_ITEM_COUNT>, KF_ITEM_STOCK_BANK_COUNT> item_stock;

void player_death_begin(void)
{
    player_state.update_state = KF_PLAYER_UPDATE_DYING;
    player_state.death_camera_pitch_step = 0;
    player_state.death_visual_blend = 0;
    sound_ref_play(audio_playback(), &player_sound_refs[KF_PLAYER_SOUND_DEATH], KF_AUDIO_MAX_VOLUME);
    player_death_saved_color_matrix = game_graphics_runtime.render_state.lighting.color_matrix;
    player_death_saved_fog_near = game_graphics_runtime.render_state.fog_near_distance;
}

void game_state_initialize(void)
{
    u8 *cursor;
    s32 count;

    kf::automap_reset();
    player_state.experience = 0;
    player_state.progress_state.level = 1;
    player_state.progress_state.current_floor = KF_FLOOR_1;
    player_state.progress_state.highest_floor = KF_FLOOR_1;
    player_state.gold = PLAYER_STARTING_GOLD;
    player_state.attack_charge_state.current = 0;
    player_state.magic_charge = 0;
    player_state.weapon_charge_delay = 0;
    player_state.cutting_defense = PLAYER_STARTING_DEFENSE;
    player_state.striking_defense = PLAYER_STARTING_DEFENSE;
    player_state.piercing_defense = PLAYER_STARTING_DEFENSE;
    player_state.poison_resistance = PLAYER_STARTING_DEFENSE;
    player_state.magic_defense = PLAYER_STARTING_DEFENSE;
    player_state.fire_defense = PLAYER_STARTING_DEFENSE;
    player_state.view_bob_offset = 0;
    player_state.view_bob_phase = 0;
    player_state.vertical_state = KF_PLAYER_VERTICAL_GROUNDED;
    player_state.vertical_velocity = 0;
    player_state.vitals.current_hp = player_level_growth_table[0].maximum_hp;
    player_state.vitals.maximum_hp = player_level_growth_table[0].maximum_hp;
    player_state.vitals.current_mp = player_level_growth_table[0].maximum_mp;
    player_state.vitals.maximum_mp = player_level_growth_table[0].maximum_mp;
    player_state.base_physical_power = player_level_growth_table[0].physical_power_step;
    player_state.base_magic = player_level_growth_table[0].magic_step;
    player_state.next_level_experience = player_level_growth_table[0].experience_threshold;
    player_state.equipped_head_armor_id = KF_OBJECT_NONE;
    player_state.equipped_body_armor_id = KF_OBJECT_NONE;
    player_state.equipped_arm_armor_id = KF_OBJECT_NONE;
    player_state.equipped_leg_armor_id = KF_OBJECT_NONE;
    player_state.equipped_shield_id = KF_OBJECT_NONE;
    player_state.equipped_accessory_id = KF_OBJECT_NONE;
    // Equipping recalculates armor stats too; zeroed IDs are not "no armor".
    player_equip_weapon(KF_ITEM_SHORT_SWORD);
    player_set_equipment_slot(KF_ITEM_SHORT_SWORD, KF_EQUIPMENT_SLOT_REFRESH_ONLY);
    player_select_magic(KF_MAGIC_LIGHT_NEEDLE);
    player_state.fire_defense_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
    player_state.illusion_staff_timer = KF_ILLUSION_STAFF_INACTIVE;
    player_state.slowed_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
    player_state.poison_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
    player_state.darkness_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
    player_state.curse_timer = KF_PLAYER_STATUS_TIMER_INACTIVE;
    map_runtime_state.world_state = {};
    item_stock = {};
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_SHORT_SWORD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_MEDICINAL_HERB)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_SHORT_SWORD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_BATTLE_AXE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_KNIGHT_SWORD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_IRON_MASK)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_KNIGHT_HELM)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_KNIGHT_PLATE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_SMALL_SHIELD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_KNIGHT_SHIELD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_GAUNTLET)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_IRON_BOOTS)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_MEDICINAL_HERB)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_ANTIDOTE_HERB)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_RECOVERY_MEDICINE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_FIRST_SHOP)][kf_enum_encode<u8>(KF_ITEM_DRAGON_KING_GRASS_FRUIT)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_KNIGHT_SWORD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_COLICHEMARDE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_CRESCENT_AXE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_KNIGHT_HELM)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_GREAT_HELM)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_BREASTPLATE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_FULL_PLATE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_FIRE_MAIL)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_TOWER_SHIELD)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_LEG_GUARDS)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_MEDICINAL_HERB)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_ANTIDOTE_HERB)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_RECOVERY_MEDICINE)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_DRAGON_KING_GRASS_LEAF)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_DRAGON_KING_GRASS_FRUIT)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_LIGHT_RING)] = 1;
    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_SECOND_SHOP)][kf_enum_encode<u8>(KF_ITEM_GOLD_CROSS)] = 1;
}

void player_death_restart(void)
{
    KfFloorId floor = player_state.progress_state.current_floor;

    if (map_floor_script(KF_FLOOR_1).floor1.revival_enabled == KF_MAP_SCRIPT_SET && item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_DRAGON_KING_GRASS_FRUIT)] != 0) {
        item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<u8>(KF_ITEM_DRAGON_KING_GRASS_FRUIT)]--;
        map_world_state_persist();
        player_state.camera_position.vx = PLAYER_REVIVAL_POSITION_X;
        player_state.vitals.current_hp = player_state.vitals.maximum_hp;
        player_state.vitals.current_mp = player_state.vitals.maximum_mp;
        player_state.camera_position.vz = PLAYER_REVIVAL_POSITION_Z;
        player_state.camera_rotation.vy = 0;
    } else {
        player_state.camera_position.vx = PLAYER_RESTART_POSITION_X;
        player_state.camera_position.vz = PLAYER_RESTART_POSITION_Z;
        player_state.camera_rotation.vy = 0;
        game_state_initialize();
        floor = KF_FLOOR_FORCE_RELOAD;
    }
    player_state.status_effect_flags = KF_PLAYER_STATUS_NONE;
    player_state.camera_rotation.vz = 0;
    player_state.camera_rotation.vx = 0;
    if (floor != KF_FLOOR_1) {
        player_state.progress_state.current_floor = KF_FLOOR_1;
        player_state.map_variant = KF_MAP_VARIANT_DEFAULT;
        animation_cache_release_all();
        audio_close_vab(audio_state);
        map_load_floor_wrapper();
    }
    player_sync_position_to_map();
    player_state.view_bob_offset = 0;
    player_state.update_state = KF_PLAYER_UPDATE_RECOVERY_FADE;
    player_state.death_camera_pitch_step = 0;
    player_state.death_visual_blend = 0;
    game_graphics_runtime.hud_brightness = 0;
    player_state.view_rotation_offset = {};
    player_state.previous_map_cell.x = player_state.motion_state.map_cell.x;
    player_state.previous_map_cell.z = player_state.motion_state.map_cell.z;
    player_state.camera_position.vy = player_state.foot_height - KF_PLAYER_CAMERA_HEIGHT;
}

void player_adjust_hp(s32 delta)
{
    s32 value = player_state.vitals.current_hp;

    value += delta;

    if (value <= 0) {
        player_state.vitals.current_hp = 0;
        player_death_begin();
        return;
    }
    player_state.vitals.current_hp = std::min<s32>(value, player_state.vitals.maximum_hp);
}

void player_adjust_mp(s32 delta)
{
    s32 value = player_state.vitals.current_mp;

    value += delta;

    if (value <= 0) {
        player_state.vitals.current_mp = 0;
        return;
    }
    player_state.vitals.current_mp = std::min<s32>(value, player_state.vitals.maximum_mp);
}

static void player_learn_trained_magic(KfEffectKind spell, s32 required_base_magic)
{
    if (player_state.base_magic < required_base_magic) {
        return;
    }
    auto &magic = effect_state.magic.entries[kf_enum_encode<u8>(spell)];
    if (magic.learned == KF_MAGIC_UNLEARNED) {
        magic.learned = KF_MAGIC_LEARNED;
        notify_enqueue(KF_NOTIFICATION_MAGIC_LEARNED);
    }
}

// Combat totals for the equipment in p, without side effects (the equipment
// screens preview them too).
void player_compute_combat_stats(KfPlayerState &p)
{
    const KfWeaponRecord *weapon;
    const KfArmorRecord *armor;
    s32 power;

    p.cutting_attack = 0;
    p.striking_attack = 0;
    p.piercing_attack = 0;
    p.holy_attack = 0;
    p.fire_attack = 0;
    p.cutting_defense = 0;
    p.striking_defense = 0;
    p.piercing_defense = 0;
    p.poison_resistance = 0;
    p.magic_defense = 0;
    p.fire_defense = 0;
    p.physical_power = p.base_physical_power;
    p.magic = p.base_magic;
    if ((p.status_effect_flags & KF_PLAYER_STATUS_CURSE) != KF_PLAYER_STATUS_NONE) {
        power = p.physical_power - CURSE_PHYSICAL_POWER_PENALTY;
        power = std::max<s32>(power, 0);
        p.physical_power = power;
    }
    if (p.equipped_weapon_id != KF_OBJECT_NONE) {
        weapon = &weapon_records.entries[kf_enum_encode<u8>(p.equipped_weapon_id)];
        p.cutting_attack += weapon->attack_components[KF_COMBAT_COMPONENT_CUTTING];
        p.striking_attack += weapon->attack_components[KF_COMBAT_COMPONENT_STRIKING];
        p.piercing_attack += weapon->attack_components[KF_COMBAT_COMPONENT_PIERCING];
        p.holy_attack += weapon->attack_components[KF_COMBAT_COMPONENT_HOLY];
        p.fire_attack += weapon->attack_components[KF_COMBAT_COMPONENT_FIRE];
    }
    if (p.equipped_head_armor_id != KF_OBJECT_NONE) {
        armor = &armor_records.entries[kf_enum_encode<u8>(p.equipped_head_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
        p.cutting_defense += armor->cutting_defense;
        p.cutting_defense += armor->cutting_defense;
        p.striking_defense += armor->striking_defense;
        p.piercing_defense += armor->piercing_defense;
        p.poison_resistance += armor->poison_resistance;
        p.magic_defense += armor->magic_defense;
        p.fire_defense += armor->fire_defense;
    }
    if (p.equipped_body_armor_id != KF_OBJECT_NONE) {
        armor = &armor_records.entries[kf_enum_encode<u8>(p.equipped_body_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
        p.cutting_defense += armor->cutting_defense;
        p.cutting_defense += armor->cutting_defense;
        p.striking_defense += armor->striking_defense;
        p.piercing_defense += armor->piercing_defense;
        p.poison_resistance += armor->poison_resistance;
        p.magic_defense += armor->magic_defense;
        p.fire_defense += armor->fire_defense;
    }
    if (p.equipped_arm_armor_id != KF_OBJECT_NONE) {
        armor = &armor_records.entries[kf_enum_encode<u8>(p.equipped_arm_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
        p.cutting_defense += armor->cutting_defense;
        p.cutting_defense += armor->cutting_defense;
        p.striking_defense += armor->striking_defense;
        p.piercing_defense += armor->piercing_defense;
        p.poison_resistance += armor->poison_resistance;
        p.magic_defense += armor->magic_defense;
        p.fire_defense += armor->fire_defense;
    }
    if (p.equipped_leg_armor_id != KF_OBJECT_NONE) {
        armor = &armor_records.entries[kf_enum_encode<u8>(p.equipped_leg_armor_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
        p.cutting_defense += armor->cutting_defense;
        p.cutting_defense += armor->cutting_defense;
        p.striking_defense += armor->striking_defense;
        p.piercing_defense += armor->piercing_defense;
        p.poison_resistance += armor->poison_resistance;
        p.magic_defense += armor->magic_defense;
        p.fire_defense += armor->fire_defense;
    }
    if (p.equipped_shield_id != KF_OBJECT_NONE) {
        armor = &armor_records.entries[kf_enum_encode<u8>(p.equipped_shield_id) - kf_enum_encode<u8>(KF_ITEM_IRON_MASK)];
        p.cutting_defense += armor->cutting_defense;
        p.cutting_defense += armor->cutting_defense;
        p.striking_defense += armor->striking_defense;
        p.piercing_defense += armor->piercing_defense;
        p.poison_resistance += armor->poison_resistance;
        p.magic_defense += armor->magic_defense;
        p.fire_defense += armor->fire_defense;
    }
    switch (p.equipped_accessory_id) {
    default:
        // Other items (including no accessory) grant no accessory-specific bonus.
        break;
    case KF_ITEM_LIGHT_RING:
        p.holy_attack += LIGHT_RING_HOLY_ATTACK_BONUS;
        break;
    case KF_ITEM_MOON_AMULET:
        p.magic_defense += MOON_AMULET_MAGIC_DEFENSE_BONUS;
        break;
    case KF_ITEM_WIND_BLADE_BRACELET:
        p.fire_defense += WIND_BLADE_BRACELET_FIRE_DEFENSE_BONUS;
        break;
    case KF_ITEM_TWO_HEADED_DRAGON_RING:
        p.magic += TWO_HEADED_DRAGON_RING_MAGIC_BONUS;
        break;
    case KF_ITEM_VERDITE:
        p.magic += VERDITE_EQUIPPED_MAGIC_BONUS;
        break;
    case KF_ITEM_GOLD_CROSS:
        p.holy_attack += GOLD_CROSS_HOLY_ATTACK_BONUS;
        break;
    }
    if (p.equipped_head_armor_id == KF_ITEM_BLACK_MASK) {
        p.physical_power -= BLACK_MASK_PHYSICAL_POWER_PENALTY;
    }
    if ((p.status_effect_flags & KF_PLAYER_STATUS_FIRE_DEFENSE_BOOST) != KF_PLAYER_STATUS_NONE) {
        p.fire_defense += FIRE_DEFENSE_STATUS_BONUS;
    }
    if (p.physical_power >= KF_PLAYER_POWER_MAX + 1) {
        p.physical_power = KF_PLAYER_POWER_MAX;
    }
    if (p.magic >= KF_PLAYER_POWER_MAX + 1) {
        p.magic = KF_PLAYER_POWER_MAX;
    }
}

void player_recalculate_combat_stats(void)
{
    player_compute_combat_stats(player_state);
    const auto &healing = effect_state.magic.entries[kf_enum_encode<u8>(KF_MAGIC_HEALING)];
    if (healing.learned != KF_MAGIC_UNLEARNED) {
        player_learn_trained_magic(KF_MAGIC_DISPOISON, DISPOISON_REQUIRED_BASE_MAGIC);
    }
    player_learn_trained_magic(KF_MAGIC_FIRE_WALL, FIRE_WALL_REQUIRED_BASE_MAGIC);
    player_learn_trained_magic(KF_MAGIC_LIGHTNING_BOLT, LIGHTNING_BOLT_REQUIRED_BASE_MAGIC);
}

void player_increment_physical_power_training(void)
{
    player_state.physical_power_training++;
    if (player_state.physical_power_training >= KF_PLAYER_TRAINING_POINTS_PER_GAIN) {
        player_state.base_physical_power++;
        player_state.physical_power_training = 0;
        if (player_state.base_physical_power >= KF_PLAYER_POWER_MAX + 1) {
            player_state.base_physical_power = KF_PLAYER_POWER_MAX;
        } else {
            notify_enqueue(KF_NOTIFICATION_PHYSICAL_POWER_INCREASED);
        }
        player_recalculate_combat_stats();
    }
}

void player_increment_magic_training(void)
{
    player_state.magic_training++;
    if (player_state.magic_training >= KF_PLAYER_TRAINING_POINTS_PER_GAIN) {
        player_state.base_magic++;
        player_state.magic_training = 0;
        if (player_state.base_magic >= KF_PLAYER_POWER_MAX + 1) {
            player_state.base_magic = KF_PLAYER_POWER_MAX;
        } else {
            notify_enqueue(KF_NOTIFICATION_MAGIC_POWER_INCREASED);
        }
        player_recalculate_combat_stats();
    }
}

void player_add_experience(s16 amount)
{
    const KfPlayerLevelGrowth *growth;
    u8 level;

    player_state.experience += amount;
    player_state.experience = std::min<s32>(player_state.experience, KF_PLAYER_EXPERIENCE_MAX);
    while (player_state.experience >= player_state.next_level_experience) {
        level = player_state.progress_state.level;
        if (player_state.progress_state.level >= KF_PLAYER_LEVEL_MAX) {
            break;
        }
        player_state.progress_state.level = level + 1;
        if (level >= KF_PLAYER_LEVEL_GROWTH_COUNT) {
            growth = &player_level_growth_table[KF_PLAYER_LEVEL_GROWTH_COUNT - 1];
            player_state.vitals.maximum_hp +=
                growth->maximum_hp
                - growth[-1].maximum_hp;
            player_state.vitals.maximum_mp +=
                growth->maximum_mp
                - growth[-1].maximum_mp;
            player_state.base_physical_power += growth->physical_power_step;
            player_state.base_magic += growth->magic_step;
            player_state.next_level_experience +=
                growth->experience_threshold
                - growth[-1].experience_threshold;
        } else {
            growth = &player_level_growth_table[level];
            player_state.vitals.maximum_hp = growth->maximum_hp;
            player_state.vitals.maximum_mp = growth->maximum_mp;
            player_state.base_physical_power += growth->physical_power_step;
            player_state.base_magic += growth->magic_step;
            player_state.next_level_experience = growth->experience_threshold;
        }
        player_state.vitals.maximum_hp = std::min<s32>(player_state.vitals.maximum_hp, KF_PLAYER_VITAL_MAX);
        player_state.vitals.maximum_mp = std::min<s32>(player_state.vitals.maximum_mp, KF_PLAYER_VITAL_MAX);
        player_state.base_physical_power =
            std::min<s32>(player_state.base_physical_power, KF_PLAYER_POWER_MAX);
        player_state.base_magic = std::min<s32>(player_state.base_magic, KF_PLAYER_POWER_MAX);
        player_recalculate_combat_stats();
        notify_enqueue(KF_NOTIFICATION_LEVEL_UP);
        sound_ref_play(audio_playback(), &player_sound_refs[KF_PLAYER_SOUND_LEVEL_UP], KF_AUDIO_MAX_VOLUME);
    }
}

s32 player_calculate_damage_component(s32 defender_power, s32 defense, s32 attack)
{
    s32 threshold = defender_power;
    s32 excess = defense;

    if (attack == 0) {
        return 0;
    }
    threshold = excess + threshold / PLAYER_DAMAGE_POWER_DIVISOR;
    excess = attack - threshold;
    excess = std::max<s32>(excess, 0);
    if (threshold == 0) {
        threshold = 1;
    }
    return excess + (attack * attack) / (threshold * PLAYER_DAMAGE_THRESHOLD_MULTIPLIER);
}

void player_apply_damage(
    u16 component0,
    u16 component1,
    u16 component2,
    KfPlayerStatusFlags status_effect_flags,
    u16 component3,
    u16 component4,
    u16 scale_q12,
    u16 multiplier_tenths)
{
    s32 damage;
    s32 loss;
    s32 remaining;

    if ((status_effect_flags & KF_PLAYER_STATUS_CURSE) != KF_PLAYER_STATUS_NONE) {
        player_state.curse_timer = KF_CURSE_DURATION_UPDATES;
        player_state.status_effect_flags |= KF_PLAYER_STATUS_CURSE;
    }
    if (((status_effect_flags & KF_PLAYER_STATUS_DARKNESS) != KF_PLAYER_STATUS_NONE)
        && player_state.equipped_accessory_id != KF_ITEM_MOON_AMULET) {
        if (player_state.darkness_timer != KF_PLAYER_STATUS_TIMER_INACTIVE) {
            player_state.darkness_timer =
                std::max<s32>(player_state.darkness_timer, KF_DARKNESS_REAPPLY_TIMER);
        } else {
            player_state.darkness_timer = KF_DARKNESS_DURATION_UPDATES;
        }
        player_state.status_effect_flags |= KF_PLAYER_STATUS_DARKNESS;
    }
    if ((status_effect_flags & KF_PLAYER_STATUS_POISON) != KF_PLAYER_STATUS_NONE) {
        if (player_state.poison_resistance
            < (kf::random_next() * PLAYER_POISON_ROLL_BUCKETS) >> PLAYER_POISON_ROLL_SHIFT) {
            player_state.poison_timer = KF_POISON_DURATION_UPDATES;
            player_state.status_effect_flags |= KF_PLAYER_STATUS_POISON;
        }
    }
    if ((status_effect_flags & KF_PLAYER_STATUS_SLOWED) != KF_PLAYER_STATUS_NONE) {
        player_state.slowed_timer = KF_SLOWED_DURATION_UPDATES;
        player_state.status_effect_flags |= KF_PLAYER_STATUS_SLOWED;
    }
    damage = player_calculate_damage_component(
        player_state.physical_power * KF_DAMAGE_SUBUNITS_PER_HP,
        player_state.cutting_defense * KF_DAMAGE_SUBUNITS_PER_HP,
        component0 * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += player_calculate_damage_component(
        player_state.physical_power * KF_DAMAGE_SUBUNITS_PER_HP,
        player_state.striking_defense * KF_DAMAGE_SUBUNITS_PER_HP,
        component1 * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += player_calculate_damage_component(
        player_state.physical_power * KF_DAMAGE_SUBUNITS_PER_HP,
        player_state.piercing_defense * KF_DAMAGE_SUBUNITS_PER_HP,
        component2 * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += player_calculate_damage_component(
        player_state.physical_power * KF_DAMAGE_SUBUNITS_PER_HP,
        player_state.magic_defense * KF_DAMAGE_SUBUNITS_PER_HP,
        component3 * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += player_calculate_damage_component(
        player_state.physical_power * KF_DAMAGE_SUBUNITS_PER_HP,
        player_state.fire_defense * KF_DAMAGE_SUBUNITS_PER_HP,
        component4 * KF_DAMAGE_SUBUNITS_PER_HP);
    damage += KF_DAMAGE_SUBUNITS_PER_HP / 2;
    damage = (scale_q12 * (damage / KF_DAMAGE_SUBUNITS_PER_HP)) >> KF_FIXED12_BITS;
    loss = (multiplier_tenths * damage) / KF_PLAYER_DAMAGE_MULTIPLIER_ONE;
    if (loss != 0) {
        remaining = player_state.vitals.current_hp - loss;
        remaining = std::max<s32>(remaining, 0);
        player_state.vitals.current_hp = remaining;
        if (player_state.update_state != KF_PLAYER_UPDATE_DYING) {
            player_state.update_state = KF_PLAYER_DAMAGE_FRAME_FIRST;
        }
    }
}

void player_apply_radial_damage(
    const VECTOR *origin,
    u32 radius,
    u16 falloff_q12,
    u16 component0,
    u16 component1,
    u16 component2,
    u16 component3,
    u16 component4,
    u16 scale_q12,
    u16 multiplier_tenths)
{
    s32 distance;
    u16 attenuation;

    distance = player_distance_to_point(origin->vx, origin->vy, origin->vz, radius, radius);
    if (distance == -1) {
        return;
    }
    if (falloff_q12 != KF_FIXED12_ONE) {
        attenuation = radial_damage_attenuated_scale(distance, radius, falloff_q12, scale_q12);
    } else {
        attenuation = scale_q12;
    }
    player_apply_damage(
        component0, component1, component2, KF_PLAYER_STATUS_NONE, component3, component4,
        attenuation, multiplier_tenths);
}

void player_select_magic(KfEffectKind magic_id)
{
    player_state.magic_charge = 0;
    player_state.selected_magic_id = magic_id;
    if (magic_id == KF_MAGIC_NONE) {
        player_state.selected_magic_record = NULL;
    } else {
        player_state.selected_magic_record =
            &effect_state.magic.entries[kf_enum_encode<u8>(player_state.selected_magic_id)];
    }
}

void player_death_reset_module_state(void)
{
    kf::restore_initial_value<player_sound_refs>();
    kf::restore_initial_value<player_death_saved_fog_near>();
    kf::restore_initial_value<player_death_saved_color_matrix>();
    kf::restore_initial_value<item_stock>();
}
