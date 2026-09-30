#ifndef KF_GAME_MENU_GLYPHS_H
#define KF_GAME_MENU_GLYPHS_H

#include <kf/game/menu.h>

#include <array>

// Original Japanese atlas sequences, not Unicode code points. Keep partial
// updates at callers: several labels intentionally reuse the preceding row.
namespace menu_glyphs {
inline constexpr std::array<s16, 3> on = {0xf9, 0xfa, MENU_TEXT_END};
inline constexpr std::array<s16, 4> off = {0xf9, 0xfb, 0xfb, MENU_TEXT_END};
inline constexpr std::array<s16, 4> experience = {0x82, 0x83, 0x84, MENU_TEXT_END};
inline constexpr std::array<s16, 4> level = {0x2b, MENU_TEXT_DAKUTEN | 0x1c, 0x2a, MENU_TEXT_END};
inline constexpr std::array<s16, 4> character_class = {0x7, 0x28, 0xc, MENU_TEXT_END};
inline constexpr std::array<s16, 3> floor = {0xcc, 0xcd, MENU_TEXT_END};
inline constexpr std::array<s16, 3> hp = {0xf0, 0xf2, MENU_TEXT_END};
inline constexpr std::array<s16, 3> mp = {0xf1, 0xf2, MENU_TEXT_END};
inline constexpr std::array<s16, 3> status = {0x85, 0x86, MENU_TEXT_END};
inline constexpr std::array<s16, 5> gold = {MENU_TEXT_DAKUTEN | 0x9, 0x2d, 0x2a, MENU_TEXT_DAKUTEN | 0x13, MENU_TEXT_END};
inline constexpr std::array<s16, 3> healthy = {0xc5, 0xc6, MENU_TEXT_END};
inline constexpr std::array<s16, 2> slowed = {0xc9, MENU_TEXT_END};
inline constexpr std::array<s16, 2> poison = {0x88, MENU_TEXT_END};
inline constexpr std::array<s16, 2> darkness = {0xc7, MENU_TEXT_END};
inline constexpr std::array<s16, 2> curse = {0xc8, MENU_TEXT_END};
inline constexpr std::array<s16, 3> physical_power = {0x8c, 0x8b, MENU_TEXT_END};
inline constexpr std::array<s16, 3> magic_power = {0x78, 0x8b, MENU_TEXT_END};
inline constexpr std::array<s16, 6> total_attack = {0xce, 0xcf, 0x89, 0x8a, 0x8b, MENU_TEXT_END};
inline constexpr std::array<s16, 6> total_defense = {0xce, 0xcf, 0x7a, 0xd0, 0x8b, MENU_TEXT_END};
inline constexpr std::array<s16, 4> attack = {0x89, 0x8a, 0x8b, MENU_TEXT_END};
inline constexpr std::array<s16, 4> defense = {0x7a, 0xd0, 0x8b, MENU_TEXT_END};
inline constexpr std::array<s16, 4> cutting = {MENU_TEXT_BLANK, 0xd1, 0x6a, MENU_TEXT_END};
inline constexpr std::array<s16, 4> striking = {MENU_TEXT_BLANK, 0xd2, 0x51, MENU_TEXT_END};
inline constexpr std::array<s16, 4> piercing = {MENU_TEXT_BLANK, 0xd3, 0x4c, MENU_TEXT_END};
inline constexpr std::array<s16, 6> holy = {MENU_TEXT_BLANK, 0xbf, 0x58, 0x78, 0x79, MENU_TEXT_END};
inline constexpr std::array<s16, 6> fire = {MENU_TEXT_BLANK, 0xd4, 0x58, 0x78, 0x79, MENU_TEXT_END};
inline constexpr std::array<s16, 3> poison_resistance = {MENU_TEXT_BLANK, 0x88, MENU_TEXT_END};
inline constexpr std::array<s16, 6> magic_defense = {MENU_TEXT_BLANK, 0x78, 0x58, 0x78, 0x79, MENU_TEXT_END};
inline constexpr std::array<s16, 3> quantity = {0xca, 0xcb, MENU_TEXT_END};
inline constexpr std::array<s16, 3> use = {0x72, 0x42, MENU_TEXT_END};
inline constexpr std::array<s16, 4> drop = {0x75, 0x52, 0x6a, MENU_TEXT_END};
inline constexpr std::array<s16, 3> yes = {0x59, 0x41, MENU_TEXT_END};
inline constexpr std::array<s16, 4> no = {0x41, 0x41, 0x43, MENU_TEXT_END};
inline constexpr std::array<s16, 3> buy = {0x74, 0x42, MENU_TEXT_END};
inline constexpr std::array<s16, 3> sell = {0x73, 0x6a, MENU_TEXT_END};
inline constexpr std::array<s16, 3> equip = {0x70, 0x71, MENU_TEXT_END};
inline constexpr std::array<s16, 4> cancel = {0x63, 0x61, 0x6a, MENU_TEXT_END};
inline constexpr std::array<s16, 3> pickup = {0x53, 0x6a, MENU_TEXT_END};
inline constexpr std::array<s16, 4> unequip = {0x59, MENU_TEXT_DAKUTEN | 0x4c, 0x4c, MENU_TEXT_END};
}

#endif // KF_GAME_MENU_GLYPHS_H
