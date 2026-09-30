#pragma once
#include <kf/game/menu.h>

// Atlas sequences, not Unicode code points. Keep partial updates at callers:
// several labels intentionally reuse the preceding row.
namespace menu_glyphs {
// Original Japanese SLPS-00017 sequences.
namespace japanese {
inline constexpr s16 on[] = {0xf9, 0xfa, MENU_TEXT_END};
inline constexpr s16 off[] = {0xf9, 0xfb, 0xfb, MENU_TEXT_END};
inline constexpr s16 experience[] = {0x82, 0x83, 0x84, MENU_TEXT_END};
inline constexpr s16 level[] = {0x2b, MENU_TEXT_DAKUTEN | 0x1c, 0x2a, MENU_TEXT_END};
inline constexpr s16 character_class[] = {0x7, 0x28, 0xc, MENU_TEXT_END};
inline constexpr s16 floor[] = {0xcc, 0xcd, MENU_TEXT_END};
inline constexpr s16 hp[] = {0xf0, 0xf2, MENU_TEXT_END};
inline constexpr s16 mp[] = {0xf1, 0xf2, MENU_TEXT_END};
inline constexpr s16 status[] = {0x85, 0x86, MENU_TEXT_END};
inline constexpr s16 gold[] = {MENU_TEXT_DAKUTEN | 0x9, 0x2d, 0x2a, MENU_TEXT_DAKUTEN | 0x13, MENU_TEXT_END};
inline constexpr s16 healthy[] = {0xc5, 0xc6, MENU_TEXT_END};
inline constexpr s16 slowed[] = {0xc9, MENU_TEXT_END};
inline constexpr s16 poison[] = {0x88, MENU_TEXT_END};
inline constexpr s16 darkness[] = {0xc7, MENU_TEXT_END};
inline constexpr s16 curse[] = {0xc8, MENU_TEXT_END};
inline constexpr s16 physical_power[] = {0x8c, 0x8b, MENU_TEXT_END};
inline constexpr s16 magic_power[] = {0x78, 0x8b, MENU_TEXT_END};
inline constexpr s16 total_attack[] = {0xce, 0xcf, 0x89, 0x8a, 0x8b, MENU_TEXT_END};
inline constexpr s16 total_defense[] = {0xce, 0xcf, 0x7a, 0xd0, 0x8b, MENU_TEXT_END};
inline constexpr s16 attack[] = {0x89, 0x8a, 0x8b, MENU_TEXT_END};
inline constexpr s16 defense[] = {0x7a, 0xd0, 0x8b, MENU_TEXT_END};
inline constexpr s16 cutting[] = {MENU_TEXT_BLANK, 0xd1, 0x6a, MENU_TEXT_END};
inline constexpr s16 striking[] = {MENU_TEXT_BLANK, 0xd2, 0x51, MENU_TEXT_END};
inline constexpr s16 piercing[] = {MENU_TEXT_BLANK, 0xd3, 0x4c, MENU_TEXT_END};
inline constexpr s16 holy[] = {MENU_TEXT_BLANK, 0xbf, 0x58, 0x78, 0x79, MENU_TEXT_END};
inline constexpr s16 fire[] = {MENU_TEXT_BLANK, 0xd4, 0x58, 0x78, 0x79, MENU_TEXT_END};
inline constexpr s16 poison_resistance[] = {MENU_TEXT_BLANK, 0x88, MENU_TEXT_END};
inline constexpr s16 magic_defense[] = {MENU_TEXT_BLANK, 0x78, 0x58, 0x78, 0x79, MENU_TEXT_END};
inline constexpr s16 quantity[] = {0xca, 0xcb, MENU_TEXT_END};
inline constexpr s16 use[] = {0x72, 0x42, MENU_TEXT_END};
inline constexpr s16 drop[] = {0x75, 0x52, 0x6a, MENU_TEXT_END};
inline constexpr s16 yes[] = {0x59, 0x41, MENU_TEXT_END};
inline constexpr s16 no[] = {0x41, 0x41, 0x43, MENU_TEXT_END};
inline constexpr s16 buy[] = {0x74, 0x42, MENU_TEXT_END};
inline constexpr s16 sell[] = {0x73, 0x6a, MENU_TEXT_END};
inline constexpr s16 equip[] = {0x70, 0x71, MENU_TEXT_END};
inline constexpr s16 cancel[] = {0x63, 0x61, 0x6a, MENU_TEXT_END};
inline constexpr s16 pickup[] = {0x53, 0x6a, MENU_TEXT_END};
inline constexpr s16 unequip[] = {0x59, MENU_TEXT_DAKUTEN | 0x4c, 0x4c, MENU_TEXT_END};
// Port additions for the Configuration menu's language row (from sushi-shi's
// port, with its "n" corrected to glyph 0x67): "gengo", "nihongo" and
// "tsukaemasen" in the original kana.
inline constexpr s16 language[] = {MENU_TEXT_DAKUTEN | 0x48, 0x67, MENU_TEXT_DAKUTEN | 0x49, MENU_TEXT_END};
inline constexpr s16 language_name[] = {0x55, 0x5d, 0x67, MENU_TEXT_DAKUTEN | 0x49, MENU_TEXT_END};
inline constexpr s16 language_unavailable[] = {0x51, 0x45, 0x43, 0x5e, 0x4d, 0x67, MENU_TEXT_END};
}

// John Osborne's English patch v1.0 (with HwitVlf's save fix) redraws the atlas
// and changes these GAME.EXE immediates. The patch varies some labels per
// screen, so those screens get their own names; Japanese uses one sequence.
namespace english {
inline constexpr s16 level[] = {0x2b, 0x1c, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 level_summary[] = {0x2b, 0x1c, 0x2a, MENU_TEXT_END};
inline constexpr s16 gold[] = {0x36, 0x37, MENU_TEXT_BLANK, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 gold_summary[] = {0x36, 0x37, 0x2a, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 gold_price[] = {0x9, 0x2d, 0x2a, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 slowed[] = {0x4143, MENU_TEXT_END};
inline constexpr s16 darkness[] = {0x4144, MENU_TEXT_END};
inline constexpr s16 curse[] = {0x4143, MENU_TEXT_END};
inline constexpr s16 physical_power[] = {0x8c, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 magic_power[] = {0x78, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 total_attack[] = {0xce, 0xcf, 0x89, 0x8a, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 total_defense[] = {0xce, 0xcf, 0x7a, 0xd0, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 attack[] = {0x89, 0x8a, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 defense[] = {0x7a, 0xd0, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 cutting[] = {MENU_TEXT_BLANK, 0xd2, 0x51, MENU_TEXT_END};
inline constexpr s16 striking[] = {MENU_TEXT_BLANK, 0xd1, 0x6a, MENU_TEXT_END};
// The patch leaves the seventh code to whatever the stack held; end it explicitly.
inline constexpr s16 holy[] = {MENU_TEXT_BLANK, 0xbf, MENU_TEXT_BLANK, MENU_TEXT_BLANK, 0x78, 0x79, MENU_TEXT_END};
inline constexpr s16 fire[] = {MENU_TEXT_BLANK, 0xd4, MENU_TEXT_BLANK, MENU_TEXT_BLANK, 0x78, 0x79, MENU_TEXT_END};
inline constexpr s16 poison_resistance[] = {MENU_TEXT_BLANK, 0x414c, 0x414d, MENU_TEXT_END};
inline constexpr s16 magic_defense[] = {MENU_TEXT_BLANK, 0x4144, MENU_TEXT_BLANK, MENU_TEXT_BLANK, 0x78, 0x79, MENU_TEXT_END};
inline constexpr s16 ok[] = {0x0, 0x6, MENU_TEXT_END};
inline constexpr s16 drop[] = {0x0, 0x6, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 cancel[] = {0x50, 0x0, MENU_TEXT_BLANK, MENU_TEXT_END};
// The save fix shows "KK" here to avoid a console freeze; the port uses NO.
inline constexpr s16 save_no[] = {0x50, 0x0, MENU_TEXT_BLANK, MENU_TEXT_END};
inline constexpr s16 unequip[] = {MENU_TEXT_BLANK, 0x414e, 0x414f, MENU_TEXT_END};
// LANGUAGE, ENGLISH and NO DATA from the translation's capital letters (the
// ones that spell BACK and COMPASS), as in sushi-shi's port.
inline constexpr s16 language[] = {0x136, 0xdc, 0x50, 0x146, 0x11, 0xdc, 0x146, 0x138, MENU_TEXT_END};
inline constexpr s16 language_name[] = {0x138, 0x50, 0x146, 0x136, 0x4, 0x134, 0x3, MENU_TEXT_END};
inline constexpr s16 language_unavailable[] = {0x50, 0x0, MENU_TEXT_BLANK, 0xb, 0xdc, 0x10, 0xdc, MENU_TEXT_END};
}

// Active sequences; menu_glyphs_use_english() switches them for the patched atlas.
inline const s16 *on = japanese::on;
inline const s16 *off = japanese::off;
inline const s16 *experience = japanese::experience;
inline const s16 *level = japanese::level;
inline const s16 *level_summary = japanese::level;
inline const s16 *character_class = japanese::character_class;
inline const s16 *floor = japanese::floor;
inline const s16 *hp = japanese::hp;
inline const s16 *mp = japanese::mp;
inline const s16 *status = japanese::status;
inline const s16 *gold = japanese::gold;
inline const s16 *gold_summary = japanese::gold;
inline const s16 *gold_price = japanese::gold;
inline const s16 *healthy = japanese::healthy;
inline const s16 *slowed = japanese::slowed;
inline const s16 *poison = japanese::poison;
inline const s16 *darkness = japanese::darkness;
inline const s16 *curse = japanese::curse;
inline const s16 *physical_power = japanese::physical_power;
inline const s16 *magic_power = japanese::magic_power;
inline const s16 *total_attack = japanese::total_attack;
inline const s16 *total_defense = japanese::total_defense;
inline const s16 *attack = japanese::attack;
inline const s16 *defense = japanese::defense;
inline const s16 *cutting = japanese::cutting;
inline const s16 *striking = japanese::striking;
inline const s16 *piercing = japanese::piercing;
inline const s16 *holy = japanese::holy;
inline const s16 *fire = japanese::fire;
inline const s16 *poison_resistance = japanese::poison_resistance;
inline const s16 *magic_defense = japanese::magic_defense;
inline const s16 *quantity = japanese::quantity;
inline const s16 *use = japanese::use;
inline const s16 *drop = japanese::drop;
inline const s16 *yes = japanese::yes;
inline const s16 *no = japanese::no;
inline const s16 *save_yes = japanese::yes;
inline const s16 *save_no = japanese::no;
inline const s16 *buy = japanese::buy;
inline const s16 *sell = japanese::sell;
inline const s16 *equip = japanese::equip;
inline const s16 *cancel = japanese::cancel;
inline const s16 *pickup = japanese::pickup;
inline const s16 *unequip = japanese::unequip;
inline const s16 *language = japanese::language;
inline const s16 *language_name = japanese::language_name;
inline const s16 *language_unavailable = japanese::language_unavailable;

// Back to the original labels (the language can change during play).
inline void menu_glyphs_use_japanese()
{
    on = japanese::on;
    off = japanese::off;
    experience = japanese::experience;
    level = japanese::level;
    character_class = japanese::character_class;
    floor = japanese::floor;
    hp = japanese::hp;
    mp = japanese::mp;
    status = japanese::status;
    gold = japanese::gold;
    healthy = japanese::healthy;
    slowed = japanese::slowed;
    poison = japanese::poison;
    darkness = japanese::darkness;
    curse = japanese::curse;
    physical_power = japanese::physical_power;
    magic_power = japanese::magic_power;
    total_attack = japanese::total_attack;
    total_defense = japanese::total_defense;
    attack = japanese::attack;
    defense = japanese::defense;
    cutting = japanese::cutting;
    striking = japanese::striking;
    piercing = japanese::piercing;
    holy = japanese::holy;
    fire = japanese::fire;
    poison_resistance = japanese::poison_resistance;
    magic_defense = japanese::magic_defense;
    quantity = japanese::quantity;
    use = japanese::use;
    drop = japanese::drop;
    yes = japanese::yes;
    no = japanese::no;
    buy = japanese::buy;
    sell = japanese::sell;
    equip = japanese::equip;
    cancel = japanese::cancel;
    pickup = japanese::pickup;
    unequip = japanese::unequip;
    language = japanese::language;
    language_name = japanese::language_name;
    language_unavailable = japanese::language_unavailable;
    level_summary = japanese::level;
    gold_summary = japanese::gold;
    gold_price = japanese::gold;
    save_yes = japanese::yes;
    save_no = japanese::no;
}

inline void menu_glyphs_use_english()
{
    level = english::level;
    level_summary = english::level_summary;
    gold = english::gold;
    gold_summary = english::gold_summary;
    gold_price = english::gold_price;
    slowed = english::slowed;
    darkness = english::darkness;
    curse = english::curse;
    physical_power = english::physical_power;
    magic_power = english::magic_power;
    total_attack = english::total_attack;
    total_defense = english::total_defense;
    attack = english::attack;
    defense = english::defense;
    cutting = english::cutting;
    striking = english::striking;
    holy = english::holy;
    fire = english::fire;
    poison_resistance = english::poison_resistance;
    magic_defense = english::magic_defense;
    use = english::ok;
    drop = english::drop;
    // The patch left the generic list's yes/no untranslated; use OK/NO there too.
    yes = english::ok;
    no = english::save_no;
    save_yes = english::ok;
    save_no = english::save_no;
    buy = english::ok;
    sell = english::ok;
    equip = english::ok;
    cancel = english::cancel;
    pickup = english::ok;
    unequip = english::unequip;
    language = english::language;
    language_name = english::language_name;
    language_unavailable = english::language_unavailable;
}
}
