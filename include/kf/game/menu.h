#ifndef KF_GAME_MENU_H
#define KF_GAME_MENU_H

#include <kf/game/save.h>
#include <kf/lib/item.h>
#include <kf/lib/map.h>
#include <kf/lib/resource_file.h>
#include <kf/lib/debug.h>
#include <kf/renderer/renderer.hpp>
#include <kf/game/magic.h>
#include <kf/lib/render_types.h>
#include <kf/lib/menu_types.h>

enum class KfMenuMode : s32 {
    KF_MENU_MODE_ROOT = 0,
    KF_MENU_MODE_ITEM_PICKUP = 1,
    KF_MENU_MODE_SHOP = 2
}; using enum KfMenuMode;

enum class KfMenuRootChoice : s32 {
    KF_ROOT_CHOICE_NONE = -1,
    KF_ROOT_CHOICE_USE_ITEM = 0,
    KF_ROOT_CHOICE_USE_MAGIC = 1,
    KF_ROOT_CHOICE_EQUIPMENT = 2,
    KF_ROOT_CHOICE_STATUS = 3,
    KF_ROOT_CHOICE_DROP_ITEM = 4,
    KF_ROOT_CHOICE_SYSTEM = 5,
    KF_ROOT_CHOICE_CONFIG = 6
}; using enum KfMenuRootChoice;

enum class KfEquipmentMenuCategory : s32 {
    KF_EQUIP_MENU_NONE = -1,
    KF_EQUIP_MENU_WEAPON = 0,
    KF_EQUIP_MENU_MAGIC = 1,
    KF_EQUIP_MENU_SHIELD = 2,
    KF_EQUIP_MENU_HEAD = 3,
    KF_EQUIP_MENU_BODY = 4,
    KF_EQUIP_MENU_ARM = 5,
    KF_EQUIP_MENU_LEG = 6,
    KF_EQUIP_MENU_ACCESSORY = 7
}; using enum KfEquipmentMenuCategory;

enum {
    KF_MENU_ROOT_RETURN_ROW = kf_enum_encode<s32>(KF_ROOT_CHOICE_CONFIG) + 1,
    KF_MENU_ROOT_ROW_COUNT = KF_MENU_ROOT_RETURN_ROW + 1,
    KF_MENU_EQUIPMENT_RETURN_ROW = kf_enum_encode<s32>(KF_EQUIP_MENU_ACCESSORY) + 1,
    KF_MENU_EQUIPMENT_ROW_COUNT = KF_MENU_EQUIPMENT_RETURN_ROW + 1,
    KF_MENU_CONFIG_EFFECTS_ROW = 0,
    KF_MENU_CONFIG_MUSIC_ROW = 1,
    KF_MENU_CONFIG_GAUGES_ROW = 2,
    KF_MENU_CONFIG_COMPASS_ROW = 3,
    KF_MENU_CONFIG_SETTING_COUNT = 4,
    KF_MENU_CONFIG_RETURN_ROW = KF_MENU_CONFIG_SETTING_COUNT,
    KF_MENU_CONFIG_ROW_COUNT = KF_MENU_CONFIG_RETURN_ROW + 1
};

enum class KfMenuConfirmKind : s32 {
    KF_MENU_CONFIRM_USE = 0,
    KF_MENU_CONFIRM_DROP = 1,
    KF_MENU_CONFIRM_YES_NO = 2,
    KF_MENU_CONFIRM_BUY = 3,
    KF_MENU_CONFIRM_SELL = 4,
    KF_MENU_CONFIRM_EQUIP = 5
}; using enum KfMenuConfirmKind;

enum class KfMenuPreviewMode : s32 {
    KF_MENU_PREVIEW_ITEM_MODEL = 0,
    KF_MENU_PREVIEW_ITEM_DETAIL = 1,
    KF_MENU_PREVIEW_MAGIC_ICON = 2
}; using enum KfMenuPreviewMode;

enum class KfTradeMode : s32 {
    KF_TRADE_NONE = -1,
    KF_TRADE_BUY = 0,
    KF_TRADE_SELL = 1
}; using enum KfTradeMode;

enum class KfMenuWindowKind : s32 {
    KF_MENU_WINDOW_ROOT = 0,
    KF_MENU_WINDOW_EQUIPMENT = 1,
    KF_MENU_WINDOW_SYSTEM = 2,

    KF_MENU_WINDOW_SAVE_LOAD = 3,
    KF_MENU_WINDOW_SAVE = 4,
    KF_MENU_WINDOW_LOAD = 5,
    KF_MENU_WINDOW_CONFIG = 6,
    KF_MENU_WINDOW_SHOP = 7
}; using enum KfMenuWindowKind;

enum {
    KF_MENU_WINDOW_LAYOUT_COUNT = 9
};

enum {
    KF_SHOP_ROW_RETURN = 2,
    KF_SHOP_ROW_GOLD = 3,
    KF_SHOP_CHOICE_COUNT = KF_SHOP_ROW_RETURN + 1
};

static inline KfMenuResult menu_confirm_result_from_choice(KfMenuConfirmChoice choice)
{
    return kf_enum_decode<KfMenuResult>(-kf_enum_encode<s32>(choice));
}

enum {
    MENU_CONFIRM_TEXT_X = 96,
    MENU_CONFIRM_ROW_STEP = 20
};

template <typename Value>
class KfMenuSelection {
public:
    constexpr KfMenuSelection(Value value) : value_(kf_enum_encode<s32>(value)) {}
    constexpr KfMenuSelection(KfMenuResult result) : value_(kf_enum_encode<s32>(result)) {}

    constexpr s32 encoded_value() const { return value_; }
    friend constexpr bool operator==(KfMenuSelection lhs, KfMenuSelection rhs)
    { return lhs.value_ == rhs.value_; }

private:
    s32 value_;
};

using ::kf_enum_encode;

template <typename Integer, typename Value>
constexpr Integer kf_enum_encode(KfMenuSelection<Value> selection)
{
    return static_cast<Integer>(selection.encoded_value());
}

typedef KfMenuSelection<KfEffectKind> KfMagicPanelResult;

enum {
    KF_MENU_SYSTEM_RETURN_ROW = 2,
    KF_MENU_SYSTEM_ROW_COUNT = KF_MENU_SYSTEM_RETURN_ROW + 1,
    KF_MENU_SAVE_RETURN_ROW = KF_SAVE_SLOT_COUNT,
    KF_MENU_SAVE_ROW_COUNT = KF_MENU_SAVE_RETURN_ROW + 1,
    KF_MENU_LOAD_RETURN_ROW = KF_SAVE_SLOT_COUNT,
    KF_MENU_LOAD_ROW_COUNT = KF_MENU_LOAD_RETURN_ROW + 1
};

enum class KfMenuSystemAction : s32 {
    KF_MENU_SYSTEM_ACTION_NONE = -1,
    KF_MENU_SYSTEM_ACTION_LOAD = 0,
    KF_MENU_SYSTEM_ACTION_QUIT = 1
}; using enum KfMenuSystemAction;

enum class KfSaveSlotOverlay : s32 {
    KF_SAVE_OVERLAY_NONE = -1,
    KF_SAVE_OVERLAY_SKIP_FIRST = 0,
    KF_SAVE_OVERLAY_SKIP_SECOND = 1,
    KF_SAVE_OVERLAY_SKIP_THIRD = 2,
    KF_SAVE_OVERLAY_ALL = KF_SAVE_SLOT_COUNT
}; using enum KfSaveSlotOverlay;

enum class KfMenuTextureId : s32 {
    KF_MENU_TEXTURE_NONE = 0xff,
    MENU_TEXTURE_LOADING_DATA = 0x67,
    MENU_TEXTURE_SAVING_DATA = 0x68,
    MENU_TEXTURE_POWER_OFF = 0x3e6
}; using enum KfMenuTextureId;

constexpr KfMenuTextureId menu_texture_from_magic(KfEffectKind magic_id)
{
    return kf_enum_decode<KfMenuTextureId>(kf_enum_encode<u8>(magic_id));
}

enum class KfMenuSoundCue : s32 {
    MENU_SOUND_CURSOR = 0,
    MENU_SOUND_CONFIRM = 1,
    MENU_SOUND_CANCEL_OR_ERROR = 2
}; using enum KfMenuSoundCue;

inline constexpr int MENU_TEXT_END = -1;
inline constexpr int MENU_TEXT_BLANK = 0xff;
inline constexpr int MENU_TEXT_GLYPH_MASK = 0x0fff;
inline constexpr int MENU_TEXT_DAKUTEN = 0x1000;
inline constexpr int MENU_TEXT_HANDAKUTEN = 0x2000;
inline constexpr int MENU_NUMBER_BLANK = 10;
inline constexpr int MENU_NUMBER_SLASH = 11;

inline constexpr int MENU_NUMBER_ADVANCE = 7;

inline constexpr int MENU_STATS_VITAL_DIGITS = 4;
inline constexpr int MENU_STATS_VALUE_DIGITS = 6;

enum {
    MENU_CLASS_MIDDLE_STAT_MIN = 40,
    MENU_CLASS_HIGH_STAT_MIN = 60,
    MENU_CLASS_MAGIC_TIER_COUNT = 3,
    MENU_CLASS_LABEL_GLYPHS = 4,
    MENU_CLASS_FIRST_GLYPH = 0x100
};

enum {
    MENU_BACKDROP_COLUMN_STEP = 71,
    MENU_BACKDROP_ROW_STEP = 104,
    MENU_BACKDROP_TOP_Y = 16,
    MENU_BACKDROP_BOTTOM_Y = MENU_BACKDROP_TOP_Y + MENU_BACKDROP_ROW_STEP
};

enum {
    MENU_MARKER_OT_DEPTH = 500,
    MENU_CONTENT_OT_DEPTH = 1000,
    MENU_WIDGET_OT_DEPTH = 2000,
    MENU_WINDOW_OT_DEPTH = 2900,
    MENU_BACKGROUND_OT_DEPTH = 3000,

    MENU_PANEL_INPUT_RELEASE_FRAME = 2
};

typedef struct MenuPoint {
    s16 x;
    s16 y;
} MenuPoint;

enum {
    MENU_GLYPHS_PER_ROW = 10,
    MENU_WINDOW_ROW_CAPACITY = 10
};

typedef struct MenuGlyphRow {
    s16 codes[MENU_GLYPHS_PER_ROW];
} MenuGlyphRow;

typedef struct MenuGlyphString {
    MenuPoint position;
    MenuGlyphRow glyphs;
} MenuGlyphString;

typedef struct MenuWindowLayout {
    MenuGlyphString title;
    MenuGlyphString rows[MENU_WINDOW_ROW_CAPACITY];
} MenuWindowLayout;

typedef struct MenuSpriteDef {
    kf::FaceMaterial material;
    u16 u;
    u16 v;
    s16 width;
    s16 height;
} MenuSpriteDef;

typedef struct MenuTileSprite {
    kf::FaceMaterial material;
    u8 u;
    u8 v;
    u16 width;
    u16 height;
} MenuTileSprite;

enum {
    MENU_LIST_TILE_BACKDROP,
    MENU_LIST_TILE_ROW,
    MENU_LIST_TILE_END,
    MENU_LIST_TILE_SELECTED,
    MENU_LIST_TILE_COUNT
};

enum {
    MENU_BACKGROUND_QUAD_COUNT = 4,
    MENU_DIALOG_QUAD_COUNT = 6
};

typedef struct KfMenuAssets {
    kf::DrawFace background_quads[KF_DISPLAY_BUFFER_COUNT][MENU_BACKGROUND_QUAD_COUNT];
    kf::DrawFace mid_depth_quads[KF_DISPLAY_BUFFER_COUNT];
    kf::DrawFace foreground_quads[KF_DISPLAY_BUFFER_COUNT];
    kf::DrawFace dialog_quads[KF_DISPLAY_BUFFER_COUNT][MENU_DIALOG_QUAD_COUNT];
    MenuSpriteDef number_atlas;
    MenuSpriteDef glyph_atlas;
    MenuTileSprite window_backdrop;
    MenuSpriteDef option_background;
    MenuSpriteDef option_highlight;
    MenuSpriteDef row_background;
    MenuSpriteDef row_confirmed_background;
    MenuTileSprite list_tiles[MENU_LIST_TILE_COUNT];
    MenuSpriteDef selection_cursor;
} KfMenuAssets;

typedef struct KfMenuList {
    MenuGlyphString title;
    u8 list_x;
    u8 list_y;
    u8 entry_count;
    u8 visible_rows;
    u8 scroll_offset;
    u8 selected_index;
    u8 cursor_row;
    u8 glyphs_per_entry;
    s16 *glyph_rows;
    u8 *quantities;
} KfMenuList;

extern void menu_list_previous(KfMenuList *list);
extern void menu_list_next(KfMenuList *list);
extern bool menu_list_handle_navigation(KfMenuList &list, u32 input, u32 previous);

enum {
    MENU_ITEM_PREVIEW_YAW_STEP = 16,
    MENU_PICKUP_PREVIEW_YAW_STEP = 8
};

enum {
    MENU_ITEM_PREVIEW_TRANSLATION_X = 560,
    MENU_ITEM_PREVIEW_TRANSLATION_Y = 140,
    MENU_ITEM_PREVIEW_TRANSLATION_Z = 1500
};

enum {
    MENU_ITEM_NAME_X = 174,
    MENU_ITEM_PREVIEW_NAME_Y = 36,
    MENU_ITEM_PREVIEW_LINE_HEIGHT = 18,
    MENU_ITEM_PREVIEW_QUANTITY_DIGITS = 2
};

extern SVECTOR menu_item_preview_rotation;
extern KfMenuAssets menu_assets;
extern MenuWindowLayout menu_window_layouts[KF_MENU_WINDOW_LAYOUT_COUNT];
extern MenuGlyphRow item_name_rows[KF_ITEM_COUNT];
extern MenuGlyphRow magic_name_rows[KF_MAGIC_PLAYER_COUNT];
extern u16 item_buy_prices[KF_ITEM_COUNT][KF_ITEM_SHOP_COUNT];
extern u16 item_sell_prices[KF_ITEM_COUNT][KF_ITEM_SHOP_COUNT];
enum class KfMenuModelAllocation : s32 {
    KF_MENU_MODEL_RELEASED = 0,
    KF_MENU_MODEL_ALLOCATED = 1
}; using enum KfMenuModelAllocation;
extern KfMenuModelAllocation menu_item_model_allocation_pending;

extern void item_load_database(void);
extern void item_menu_root(KfItemStockBank shop_bank);
extern KfMenuResult item_pickup_confirm(KfObjectId item_id);
extern void menu_add_frame_quad(void);
extern void menu_add_marker_quad(void);
extern void menu_blit_sprite(
    const MenuSpriteDef *sprite, const MenuPoint *position);
extern void menu_blit_sprite_translucent(
    const MenuSpriteDef *sprite, const MenuPoint *position);
extern void menu_config_panel(void);
extern void menu_draw_dialog_frame(
    const KfSaveSlotSummary *summaries, KfSaveSlotOverlay slot_overlay);
extern void menu_draw_item_detail(
    KfObjectId item_id, KfItemStockBank shop_bank, KfTradeMode price_mode);
extern void menu_draw_item_name_frame(KfObjectId item_id);
extern void menu_draw_number(
    const MenuSpriteDef *font, const MenuGlyphString *string);
extern void menu_draw_string(
    const MenuSpriteDef *font, const MenuGlyphString *string);
extern void menu_draw_name_list(void);
extern void menu_draw_stats_header(void);
extern void menu_draw_status_details(void);
extern void menu_draw_two_option(
    const MenuGlyphString *accept_label, const MenuGlyphString *decline_label,
    KfMenuConfirmChoice selected_choice, KfMenuConfirmState confirmation);
extern void menu_draw_window(KfMenuWindowKind window_kind, s32 count, s32 highlight, KfMenuConfirmState confirmation);
extern void menu_draw_window_backdrop(void);
extern void menu_format_number(
    s32 value, s32 count, KfFormatPaddingMode padding_mode, s16 *out);
extern void menu_drop_item(void);
extern u32 menu_enter_mode(KfMenuMode menu_mode);
extern u32 menu_enter_mode(KfMenuMode menu_mode, KfObjectId item_id);
extern u32 menu_enter_mode(KfMenuMode menu_mode, KfItemStockBank shop_bank);
extern void menu_equip_select(KfEquipmentMenuCategory equipment_category);
extern void menu_frame_begin(void);
extern void menu_item_model_preview(KfObjectId item_id);
// PC extension: while set, item previews compare equipment totals for this slot.
extern void menu_set_equipment_compare(KfEquipmentMenuCategory category);
extern void menu_list_init(KfMenuList *list, KfMenuWindowKind window_kind, s32 row);
extern KfMenuResult menu_list_interact(
    const KfMenuList *list, KfMenuConfirmKind confirm_kind, KfMenuPreviewMode preview_mode,
    KfObjectId item_id, KfItemStockBank shop_bank, KfTradeMode price_mode);
extern KfMenuResult menu_list_interact(
    const KfMenuList *list, KfMenuConfirmKind confirm_kind, KfMenuPreviewMode preview_mode,
    KfEffectKind magic_id, KfItemStockBank shop_bank, KfTradeMode price_mode);
extern void menu_list_render(const KfMenuList *list);
extern KfResourceLoadResult menu_load_item_model(KfObjectId item_id);
extern KfResourceLoadResult menu_load_item_texture(KfMenuTextureId texture_id);
extern void menu_release_item_model(void);
extern KfMenuResult menu_load_panel(void);
extern KfMagicPanelResult menu_magic_panel(void);
extern void menu_map_viewer(KfObjectId item_id);
extern void menu_option_root(void);
extern void menu_play_input_sound(KfMenuSoundCue cue);
extern void menu_present_frame(void);
extern s32 menu_root(void);
extern void menu_save_confirm(void);
extern KfMenuResult menu_save_load_hub(void);
extern KfMenuResult menu_save_panel(void);
extern void menu_spell_select(void);
extern void menu_status_panel(void);
extern KfMenuResult menu_two_option_prompt(
    KfMenuWindowKind window_kind, s32 count, s32 highlight,
    const KfSaveSlotSummary *summaries);
extern void talk_show_dialogue_page(KfFloorId floor, u8 stage, KfCharacterId character_id, u8 page);

void menu_enqueue_background(void);

#endif
