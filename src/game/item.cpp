#include <kf/game/resources.h>
#include <kf/game/menu_glyphs.h>
#include <kf/lib/null.h>
#include <kf/game/graphics.h>

#include <kf/platform/input.hpp>
#include <kf/lib/map_data.h>
#include <kf/lib/item.h>
#include <kf/game/resource_file.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/game/game.h>
#include <kf/lib/render_face.h>

void item_menu_buy(KfItemStockBank shop_bank);
void item_menu_sell(KfItemStockBank shop_bank);

enum {
    MENU_SHOP_VISIBLE_ROWS = 9,
    MENU_PICKUP_CONFIRM_TEXT_X = 60,
    MENU_PICKUP_CONFIRM_ACCEPT_Y = 26,
    MENU_PICKUP_CONFIRM_DECLINE_Y = MENU_PICKUP_CONFIRM_ACCEPT_Y + MENU_CONFIRM_ROW_STEP
};

KfMenuAssets menu_assets;

MenuWindowLayout menu_window_layouts[KF_MENU_WINDOW_LAYOUT_COUNT];

MenuGlyphRow item_name_rows[KF_ITEM_COUNT];

MenuGlyphRow magic_name_rows[KF_MAGIC_PLAYER_COUNT];

u16 item_buy_prices[KF_ITEM_COUNT][KF_ITEM_SHOP_COUNT];

u16 item_sell_prices[KF_ITEM_COUNT][KF_ITEM_SHOP_COUNT];

// STAT.DAT stores little-endian words and authored GPU templates. Only this
// loading boundary knows that layout; menus retain copied native descriptions.
namespace {
constexpr std::size_t stat_textured_quad_bytes = 40;
constexpr std::size_t stat_solid_quad_bytes = 24;
constexpr std::size_t stat_packet_word_bytes = 4;
constexpr unsigned stat_packet_length_offset = 3;
constexpr unsigned stat_packet_red_offset = 4;
constexpr unsigned stat_packet_green_offset = 5;
constexpr unsigned stat_packet_blue_offset = 6;
constexpr unsigned stat_packet_command_offset = 7;
constexpr u8 stat_packet_kind_mask = 0xfc;
constexpr u8 stat_textured_quad_command = 0x2c;
constexpr u8 stat_solid_quad_command = 0x28;
constexpr u8 stat_packet_blend_flag = 2;
constexpr u8 stat_packet_raw_texture_flag = 1;
constexpr unsigned stat_quad_page_offset = 22;
constexpr unsigned stat_quad_palette_offset = 14;
constexpr unsigned stat_quad_corners_offset = 8;
constexpr unsigned stat_textured_corner_bytes = 8;
constexpr unsigned stat_solid_corner_bytes = 4;
constexpr unsigned stat_corner_y_offset = 2;
constexpr unsigned stat_corner_u_offset = 4;
constexpr unsigned stat_corner_v_offset = 5;
constexpr std::size_t stat_sprite_bytes = 12;
constexpr unsigned stat_sprite_palette_offset = 2;
constexpr unsigned stat_sprite_u_offset = 4;
constexpr unsigned stat_sprite_v_offset = 6;
constexpr unsigned stat_sprite_x_or_width_offset = 8;
constexpr unsigned stat_sprite_y_or_height_offset = 10;
constexpr unsigned stat_save_return_row = 4;
}

struct MenuDataReader {
    const u8 *cursor;
    std::size_t remaining;
};

static const u8 *menu_data_take(MenuDataReader *reader, std::size_t size)
{
    if (size > reader->remaining)
        kf::host_fail("Truncated COM/STAT.DAT");
    const u8 *data = reader->cursor;
    reader->cursor += size;
    reader->remaining -= size;
    return data;
}

static u16 menu_data_u16(const u8 *data)
{
    return data[0] | (static_cast<u16>(data[1]) << 8);
}

static u16 menu_data_word(MenuDataReader *reader)
{
    return menu_data_u16(menu_data_take(reader, 2));
}

enum class MenuTemplateKind : u8 { Textured, Solid };

static kf::DrawFace menu_data_face(MenuDataReader *reader, MenuTemplateKind kind)
{
    const bool textured = kind == MenuTemplateKind::Textured;
    const std::size_t size = textured ? stat_textured_quad_bytes : stat_solid_quad_bytes;
    const u8 *data = menu_data_take(reader, size);
    kf::DrawFace face{};
    face.shape = kf::FaceShape::Quad;
    // The second bank's first dialog template is empty in the original file.
    if (data[stat_packet_length_offset] == 0) {
        for (std::size_t i = 0; i < size; ++i)
            if (data[i] != 0)
                kf::host_fail("Invalid empty menu template in COM/STAT.DAT");
        return face;
    }
    const u8 command = data[stat_packet_command_offset];
    if (data[stat_packet_length_offset] != size / stat_packet_word_bytes - 1 || (command & stat_packet_kind_mask) != (textured ? stat_textured_quad_command : stat_solid_quad_command))
        kf::host_fail("Unsupported menu template in COM/STAT.DAT");
    if (textured)
        face.material = render_texture_material(menu_data_u16(data + stat_quad_page_offset), menu_data_u16(data + stat_quad_palette_offset));
    else
        face.material.kind = kf::SurfaceKind::Solid;
    face.transparency = command & stat_packet_blend_flag ? kf::FaceTransparency::Blend : kf::FaceTransparency::Opaque;
    face.material.color_mode = textured && (command & stat_packet_raw_texture_flag)
        ? kf::TextureColorMode::Raw : kf::TextureColorMode::Modulated;
    for (unsigned i = 0; i < 4; ++i) {
        auto &vertex = face.vertices[i];
        const u8 *corner = data + stat_quad_corners_offset + i * (textured ? stat_textured_corner_bytes : stat_solid_corner_bytes);
        vertex.x = static_cast<s16>(menu_data_u16(corner));
        vertex.y = static_cast<s16>(menu_data_u16(corner + stat_corner_y_offset));
        if (textured) {
            vertex.u = corner[stat_corner_u_offset] / kf::texture_uv_scale;
            vertex.v = corner[stat_corner_v_offset] / kf::texture_uv_scale;
        }
        const float divisor = textured ? kf::texture_color_unity : kf::color8_scale;
        const bool raw_texture = textured && (command & stat_packet_raw_texture_flag);
        vertex.r = raw_texture ? 1.0f : data[stat_packet_red_offset] / divisor;
        vertex.g = raw_texture ? 1.0f : data[stat_packet_green_offset] / divisor;
        vertex.b = raw_texture ? 1.0f : data[stat_packet_blue_offset] / divisor;
        vertex.a = 1;
    }
    return face;
}

static MenuSpriteDef menu_data_sprite(MenuDataReader *reader)
{
    const u8 *data = menu_data_take(reader, stat_sprite_bytes);
    return {render_texture_material(menu_data_u16(data), menu_data_u16(data + stat_sprite_palette_offset)),
        menu_data_u16(data + stat_sprite_u_offset), menu_data_u16(data + stat_sprite_v_offset),
        static_cast<s16>(menu_data_u16(data + stat_sprite_x_or_width_offset)), static_cast<s16>(menu_data_u16(data + stat_sprite_y_or_height_offset))};
}

static MenuTileSprite menu_data_tile(MenuDataReader *reader)
{
    const u8 *data = menu_data_take(reader, stat_sprite_bytes);
    return {render_texture_material(menu_data_u16(data), menu_data_u16(data + stat_sprite_palette_offset)),
        data[stat_sprite_u_offset], data[stat_sprite_v_offset], menu_data_u16(data + stat_sprite_x_or_width_offset), menu_data_u16(data + stat_sprite_y_or_height_offset)};
}

static void menu_data_glyphs(MenuDataReader *reader, MenuGlyphRow *row)
{
    for (s16 &code : row->codes)
        code = static_cast<s16>(menu_data_word(reader));
}

static void menu_data_string(MenuDataReader *reader, MenuGlyphString *string)
{
    string->position.x = static_cast<s16>(menu_data_word(reader));
    string->position.y = static_cast<s16>(menu_data_word(reader));
    menu_data_glyphs(reader, &string->glyphs);
}

void menu_resources_reload(void)
{
    u8 *stat_data;
    std::size_t stat_size;
    resource_file_load_allocated(memory_arena, &stat_data, "COM/STAT.DAT", &stat_size);
    MenuDataReader reader{stat_data, stat_size};

    for (auto &bank : menu_assets.background_quads)
        for (auto &face : bank)
            face = menu_data_face(&reader, MenuTemplateKind::Textured);
    for (auto &face : menu_assets.mid_depth_quads)
        face = menu_data_face(&reader, MenuTemplateKind::Textured);
    for (auto &face : menu_assets.foreground_quads)
        face = menu_data_face(&reader, MenuTemplateKind::Textured);
    for (auto &bank : menu_assets.dialog_quads)
        for (auto &face : bank)
            face = menu_data_face(&reader, MenuTemplateKind::Solid);
    menu_assets.number_atlas = menu_data_sprite(&reader);
    menu_assets.glyph_atlas = menu_data_sprite(&reader);
    menu_assets.window_backdrop = menu_data_tile(&reader);
    menu_assets.option_background = menu_data_sprite(&reader);
    menu_assets.option_highlight = menu_data_sprite(&reader);
    menu_assets.row_background = menu_data_sprite(&reader);
    menu_assets.row_confirmed_background = menu_data_sprite(&reader);
    for (auto &tile : menu_assets.list_tiles)
        tile = menu_data_tile(&reader);
    menu_assets.selection_cursor = menu_data_sprite(&reader);
    for (auto &layout : menu_window_layouts) {
        menu_data_string(&reader, &layout.title);
        for (auto &row : layout.rows)
            menu_data_string(&reader, &row);
    }
    // Ordinary save files have no format-card action. Keep the authored return
    // label, moved into that removed row in the native menu description.
    auto &save_layout = menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_SAVE)];
    save_layout.rows[KF_MENU_SAVE_RETURN_ROW].glyphs = save_layout.rows[stat_save_return_row].glyphs;
    // Port addition: Configuration gets a language row where Return was, and
    // Return moves down a row. menu_draw_window draws the row's text.
    auto &config = menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_CONFIG)];
    config.rows[KF_MENU_CONFIG_RETURN_ROW] = config.rows[KF_MENU_CONFIG_LANGUAGE_ROW];
    config.rows[KF_MENU_CONFIG_RETURN_ROW].position.y += config.rows[1].position.y - config.rows[0].position.y;
    config.rows[KF_MENU_CONFIG_LANGUAGE_ROW].glyphs.codes[0] = MENU_TEXT_END;
    for (auto &row : item_name_rows)
        menu_data_glyphs(&reader, &row);
    for (auto &row : magic_name_rows)
        menu_data_glyphs(&reader, &row);
    for (auto &item : item_buy_prices)
        for (u16 &price : item)
            price = menu_data_word(&reader);
    for (auto &item : item_sell_prices)
        for (u16 &price : item)
            price = menu_data_word(&reader);

    memory_release_last(memory_arena);
}

void item_load_database(void)
{
    menu_resources_reload();
    resource_file_index_item_models();
}

void item_menu_root(KfItemStockBank shop_bank)
{
    s32 cursor = kf_enum_encode<s32>(KF_TRADE_BUY);
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    KfMenuResult phase = KF_MENU_RESULT_PENDING;
    KfTradeMode action = KF_TRADE_NONE;

    menu_frame_begin();
    menu_draw_window(KF_MENU_WINDOW_SHOP, KF_SHOP_CHOICE_COUNT, kf_enum_encode<s32>(KF_TRADE_BUY), KF_MENU_CONFIRM_IDLE);
    menu_present_frame();
    menu_frame_begin();
    menu_draw_window(KF_MENU_WINDOW_SHOP, KF_SHOP_CHOICE_COUNT, kf_enum_encode<s32>(KF_TRADE_BUY), KF_MENU_CONFIRM_IDLE);
    menu_present_frame();
    menu_frame_begin();
    menu_draw_window(KF_MENU_WINDOW_SHOP, KF_SHOP_CHOICE_COUNT, kf_enum_encode<s32>(KF_TRADE_BUY), KF_MENU_CONFIRM_IDLE);
    menu_play_input_sound(MENU_SOUND_CURSOR);
    kf::host_wait_buttons_released();

    for (;;) {
        menu_present_frame();
        if (action != KF_TRADE_NONE
                || kf_enum_encode<s32>(phase) == kf_enum_encode<s32>(action)) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_SHOP, KF_SHOP_CHOICE_COUNT, cursor, confirm);
            menu_present_frame();
            kf::host_wait_buttons_released();
        }
        switch (action) {
        case KF_TRADE_BUY:
            item_menu_buy(shop_bank);
            break;
        case KF_TRADE_SELL:
            item_menu_sell(shop_bank);
            break;
        }
        action = KF_TRADE_NONE;
        if (phase != KF_MENU_RESULT_PENDING) {
            kf::host_wait_buttons_released();
            return;
        }

        menu_frame_begin();
        confirm = KF_MENU_CONFIRM_IDLE;
        prev = input;
        input = kf::host_read_buttons();
        if (kf::button_pressed(input, prev, kf::Button::Up)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != kf_enum_encode<s32>(KF_TRADE_BUY))
                cursor--;
            else
                cursor = KF_SHOP_ROW_RETURN;
        } else if (kf::button_pressed(input, prev, kf::Button::Down)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != KF_SHOP_ROW_RETURN)
                cursor++;
            else
                cursor = kf_enum_encode<s32>(KF_TRADE_BUY);
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            confirm = KF_MENU_CONFIRM_REQUESTED;
            if (cursor < KF_SHOP_ROW_RETURN)
                action = kf_enum_decode<KfTradeMode>(cursor);
            else
                phase = KF_MENU_RESULT_CANCELLED;
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            phase = KF_MENU_RESULT_CANCELLED;
        }
        menu_draw_window(KF_MENU_WINDOW_SHOP, KF_SHOP_CHOICE_COUNT, cursor, confirm);
    }
}

void item_menu_buy(KfItemStockBank shop_bank)
{
    KfMenuList ctx;
    s16 entries[KF_ITEM_COUNT][MENU_GLYPHS_PER_ROW];
    u8 available[KF_ITEM_COUNT];
    KfObjectId index[KF_ITEM_COUNT];
    u8 *inv;
    s32 slot;
    s32 found;
    s32 j;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    s32 selection = kf_enum_encode<s32>(KF_MENU_RESULT_PENDING);

    kf::host_wait_buttons_released();
    menu_list_init(&ctx, KF_MENU_WINDOW_SHOP, kf_enum_encode<s32>(KF_TRADE_BUY));

    inv = item_stock[kf_enum_encode<s32>(shop_bank)];
    found = 0;
    for (slot = kf_enum_encode<s32>(KF_ITEM_VERDITE); slot < KF_ITEM_COUNT; slot++) {
        if (inv[slot] != 0 && item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][slot] < KF_ITEM_STACK_CAPACITY) {
            for (j = 0; j < MENU_GLYPHS_PER_ROW; j++)
                entries[found][j] = item_name_rows[slot].codes[j];
            available[found] = inv[slot];
            index[found] = kf_enum_decode<KfObjectId>(slot);
            found++;
        }
    }
    for (slot = 0; slot < kf_enum_encode<s32>(KF_ITEM_VERDITE); slot++) {
        if (inv[slot] != 0 && item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][slot] < KF_ITEM_STACK_CAPACITY) {
            for (j = 0; j < MENU_GLYPHS_PER_ROW; j++)
                entries[found][j] = item_name_rows[slot].codes[j];
            available[found] = inv[slot];
            index[found] = kf_enum_decode<KfObjectId>(slot);
            found++;
        }
    }
    ctx.entry_count = found;
    ctx.visible_rows = MENU_SHOP_VISIBLE_ROWS;
    ctx.glyphs_per_entry = MENU_GLYPHS_PER_ROW;
    ctx.glyph_rows = &entries[0][0];
    ctx.quantities = NULL;

    menu_frame_begin();
    if (ctx.entry_count != 0) {
        if (menu_load_item_model(index[ctx.selected_index]) != KF_RESOURCE_LOADED)
            return;
        menu_draw_item_detail(index[ctx.selected_index], shop_bank, KF_TRADE_BUY);
    }
    menu_list_render(&ctx);

    for (;;) {
        menu_present_frame();
        if (confirm == KF_MENU_CONFIRM_REQUESTED) {
            if (menu_list_interact(&ctx, KF_MENU_CONFIRM_BUY,
                    KF_MENU_PREVIEW_ITEM_DETAIL, index[ctx.selected_index], shop_bank, KF_TRADE_BUY)
                    == KF_MENU_RESULT_CANCELLED)
                selection = kf_enum_encode<s32>(KF_MENU_RESULT_PENDING);
            else
                selection = kf_enum_encode<u8>(index[ctx.selected_index]);
        }
        confirm = KF_MENU_CONFIRM_IDLE;
        if (selection != kf_enum_encode<s32>(KF_MENU_RESULT_PENDING)) {
            kf::host_wait_buttons_released();
            break;
        }

        prev = input;
        input = kf::host_read_buttons();
        if (ctx.entry_count == 0) {
            if (input != 0) {
                menu_play_input_sound(MENU_SOUND_CURSOR);
                selection = kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED);
            }
        } else if (menu_list_handle_navigation(ctx, input, prev)) {
            if (menu_load_item_model(index[ctx.selected_index]) != KF_RESOURCE_LOADED)
                return;
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            if (player_state.gold
                    < item_buy_prices[kf_enum_encode<u8>(index[ctx.selected_index])][kf_enum_encode<s32>(shop_bank) - kf_enum_encode<s32>(KF_ITEM_STOCK_FIRST_SHOP)]) {
                menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            } else {
                menu_play_input_sound(MENU_SOUND_CONFIRM);
                confirm = KF_MENU_CONFIRM_REQUESTED;
            }
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            selection = kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED);
        }

        menu_frame_begin();
        if (ctx.entry_count != 0)
            menu_draw_item_detail(index[ctx.selected_index], shop_bank, KF_TRADE_BUY);
        menu_list_render(&ctx);
    }

    menu_release_item_model();
    if (selection != kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED)) {
        if (selection == kf_enum_encode<s32>(KF_ITEM_GOLD_CROSS))
            inv[kf_enum_encode<u8>(KF_ITEM_GOLD_CROSS)]--;
        player_state.gold -= item_buy_prices[selection][kf_enum_encode<s32>(shop_bank) - kf_enum_encode<s32>(KF_ITEM_STOCK_FIRST_SHOP)];
        item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][selection]++;
    }
}

void item_menu_sell(KfItemStockBank shop_bank)
{
    KfMenuList ctx;
    s16 entries[KF_ITEM_COUNT][MENU_GLYPHS_PER_ROW];
    u8 available[KF_ITEM_COUNT];
    KfObjectId index[KF_ITEM_COUNT];
    u8 *inv;
    s32 slot;
    s32 found;
    s32 j;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    s32 selection = kf_enum_encode<s32>(KF_MENU_RESULT_PENDING);

    kf::host_wait_buttons_released();
    menu_list_init(&ctx, KF_MENU_WINDOW_SHOP, kf_enum_encode<s32>(KF_TRADE_SELL));

    inv = item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)];
    found = 0;
    for (slot = 0; slot < kf_enum_encode<s32>(KF_ITEM_GOLD_CROSS); slot++) {
        if (inv[slot] != 0) {
            available[found] = inv[slot];
            if (player_item_is_equipped(kf_enum_decode<KfObjectId>(slot)))
                available[found]--;
            if (available[found] != 0) {
                for (j = 0; j < MENU_GLYPHS_PER_ROW; j++)
                    entries[found][j] = item_name_rows[slot].codes[j];
                index[found] = kf_enum_decode<KfObjectId>(slot);
                found++;
            }
        }
    }
    ctx.entry_count = found;
    ctx.visible_rows = MENU_SHOP_VISIBLE_ROWS;
    ctx.glyphs_per_entry = MENU_GLYPHS_PER_ROW;
    ctx.glyph_rows = &entries[0][0];
    ctx.quantities = NULL;

    menu_frame_begin();
    if (ctx.entry_count != 0) {
        if (menu_load_item_model(index[ctx.selected_index]) != KF_RESOURCE_LOADED)
            return;
        menu_draw_item_detail(index[ctx.selected_index], shop_bank, KF_TRADE_SELL);
    }
    menu_list_render(&ctx);

    for (;;) {
        menu_present_frame();
        if (confirm == KF_MENU_CONFIRM_REQUESTED) {
            selection = kf_enum_encode<s32>(menu_list_interact(&ctx, KF_MENU_CONFIRM_SELL,
                    KF_MENU_PREVIEW_ITEM_DETAIL, index[ctx.selected_index], shop_bank, KF_TRADE_SELL));
            if (selection == kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED))
                selection = kf_enum_encode<s32>(KF_MENU_RESULT_PENDING);
            else
                selection = kf_enum_encode<u8>(index[ctx.selected_index]);
        }
        confirm = KF_MENU_CONFIRM_IDLE;
        if (selection != kf_enum_encode<s32>(KF_MENU_RESULT_PENDING)) {
            kf::host_wait_buttons_released();
            break;
        }

        prev = input;
        input = kf::host_read_buttons();
        if (ctx.entry_count == 0) {
            if (input != 0) {
                menu_play_input_sound(MENU_SOUND_CURSOR);
                selection = kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED);
            }
        } else if (menu_list_handle_navigation(ctx, input, prev)) {
            if (menu_load_item_model(index[ctx.selected_index]) != KF_RESOURCE_LOADED)
                return;
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            confirm = KF_MENU_CONFIRM_REQUESTED;
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            selection = kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED);
        }

        menu_frame_begin();
        if (ctx.entry_count != 0)
            menu_draw_item_detail(index[ctx.selected_index], shop_bank, KF_TRADE_SELL);
        menu_list_render(&ctx);
    }

    menu_release_item_model();
    if (selection != kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED)) {
        inv[selection]--;
        player_state.gold += item_sell_prices[selection][kf_enum_encode<s32>(shop_bank) - kf_enum_encode<s32>(KF_ITEM_STOCK_FIRST_SHOP)];
    }
}

KfMenuResult item_pickup_confirm(KfObjectId item_id)
{
    MenuGlyphString accept_label;
    MenuGlyphString decline_label;
    KfMenuConfirmChoice choice = KF_MENU_CHOICE_ACCEPT;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    KfMenuResult result = KF_MENU_RESULT_PENDING;
    s32 stock_count;
    s32 prev;

    stock_count = item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<s32>(item_id)];
    if (menu_load_item_model(item_id) != KF_RESOURCE_LOADED)
        return KF_MENU_RESULT_DECLINED;

    accept_label.position.x = MENU_PICKUP_CONFIRM_TEXT_X;
    accept_label.position.y = MENU_PICKUP_CONFIRM_ACCEPT_Y;
    accept_label.glyphs.codes[0] = menu_glyphs::pickup[0];
    accept_label.glyphs.codes[1] = menu_glyphs::pickup[1];
    accept_label.glyphs.codes[2] = MENU_TEXT_END;
    decline_label.position.x = MENU_PICKUP_CONFIRM_TEXT_X;
    decline_label.position.y = MENU_PICKUP_CONFIRM_DECLINE_Y;
    decline_label.glyphs.codes[0] = menu_glyphs::cancel[0];
    decline_label.glyphs.codes[1] = menu_glyphs::cancel[1];
    decline_label.glyphs.codes[2] = menu_glyphs::cancel[2];
    decline_label.glyphs.codes[3] = MENU_TEXT_END;

    menu_frame_begin();
    menu_draw_item_name_frame(item_id);
    menu_draw_two_option(
        &accept_label,
        &decline_label, KF_MENU_CHOICE_ACCEPT, KF_MENU_CONFIRM_IDLE);
    menu_present_frame();
    menu_frame_begin();
    menu_draw_item_name_frame(item_id);
    menu_draw_two_option(
        &accept_label,
        &decline_label, KF_MENU_CHOICE_ACCEPT, KF_MENU_CONFIRM_IDLE);
    menu_present_frame();
    menu_frame_begin();
    menu_draw_item_name_frame(item_id);
    menu_draw_two_option(
        &accept_label,
        &decline_label, KF_MENU_CHOICE_ACCEPT, KF_MENU_CONFIRM_IDLE);
    menu_play_input_sound(MENU_SOUND_CURSOR);
    kf::host_wait_buttons_released();

    for (;;) {
        menu_present_frame();
        if (result != KF_MENU_RESULT_PENDING) {
            menu_frame_begin();
            menu_draw_item_name_frame(item_id);
            menu_draw_two_option(
                &accept_label,
                &decline_label, choice, confirm);
            menu_present_frame();
            kf::host_wait_buttons_released();
            break;
        }

        menu_frame_begin();
        prev = input;
        input = kf::host_read_buttons();
        if ((kf::button_pressed(input, prev, kf::Button::Up))
                || (kf::button_pressed(input, prev, kf::Button::Down))) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (choice != KF_MENU_CHOICE_ACCEPT)
                choice = KF_MENU_CHOICE_ACCEPT;
            else
                choice = KF_MENU_CHOICE_DECLINE;
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            confirm = KF_MENU_CONFIRM_REQUESTED;
            if (choice != KF_MENU_CHOICE_ACCEPT) {
                result = KF_MENU_RESULT_DECLINED;
            } else {
                result = KF_MENU_RESULT_STACK_FULL;
                if (stock_count != KF_ITEM_STACK_CAPACITY) {
                    item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<s32>(item_id)]++;
                    result = KF_MENU_RESULT_ACCEPTED;
                }
            }
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            result = KF_MENU_RESULT_DECLINED;
        }
        menu_draw_item_name_frame(item_id);
        menu_draw_two_option(
            &accept_label,
            &decline_label, choice, confirm);
    }

    menu_release_item_model();
    return result;
}

void item_reset_module_state(void)
{
    kf::restore_initial_value<menu_assets>();
    kf::restore_initial_value<menu_window_layouts>();
    kf::restore_initial_value<item_name_rows>();
    kf::restore_initial_value<magic_name_rows>();
    kf::restore_initial_value<item_buy_prices>();
    kf::restore_initial_value<item_sell_prices>();
}
