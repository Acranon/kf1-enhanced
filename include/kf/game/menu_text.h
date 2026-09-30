#ifndef KF_GAME_MENU_TEXT_H
#define KF_GAME_MENU_TEXT_H

#include <kf/game/menu.h>
#include <kf/platform/language.h>

// Authored atlas glyphs, not Unicode. Resource-owned rows still come from STAT.DAT.
// English v1.0's executable edits are recorded in docs/english-resources.md.
enum class MenuLabel {
    Pickup, Cancel, Yes, No, Use, Drop, Buy, Sell, Equip, Unequip,
    HeaderLevel, DetailLevel, HeaderGold, DetailGold, PriceUnit,
    PhysicalPower, MagicPower, TotalAttack, TotalDefense, Attack, Defense,
    Cutting, Striking, Holy, Fire, PoisonResistance, MagicDefense,
    SlowedStatus, DarknessStatus, CurseStatus,
    Language, LanguageName, LanguageUnavailable
};

inline MenuGlyphRow menu_label(MenuLabel label)
{
    const bool english = kf::game_language() == kf::Language::English;
    switch (label) {
    case MenuLabel::Language:
        // The translation's individual capitals also spell BACK and COMPASS.
        // Japanese uses the original kana (hiragana n is glyph 0x67): げんご.
        return english ? MenuGlyphRow{{0x136, 0xdc, 0x50, 0x146, 0x11, 0xdc, 0x146, 0x138, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_DAKUTEN | 0x48, 0x67, MENU_TEXT_DAKUTEN | 0x49, MENU_TEXT_END}};
    case MenuLabel::LanguageName:
        // ENGLISH / にほんご.
        return english ? MenuGlyphRow{{0x138, 0x50, 0x146, 0x136, 0x4, 0x134, 0x3, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x55, 0x5d, 0x67, MENU_TEXT_DAKUTEN | 0x49, MENU_TEXT_END}};
    case MenuLabel::LanguageUnavailable:
        // NO DATA / つかえません.
        return english ? MenuGlyphRow{{0x50, 0x0, MENU_TEXT_BLANK, 0xb, 0xdc, 0x10, 0xdc, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x51, 0x45, 0x43, 0x5e, 0x4d, 0x67, MENU_TEXT_END}};
    case MenuLabel::Pickup:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x53, 0x6a, MENU_TEXT_END}};
    case MenuLabel::Cancel:
        return english ? MenuGlyphRow{{0x50, 0x00, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x63, 0x61, 0x6a, MENU_TEXT_END}};
    case MenuLabel::Yes:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x59, 0x41, MENU_TEXT_END}};
    case MenuLabel::No:
        return english ? MenuGlyphRow{{0x50, 0x00, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x41, 0x41, 0x43, MENU_TEXT_END}};
    case MenuLabel::Use:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x72, 0x42, MENU_TEXT_END}};
    case MenuLabel::Drop:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x75, 0x52, 0x6a, MENU_TEXT_END}};
    case MenuLabel::Buy:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x74, 0x42, MENU_TEXT_END}};
    case MenuLabel::Sell:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x73, 0x6a, MENU_TEXT_END}};
    case MenuLabel::Equip:
        return english ? MenuGlyphRow{{0x00, 0x06, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x70, 0x71, MENU_TEXT_END}};
    case MenuLabel::Unequip:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0x414e, 0x414f, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x59, MENU_TEXT_DAKUTEN | 0x4c, 0x4c, MENU_TEXT_END}};
    case MenuLabel::HeaderLevel:
        return english ? MenuGlyphRow{{0x2b, 0x1c, MENU_TEXT_BLANK, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x2b, MENU_TEXT_DAKUTEN | 0x1c, 0x2a, MENU_TEXT_END}};
    case MenuLabel::DetailLevel:
        return english ? MenuGlyphRow{{0x2b, 0x1c, 0x2a, MENU_TEXT_END}}
                       : MenuGlyphRow{{0x2b, MENU_TEXT_DAKUTEN | 0x1c, 0x2a, MENU_TEXT_END}};
    case MenuLabel::HeaderGold:
        return english ? MenuGlyphRow{{0x36, 0x37, MENU_TEXT_BLANK, MENU_TEXT_BLANK, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_DAKUTEN | 0x9, 0x2d, 0x2a, MENU_TEXT_DAKUTEN | 0x13, MENU_TEXT_END}};
    case MenuLabel::DetailGold:
        return english ? MenuGlyphRow{{0x36, 0x37, 0x2a, MENU_TEXT_BLANK, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_DAKUTEN | 0x9, 0x2d, 0x2a, MENU_TEXT_DAKUTEN | 0x13, MENU_TEXT_END}};
    case MenuLabel::PriceUnit:
        return english ? MenuGlyphRow{{0x09, 0x2d, 0x2a, MENU_TEXT_BLANK, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_DAKUTEN | 0x9, 0x2d, 0x2a, MENU_TEXT_DAKUTEN | 0x13, MENU_TEXT_END}};
    case MenuLabel::PhysicalPower:
        return {{0x8c, static_cast<s16>(english ? MENU_TEXT_BLANK : 0x8b), MENU_TEXT_END}};
    case MenuLabel::MagicPower:
        return {{0x78, static_cast<s16>(english ? MENU_TEXT_BLANK : 0x8b), MENU_TEXT_END}};
    case MenuLabel::TotalAttack:
        return {{0xce, 0xcf, 0x89, 0x8a, static_cast<s16>(english ? MENU_TEXT_BLANK : 0x8b), MENU_TEXT_END}};
    case MenuLabel::TotalDefense:
        return {{0xce, 0xcf, 0x7a, 0xd0, static_cast<s16>(english ? MENU_TEXT_BLANK : 0x8b), MENU_TEXT_END}};
    case MenuLabel::Attack:
        return {{0x89, 0x8a, static_cast<s16>(english ? MENU_TEXT_BLANK : 0x8b), MENU_TEXT_END}};
    case MenuLabel::Defense:
        return {{0x7a, 0xd0, static_cast<s16>(english ? MENU_TEXT_BLANK : 0x8b), MENU_TEXT_END}};
    case MenuLabel::Cutting:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0xd2, 0x51, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_BLANK, 0xd1, 0x6a, MENU_TEXT_END}};
    case MenuLabel::Striking:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0xd1, 0x6a, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_BLANK, 0xd2, 0x51, MENU_TEXT_END}};
    case MenuLabel::Holy:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0xbf, MENU_TEXT_BLANK, MENU_TEXT_BLANK, 0x78, 0x79, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_BLANK, 0xbf, 0x58, 0x78, 0x79, MENU_TEXT_END}};
    case MenuLabel::Fire:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0xd4, MENU_TEXT_BLANK, MENU_TEXT_BLANK, 0x78, 0x79, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_BLANK, 0xd4, 0x58, 0x78, 0x79, MENU_TEXT_END}};
    case MenuLabel::PoisonResistance:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0x414c, 0x414d, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_BLANK, 0x88, MENU_TEXT_END}};
    case MenuLabel::MagicDefense:
        return english ? MenuGlyphRow{{MENU_TEXT_BLANK, 0x4144, MENU_TEXT_BLANK, MENU_TEXT_BLANK, 0x78, 0x79, MENU_TEXT_END}}
                       : MenuGlyphRow{{MENU_TEXT_BLANK, 0x78, 0x58, 0x78, 0x79, MENU_TEXT_END}};
    case MenuLabel::SlowedStatus:
        return {{static_cast<s16>(english ? 0x4143 : 0xc9), MENU_TEXT_END}};
    case MenuLabel::DarknessStatus:
        return {{static_cast<s16>(english ? 0x4144 : 0xc7), MENU_TEXT_END}};
    case MenuLabel::CurseStatus:
        return {{static_cast<s16>(english ? 0x4143 : 0xc8), MENU_TEXT_END}};
    }
    return {{MENU_TEXT_END}};
}

#endif // KF_GAME_MENU_TEXT_H
