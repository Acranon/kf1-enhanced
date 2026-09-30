#include <kf/game/resources.h>
#include <kf/game/menu_glyphs.h>
#include <kf/lib/null.h>
#include <kf/lib/bool.h>

#include <kf/platform/input.hpp>
#include <kf/game/menu.h>
#include <kf/game/game.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <kf/game/graphics.h>
#include <kf/lib/render_face.h>

static constexpr s32 pickup_preview_translation[] = {220, 140, 1500};

KfMenuModelAllocation menu_item_model_allocation_pending = KF_MENU_MODEL_RELEASED;

SVECTOR menu_item_preview_rotation = {0, 0, 0, 0};

static constexpr int MENU_PRIMITIVE_BRIGHTNESS = 96;
static constexpr int MENU_STATUS_BACKDROP_LEFT_X = 6;
static constexpr int MENU_STATUS_BACKDROP_RIGHT_X = MENU_STATUS_BACKDROP_LEFT_X + MENU_BACKDROP_COLUMN_STEP;

static inline void menu_preview_light_source(MATRIX *light_source)
{
    light_source->m[0][0] = -KF_FIXED12_ONE;
    light_source->m[0][1] = -KF_FIXED12_ONE;
    light_source->m[0][2] = -KF_FIXED12_ONE;
    light_source->m[1][0] = -KF_FIXED12_ONE;
    light_source->m[1][1] = -KF_FIXED12_ONE;
    light_source->m[1][2] = -KF_FIXED12_ONE;
    light_source->m[2][0] = 0;
    light_source->m[2][1] = 0;
    light_source->m[2][2] = 0;
}

static kf::DrawFace menu_textured_quad(const kf::FaceMaterial &material,
    s32 x, s32 y, s32 width, s32 height)
{
    kf::DrawFace face{};
    face.material = material;
    render_face_rectangle(&face, x, y, x + width, y + height);
    for (auto &vertex : face.vertices) {
        vertex.r = vertex.g = vertex.b = MENU_PRIMITIVE_BRIGHTNESS / kf::texture_color_unity;
        vertex.a = 1;
    }
    return face;
}

static void menu_submit_template(kf::DrawFace face, s32 depth)
{
    face.depth = depth;
    kf::host_enqueue_face(face);
}

void menu_enqueue_background(void)
{
    const auto &background = menu_assets.background_quads[
        kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)];
    for (s32 i = MENU_BACKGROUND_QUAD_COUNT - 1; i >= 0; --i)
        menu_submit_template(background[i], MENU_BACKGROUND_OT_DEPTH);
}

static void menu_draw_backdrop_tile(s32 x, s32 y, KfBool32 flip_x, KfBool32 flip_y)
{
    const auto &tile = menu_assets.window_backdrop;
    auto face = menu_textured_quad(tile.material, x, y, tile.width, tile.height);
    face.transparency = kf::FaceTransparency::Blend;
    render_face_uv_rectangle(&face,
        flip_x ? tile.u + tile.width : tile.u,
        flip_y ? tile.v + tile.height : tile.v,
        flip_x ? tile.u : tile.u + tile.width,
        flip_y ? tile.v : tile.v + tile.height);
    menu_submit_template(face, MENU_WINDOW_OT_DEPTH);
}

void menu_status_panel(void)
{
    s32 frame;
    s32 input;

    frame = 0;
    while (1) {
        menu_frame_begin();
        menu_draw_status_details();

        menu_draw_backdrop_tile(MENU_STATUS_BACKDROP_LEFT_X, MENU_BACKDROP_TOP_Y, false, false);

        menu_draw_backdrop_tile(MENU_STATUS_BACKDROP_RIGHT_X, MENU_BACKDROP_TOP_Y, true, false);

        menu_draw_backdrop_tile(MENU_STATUS_BACKDROP_LEFT_X, MENU_BACKDROP_BOTTOM_Y, false, true);

        menu_draw_backdrop_tile(MENU_STATUS_BACKDROP_RIGHT_X, MENU_BACKDROP_BOTTOM_Y, true, true);

        menu_draw_window_backdrop();
        menu_present_frame();
        if (frame < MENU_PANEL_INPUT_RELEASE_FRAME) {
            frame++;
            continue;
        }
        if (frame == MENU_PANEL_INPUT_RELEASE_FRAME) {
            kf::host_wait_buttons_released();
            frame++;
            continue;
        }
        input = kf::host_read_buttons();
        if (input != 0) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            kf::host_wait_buttons_released();
            return;
        }
    }
}

void menu_drop_item(void)
{
    KfMenuList ctx;
    s16 labels[KF_ITEM_COUNT][MENU_GLYPHS_PER_ROW];
    u8 counts[KF_ITEM_COUNT];
    KfObjectId codes[KF_ITEM_COUNT];
    u8 *inv;
    s32 found;
    s32 code;
    s32 j;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    s32 selection = kf_enum_encode<s32>(KF_MENU_RESULT_PENDING);

    kf::host_wait_buttons_released();
    menu_list_init(&ctx, KF_MENU_WINDOW_ROOT, kf_enum_encode<s32>(KF_ROOT_CHOICE_DROP_ITEM));

    found = 0;
    code = 0;
    inv = item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)];
    for (; code < KF_ITEM_COUNT; code++) {
        if (inv[code] != 0) {
            counts[found] = inv[code];
            if (player_item_is_equipped(kf_enum_decode<KfObjectId>(code)))
                counts[found]--;
            if (counts[found] != 0) {
                for (j = 0; j < MENU_GLYPHS_PER_ROW; j++)
                    labels[found][j] = item_name_rows[code].codes[j];
                codes[found] = kf_enum_decode<KfObjectId>(code);
                found++;
            }
        }
    }

    ctx.entry_count = found;
    ctx.glyphs_per_entry = MENU_GLYPHS_PER_ROW;
    ctx.glyph_rows = &labels[0][0];
    ctx.quantities = NULL;

    menu_frame_begin();
    if (ctx.entry_count != 0) {
        if (menu_load_item_model(codes[ctx.selected_index]) != KF_RESOURCE_LOADED)
            return;
        menu_item_model_preview(codes[ctx.selected_index]);
    }
    menu_list_render(&ctx);

    for (;;) {
        menu_present_frame();
        if (confirm == KF_MENU_CONFIRM_REQUESTED) {
            selection = kf_enum_encode<s32>(menu_list_interact(&ctx, KF_MENU_CONFIRM_DROP,
                    KF_MENU_PREVIEW_ITEM_MODEL, codes[ctx.selected_index], KF_ITEM_STOCK_PLAYER, KF_TRADE_BUY));
            if (selection == kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED))
                selection = kf_enum_encode<s32>(KF_MENU_RESULT_PENDING);
            else
                selection = kf_enum_encode<u8>(codes[ctx.selected_index]);
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
            if (menu_load_item_model(codes[ctx.selected_index]) != KF_RESOURCE_LOADED)
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
            menu_item_model_preview(codes[ctx.selected_index]);
        menu_list_render(&ctx);
    }

    menu_release_item_model();
    if (selection != kf_enum_encode<s32>(KF_MENU_RESULT_CANCELLED))
        inv[selection]--;
}

KfMenuResult menu_save_load_hub(void)
{
    s32 cursor = kf_enum_encode<s32>(KF_MENU_SYSTEM_ACTION_LOAD);
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    KfMenuResult result = KF_MENU_RESULT_PENDING;
    KfMenuSystemAction action = KF_MENU_SYSTEM_ACTION_NONE;

    for (;;) {
        if (action != KF_MENU_SYSTEM_ACTION_NONE || kf_enum_encode<s32>(result) == kf_enum_encode<s32>(action)) {
            menu_frame_begin();
            menu_draw_window(KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_ROW_COUNT, cursor, confirm);
            menu_present_frame();
            kf::host_wait_buttons_released();
        }

        switch (action) {
        case KF_MENU_SYSTEM_ACTION_LOAD:
            result = menu_load_panel();
            if (result == KF_MENU_RESULT_ACCEPTED)
                result = KF_MENU_RESULT_GAME_LOADED;
            break;
        case KF_MENU_SYSTEM_ACTION_QUIT:
            result = menu_two_option_prompt(
                KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_ROW_COUNT, cursor, NULL);
            if (result == KF_MENU_RESULT_ACCEPTED) {
                menu_load_item_texture(MENU_TEXTURE_POWER_OFF);
                audio_stop_sequence_fade();
                for (;;) {
                    menu_frame_begin();
                    menu_add_frame_quad();
                    menu_draw_window(KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_ROW_COUNT, cursor, confirm);
                    menu_present_frame();
                }
            }
            break;
        default:
            break;
        }

        if (action != KF_MENU_SYSTEM_ACTION_NONE && result == KF_MENU_RESULT_CANCELLED)
            result = KF_MENU_RESULT_PENDING;
        action = KF_MENU_SYSTEM_ACTION_NONE;
        if (result != KF_MENU_RESULT_PENDING)
            break;

        confirm = KF_MENU_CONFIRM_IDLE;
        prev = input;
        input = kf::host_read_buttons();
        if (kf::button_pressed(input, prev, kf::Button::Up)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != 0)
                cursor--;
            else
                cursor = KF_MENU_SYSTEM_RETURN_ROW;
        } else if (kf::button_pressed(input, prev, kf::Button::Down)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != KF_MENU_SYSTEM_RETURN_ROW)
                cursor++;
            else
                cursor = kf_enum_encode<s32>(KF_MENU_SYSTEM_ACTION_LOAD);
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            confirm = KF_MENU_CONFIRM_REQUESTED;
            if (cursor == KF_MENU_SYSTEM_RETURN_ROW) {
                result = KF_MENU_RESULT_CANCELLED;
            } else {
                action = kf_enum_decode<KfMenuSystemAction>(cursor);
            }
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            result = KF_MENU_RESULT_CANCELLED;
        }

        menu_frame_begin();
        menu_draw_window(KF_MENU_WINDOW_SYSTEM, KF_MENU_SYSTEM_ROW_COUNT, cursor, confirm);
        menu_present_frame();
    }
    return result;
}

KfMenuResult menu_save_panel(void)
{
    KfSaveSlotSummary summaries[KF_SAVE_SLOT_COUNT];
    s32 cursor = 0;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    KfMenuResult result = KF_MENU_RESULT_PENDING;
    s32 status;
    s32 i;

    status = kf_enum_encode<s32>(save_system_read_catalog(summaries));
    if (status != kf_enum_encode<s32>(KF_SAVE_RESULT_OK) && status != kf_enum_encode<s32>(KF_SAVE_RESULT_NO_SPACE)) {
        menu_play_input_sound(MENU_SOUND_CURSOR);
        while (kf::host_read_buttons() == 0) {
            menu_frame_begin();
            menu_add_frame_quad();
            menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
            menu_draw_window(KF_MENU_WINDOW_SAVE, KF_MENU_SAVE_ROW_COUNT, cursor, confirm);
            menu_present_frame();
        }
        menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
        kf::host_wait_buttons_released();
        return KF_MENU_RESULT_CANCELLED;
    }

    for (;;) {
        if (confirm == KF_MENU_CONFIRM_REQUESTED || result == KF_MENU_RESULT_CANCELLED) {
            menu_frame_begin();
            menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
            menu_draw_window(KF_MENU_WINDOW_SAVE, KF_MENU_SAVE_ROW_COUNT, cursor, confirm);
            menu_present_frame();
            kf::host_wait_buttons_released();
        }

        if (confirm == KF_MENU_CONFIRM_REQUESTED && cursor != KF_MENU_SAVE_RETURN_ROW) {
            result = menu_two_option_prompt(KF_MENU_WINDOW_SAVE, KF_MENU_SAVE_ROW_COUNT, cursor, summaries);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                if (cursor < KF_SAVE_SLOT_COUNT) {
                    menu_load_item_texture(MENU_TEXTURE_SAVING_DATA);
                    for (i = 0; i < 3; i++) {
                        menu_frame_begin();
                        menu_add_frame_quad();
                        menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
                        menu_draw_window(KF_MENU_WINDOW_SAVE, KF_MENU_SAVE_ROW_COUNT, cursor, confirm);
                        menu_present_frame();
                    }
                    status = kf_enum_encode<s32>(save_system_write_slot(kf_enum_decode<KfSaveSlotArgument>(cursor + kf_enum_encode<s16>(KF_SAVE_SLOT_FIRST))));
                }

                if (status != kf_enum_encode<s32>(KF_SAVE_RESULT_OK)) {
                    while (kf::host_read_buttons() == 0) {
                        menu_frame_begin();
                        menu_add_frame_quad();
                        menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
                        menu_draw_window(KF_MENU_WINDOW_SAVE, KF_MENU_SAVE_ROW_COUNT, cursor, confirm);
                        menu_present_frame();
                    }
                    menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
                    kf::host_wait_buttons_released();
                    result = KF_MENU_RESULT_PENDING;
                }
            }
        }

        confirm = KF_MENU_CONFIRM_IDLE;
        if (result != KF_MENU_RESULT_PENDING)
            break;

        prev = input;
        input = kf::host_read_buttons();
        if (kf::button_pressed(input, prev, kf::Button::Up)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != 0)
                cursor--;
            else
                cursor = KF_MENU_SAVE_RETURN_ROW;
        } else if (kf::button_pressed(input, prev, kf::Button::Down)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != KF_MENU_SAVE_RETURN_ROW)
                cursor++;
            else
                cursor = 0;
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            confirm = KF_MENU_CONFIRM_REQUESTED;
            if (cursor == KF_MENU_SAVE_RETURN_ROW)
                result = KF_MENU_RESULT_CANCELLED;
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            result = KF_MENU_RESULT_CANCELLED;
        }

        menu_frame_begin();
        menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
        menu_draw_window(KF_MENU_WINDOW_SAVE, KF_MENU_SAVE_ROW_COUNT, cursor, confirm);
        menu_present_frame();
    }
    return result;
}

KfMenuResult menu_load_panel(void)
{
    KfSaveSlotSummary summaries[KF_SAVE_SLOT_COUNT];
    s32 cursor = 0;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    s32 input = 0;
    s32 prev;
    KfMenuResult result = KF_MENU_RESULT_PENDING;
    s32 i;

    if (save_system_read_catalog(summaries) != KF_SAVE_RESULT_OK) {
        menu_play_input_sound(MENU_SOUND_CURSOR);
        while (kf::host_read_buttons() == 0) {
            menu_frame_begin();
            menu_add_frame_quad();
            menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
            menu_draw_window(KF_MENU_WINDOW_LOAD, KF_MENU_LOAD_ROW_COUNT, cursor, confirm);
            menu_present_frame();
        }
        menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
        kf::host_wait_buttons_released();
        return KF_MENU_RESULT_CANCELLED;
    }

    for (;;) {
        if (confirm == KF_MENU_CONFIRM_REQUESTED || result == KF_MENU_RESULT_CANCELLED) {
            menu_frame_begin();
            menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
            menu_draw_window(KF_MENU_WINDOW_LOAD, KF_MENU_LOAD_ROW_COUNT, cursor, confirm);
            menu_present_frame();
            kf::host_wait_buttons_released();
        }

        if (confirm == KF_MENU_CONFIRM_REQUESTED && cursor != KF_MENU_LOAD_RETURN_ROW) {
            result = menu_two_option_prompt(KF_MENU_WINDOW_LOAD, KF_MENU_LOAD_ROW_COUNT, cursor, summaries);
            if (result == KF_MENU_RESULT_CANCELLED) {
                result = KF_MENU_RESULT_PENDING;
            } else {
                menu_load_item_texture(MENU_TEXTURE_LOADING_DATA);
                for (i = 0; i < 3; i++) {
                    menu_frame_begin();
                    menu_add_frame_quad();
                    menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
                    menu_draw_window(KF_MENU_WINDOW_LOAD, KF_MENU_LOAD_ROW_COUNT, cursor, confirm);
                    menu_present_frame();
                }
                if (save_system_read_slot(kf_enum_decode<KfSaveSlotArgument>(cursor + kf_enum_encode<s16>(KF_SAVE_SLOT_FIRST))) != KF_SAVE_RESULT_OK) {
                    while (kf::host_read_buttons() == 0) {
                        menu_frame_begin();
                        menu_add_frame_quad();
                        menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
                        menu_draw_window(KF_MENU_WINDOW_LOAD, KF_MENU_LOAD_ROW_COUNT, cursor, confirm);
                        menu_present_frame();
                    }
                    menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
                    kf::host_wait_buttons_released();
                    result = KF_MENU_RESULT_PENDING;
                }
            }
        }

        confirm = KF_MENU_CONFIRM_IDLE;
        if (result != KF_MENU_RESULT_PENDING)
            break;

        prev = input;
        input = kf::host_read_buttons();
        if (kf::button_pressed(input, prev, kf::Button::Up)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != 0)
                cursor--;
            else
                cursor = KF_MENU_LOAD_RETURN_ROW;
        } else if (kf::button_pressed(input, prev, kf::Button::Down)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (cursor != KF_MENU_LOAD_RETURN_ROW)
                cursor++;
            else
                cursor = 0;
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            if (cursor == KF_MENU_LOAD_RETURN_ROW) {
                menu_play_input_sound(MENU_SOUND_CONFIRM);
                confirm = KF_MENU_CONFIRM_REQUESTED;
                result = KF_MENU_RESULT_CANCELLED;
            } else if (summaries[cursor].state == KfSaveSlotState::Empty) {
                menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            } else {
                menu_play_input_sound(MENU_SOUND_CONFIRM);
                confirm = KF_MENU_CONFIRM_REQUESTED;
            }
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            result = KF_MENU_RESULT_CANCELLED;
        }

        menu_frame_begin();
        menu_draw_dialog_frame(summaries, kf_enum_decode<KfSaveSlotOverlay>(cursor));
        menu_draw_window(KF_MENU_WINDOW_LOAD, KF_MENU_LOAD_ROW_COUNT, cursor, confirm);
        menu_present_frame();
    }
    return result;
}

enum {
    CONFIG_OPTION_ON_X = 180,
    CONFIG_OPTION_OFF_X = 240,
    CONFIG_OPTION_FIRST_Y = 41,
    CONFIG_OPTION_ROW_STEP = 22
};

void menu_config_panel_draw(
    MenuGlyphString on_label, MenuGlyphString off_label, KfPlayerOption *option_states);

// Copies a label sequence up to its end marker.
static void menu_copy_label(s16 *codes, const s16 *sequence)
{
    s32 i = 0;
    for (; i < MENU_GLYPHS_PER_ROW - 1 && sequence[i] != MENU_TEXT_END; i++)
        codes[i] = sequence[i];
    codes[i] = MENU_TEXT_END;
}

void menu_config_panel(void)
{
    KfPlayerOption states[KF_MENU_CONFIG_SETTING_COUNT];
    MenuGlyphString option_a;
    MenuGlyphString option_b;
    s32 row = 0;
    KfMenuConfirmState confirm = KF_MENU_CONFIRM_IDLE;
    u32 pad = 0;
    u32 prev;
    KfMenuResult phase = KF_MENU_RESULT_PENDING;
    KfPlayerOption music_orig;
    // Port addition: the language row switches between Japanese and English at
    // once; without the other language's files it says so (F1 can add them).
    bool language_missing = false;
    const auto change_option = [&] {
        if (row == KF_MENU_CONFIG_LANGUAGE_ROW) {
            const auto target = kf::game_language() == kf::Language::Japanese
                ? kf::Language::English : kf::Language::Japanese;
            language_missing = !kf::language_available(target);
            if (!language_missing) {
                kf::language_request(target);
                language_missing = !game_apply_language();
            }
            menu_play_input_sound(language_missing ? MENU_SOUND_CANCEL_OR_ERROR : MENU_SOUND_CONFIRM);
        } else if (row < KF_MENU_CONFIG_SETTING_COUNT) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            states[row] = kf_enum_decode<KfPlayerOption>(states[row] == KF_PLAYER_OPTION_OFF);
        }
    };

    kf::host_wait_buttons_released();

    option_a.position.x = CONFIG_OPTION_ON_X;
    option_a.position.y = CONFIG_OPTION_FIRST_Y;
    option_a.glyphs.codes[0] = menu_glyphs::on[0];
    option_a.glyphs.codes[1] = menu_glyphs::on[1];
    option_a.glyphs.codes[2] = MENU_TEXT_END;
    option_b.position.x = CONFIG_OPTION_OFF_X;
    option_b.position.y = CONFIG_OPTION_FIRST_Y;
    option_b.glyphs.codes[0] = menu_glyphs::off[0];
    option_b.glyphs.codes[1] = menu_glyphs::off[1];
    option_b.glyphs.codes[2] = menu_glyphs::off[2];
    option_b.glyphs.codes[3] = MENU_TEXT_END;
    states[KF_MENU_CONFIG_EFFECTS_ROW] = player_state.audio_effects_enabled;
    states[KF_MENU_CONFIG_MUSIC_ROW] = player_state.audio_music_enabled;
    states[KF_MENU_CONFIG_GAUGES_ROW] = player_state.hud_gauges_enabled;
    states[KF_MENU_CONFIG_COMPASS_ROW] = player_state.compass_enabled;
    music_orig = states[KF_MENU_CONFIG_MUSIC_ROW];

    menu_frame_begin();
    menu_config_panel_draw(option_a, option_b, states);
    menu_draw_window(KF_MENU_WINDOW_CONFIG, KF_MENU_CONFIG_ROW_COUNT, row, confirm);
    menu_present_frame();
    do {
        // A change requested from the F1 overlay while this menu is open.
        if (game_apply_language()) {
            language_missing = false;
            option_a.glyphs.codes[0] = menu_glyphs::on[0];
            option_a.glyphs.codes[1] = menu_glyphs::on[1];
            option_b.glyphs.codes[0] = menu_glyphs::off[0];
            option_b.glyphs.codes[1] = menu_glyphs::off[1];
            option_b.glyphs.codes[2] = menu_glyphs::off[2];
        }
        if (confirm == KF_MENU_CONFIRM_REQUESTED || phase == KF_MENU_RESULT_CANCELLED) {
            menu_frame_begin();
            menu_config_panel_draw(option_a, option_b, states);
            menu_draw_window(KF_MENU_WINDOW_CONFIG, KF_MENU_CONFIG_ROW_COUNT, row, confirm);
            menu_present_frame();
            kf::host_wait_buttons_released();
        }
        if (phase != KF_MENU_RESULT_PENDING) {
            break;
        }
        confirm = KF_MENU_CONFIRM_IDLE;
        menu_frame_begin();
        prev = pad;
        pad = kf::host_read_buttons();
        if (kf::button_pressed(pad, prev, kf::Button::Up)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (row != 0) {
                row--;
            } else {
                row = KF_MENU_CONFIG_RETURN_ROW;
            }
        } else if (kf::button_pressed(pad, prev, kf::Button::Down)) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (row != KF_MENU_CONFIG_RETURN_ROW) {
                row++;
            } else {
                row = 0;
            }
        } else if ((kf::button_pressed(pad, prev, kf::Button::Right)) ||
                   (kf::button_pressed(pad, prev, kf::Button::Left))) {
            change_option();
        } else if (kf::button_pressed(pad, prev, kf::Button::Confirm)) {
            if (row == KF_MENU_CONFIG_RETURN_ROW) {
                menu_play_input_sound(MENU_SOUND_CONFIRM);
                confirm = KF_MENU_CONFIRM_REQUESTED;
                phase = KF_MENU_RESULT_CANCELLED;
            } else {
                change_option();
            }
        } else if (kf::button_pressed(pad, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            phase = KF_MENU_RESULT_CANCELLED;
        }
        menu_config_panel_draw(option_a, option_b, states);
        menu_draw_window(KF_MENU_WINDOW_CONFIG, KF_MENU_CONFIG_ROW_COUNT, row, confirm);
        if (language_missing) {
            MenuGlyphString message;
            message.position = menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_CONFIG)]
                                   .rows[KF_MENU_CONFIG_LANGUAGE_ROW].position;
            message.position.x = CONFIG_OPTION_ON_X;
            message.position.y += 16;
            menu_copy_label(message.glyphs.codes, menu_glyphs::language_unavailable);
            menu_draw_string(&menu_assets.glyph_atlas, &message);
        }
        menu_present_frame();
    } while (1);

    player_state.audio_effects_enabled = states[KF_MENU_CONFIG_EFFECTS_ROW];
    player_state.audio_music_enabled = states[KF_MENU_CONFIG_MUSIC_ROW];
    player_state.hud_gauges_enabled = states[KF_MENU_CONFIG_GAUGES_ROW];
    player_state.compass_enabled = states[KF_MENU_CONFIG_COMPASS_ROW];
    if (player_state.audio_music_enabled != music_orig) {
        if (player_state.audio_music_enabled == KF_PLAYER_OPTION_OFF) {
            audio_stop_sequence_fade();
        } else {
            audio_play_current_map_sequence();
        }
    }
}

void menu_config_panel_draw(
    MenuGlyphString on_label, MenuGlyphString off_label, KfPlayerOption *option_states)
{
    s32 i;
    KfPlayerOption *states;
    const MenuSpriteDef *box_b;

    states = option_states;
    for (i = 0; i < KF_MENU_CONFIG_SETTING_COUNT; i++) {
        if (*states == KF_PLAYER_OPTION_ON) {
            menu_blit_sprite_translucent(&menu_assets.option_highlight, &on_label.position);
            box_b = &menu_assets.option_background;
        } else {
            menu_blit_sprite_translucent(&menu_assets.option_background, &on_label.position);
            box_b = &menu_assets.option_highlight;
        }
        menu_blit_sprite_translucent(box_b, &off_label.position);
        menu_draw_string(
            &menu_assets.glyph_atlas,
            &on_label);
        menu_draw_string(
            &menu_assets.glyph_atlas,
            &off_label);
        states++;
        on_label.position.y += CONFIG_OPTION_ROW_STEP;
        off_label.position.y += CONFIG_OPTION_ROW_STEP;
    }
    menu_enqueue_background();
}

enum {
    STATS_HEADER_ROW_STEP = 23
};

// Copies codes[1..] through the terminator; codes[0] keeps the preceding row's value.
static void menu_copy_row_tail(MenuGlyphString &string, const s16 *sequence)
{
    for (s32 i = 1; i < MENU_GLYPHS_PER_ROW; i++) {
        string.glyphs.codes[i] = sequence[i];
        if (sequence[i] == MENU_TEXT_END)
            break;
    }
}

static void menu_draw_vital_fraction(MenuGlyphString &string, s32 current, s32 maximum, s32 digits)
{
    menu_format_number(current, digits, KF_FORMAT_PAD_SPACES, string.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &string);
    string.glyphs.codes[0] = MENU_NUMBER_SLASH;
    string.glyphs.codes[1] = MENU_TEXT_END;
    string.position.x += digits * MENU_NUMBER_ADVANCE;
    menu_draw_number(&menu_assets.number_atlas, &string);
    string.position.x += MENU_NUMBER_ADVANCE;
    menu_format_number(maximum, digits, KF_FORMAT_PAD_SPACES, string.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &string);
}

void menu_draw_stats_header(void)
{
    MenuGlyphString gs;
    s32 glyph_index;
    s32 row_step = STATS_HEADER_ROW_STEP;

    gs.position.x = 0xb5;
    gs.position.y = 0x24;
    gs.glyphs.codes[0] = menu_glyphs::experience[0];
    gs.glyphs.codes[1] = menu_glyphs::experience[1];
    gs.glyphs.codes[2] = menu_glyphs::experience[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::level[0];
    gs.glyphs.codes[1] = menu_glyphs::level[1];
    gs.glyphs.codes[2] = menu_glyphs::level[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::character_class[0];
    gs.glyphs.codes[1] = menu_glyphs::character_class[1];
    gs.glyphs.codes[2] = menu_glyphs::character_class[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::floor[0];
    gs.glyphs.codes[1] = menu_glyphs::floor[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::hp[0];
    gs.glyphs.codes[1] = menu_glyphs::hp[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::mp[0];
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::status[0];
    gs.glyphs.codes[1] = menu_glyphs::status[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::gold[0];
    gs.glyphs.codes[1] = menu_glyphs::gold[1];
    gs.glyphs.codes[2] = menu_glyphs::gold[2];
    gs.glyphs.codes[3] = menu_glyphs::gold[3];
    gs.glyphs.codes[4] = MENU_TEXT_END;
    gs.position.y += row_step;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = 0xfb;
    gs.position.y = 0x24;
    menu_format_number(player_state.experience, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    gs.position.y += row_step;
    menu_format_number(player_state.progress_state.level, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    gs.position.x = 0xed;
    gs.position.y += row_step;
    if (player_state.base_magic < MENU_CLASS_MIDDLE_STAT_MIN) {
        glyph_index = 0;
    } else {
        glyph_index = 2;
        if (player_state.base_magic < MENU_CLASS_HIGH_STAT_MIN) {
            glyph_index = 1;
        }
    }
    if (player_state.base_physical_power > (MENU_CLASS_MIDDLE_STAT_MIN - 1)) {
        if (player_state.base_physical_power < MENU_CLASS_HIGH_STAT_MIN) {
            glyph_index += MENU_CLASS_MAGIC_TIER_COUNT;
        } else {
            glyph_index += 2 * MENU_CLASS_MAGIC_TIER_COUNT;
        }
    }
    glyph_index *= MENU_CLASS_LABEL_GLYPHS;
    gs.glyphs.codes[0] = glyph_index + MENU_CLASS_FIRST_GLYPH;
    gs.glyphs.codes[1] = glyph_index + MENU_CLASS_FIRST_GLYPH + 1;
    gs.glyphs.codes[2] = glyph_index + MENU_CLASS_FIRST_GLYPH + 2;
    gs.glyphs.codes[3] = glyph_index + MENU_CLASS_FIRST_GLYPH + 3;
    gs.glyphs.codes[4] = MENU_TEXT_END;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = 0xfb;
    gs.position.y += row_step;
    menu_format_number(kf_enum_encode<u8>(player_state.progress_state.current_floor), MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    gs.position.x = 0xe6;
    gs.position.y += row_step;
    menu_draw_vital_fraction(gs, player_state.vitals.current_hp,
        player_state.vitals.maximum_hp, MENU_STATS_VITAL_DIGITS);

    gs.position.x = 0xe6;
    gs.position.y += row_step;
    menu_draw_vital_fraction(gs, player_state.vitals.current_mp,
        player_state.vitals.maximum_mp, MENU_STATS_VITAL_DIGITS);

    gs.position.x = 0xdf;
    gs.glyphs.codes[0] = MENU_TEXT_BLANK;
    gs.glyphs.codes[1] = MENU_TEXT_BLANK;
    gs.glyphs.codes[2] = MENU_TEXT_BLANK;
    gs.glyphs.codes[3] = MENU_TEXT_BLANK;
    gs.glyphs.codes[4] = MENU_TEXT_BLANK;
    gs.glyphs.codes[5] = MENU_TEXT_END;
    gs.position.y += row_step;
    if (player_state.status_effect_flags == KF_PLAYER_STATUS_NONE) {
        gs.glyphs.codes[3] = menu_glyphs::healthy[0];
        gs.glyphs.codes[4] = menu_glyphs::healthy[1];
    }
    glyph_index = 4;
    if ((player_state.status_effect_flags & KF_PLAYER_STATUS_SLOWED) != KF_PLAYER_STATUS_NONE) {
        gs.glyphs.codes[4] = menu_glyphs::slowed[0];
        glyph_index = 3;
    }
    if ((player_state.status_effect_flags & KF_PLAYER_STATUS_POISON) != KF_PLAYER_STATUS_NONE) {
        gs.glyphs.codes[glyph_index] = menu_glyphs::poison[0];
        glyph_index--;
    }
    if ((player_state.status_effect_flags & KF_PLAYER_STATUS_DARKNESS) != KF_PLAYER_STATUS_NONE) {
        gs.glyphs.codes[glyph_index] = menu_glyphs::darkness[0];
        glyph_index--;
    }
    if ((player_state.status_effect_flags & KF_PLAYER_STATUS_CURSE) != KF_PLAYER_STATUS_NONE) {
        gs.glyphs.codes[glyph_index] = menu_glyphs::curse[0];
    }
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = 0xfb;
    gs.position.y += row_step;
    menu_format_number(player_state.gold, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
}

enum {
    STATUS_SUMMARY_ROW_STEP = 16,
    STATUS_COMPONENT_ROW_STEP = 14
};

enum {
    STATUS_PHYSICAL_ATTACK_MULTIPLIER = 3,
    STATUS_PHYSICAL_ATTACK_DOWNSHIFT = 1,
    STATUS_ELEMENTAL_ATTACK_MULTIPLIER = 2,
    STATUS_ATTACK_SCALE_NUMERATOR = 10,
    STATUS_ATTACK_SCALE_DENOMINATOR = 8,
    STATUS_POISON_RESISTANCE_DIVISOR = 5,
    STATUS_DEFENSE_SCALE_NUMERATOR = 10,
    STATUS_DEFENSE_SCALE_DENOMINATOR = 7
};

void menu_draw_status_details(void)
{
    MenuGlyphString gs;
    s32 glyph_index;
    s32 rating;
    s32 summary_y_origin;

    summary_y_origin = 0x23;
    gs.position.x = 0x15;
    gs.position.y = summary_y_origin;
    gs.glyphs.codes[0] = menu_glyphs::experience[0];
    gs.glyphs.codes[1] = menu_glyphs::experience[1];
    gs.glyphs.codes[2] = menu_glyphs::experience[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::level_summary[0];
    gs.glyphs.codes[1] = menu_glyphs::level_summary[1];
    gs.glyphs.codes[2] = menu_glyphs::level_summary[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::character_class[0];
    gs.glyphs.codes[1] = menu_glyphs::character_class[1];
    gs.glyphs.codes[2] = menu_glyphs::character_class[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::hp[0];
    gs.glyphs.codes[1] = menu_glyphs::hp[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::mp[0];
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::status[0];
    gs.glyphs.codes[1] = menu_glyphs::status[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::gold_summary[0];
    gs.glyphs.codes[1] = menu_glyphs::gold_summary[1];
    gs.glyphs.codes[2] = menu_glyphs::gold_summary[2];
    gs.glyphs.codes[3] = menu_glyphs::gold_summary[3];
    gs.glyphs.codes[4] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::physical_power[0];
    gs.glyphs.codes[1] = menu_glyphs::physical_power[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::magic_power[0];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::total_attack[0];
    gs.glyphs.codes[1] = menu_glyphs::total_attack[1];
    gs.glyphs.codes[2] = menu_glyphs::total_attack[2];
    gs.glyphs.codes[3] = menu_glyphs::total_attack[3];
    gs.glyphs.codes[4] = menu_glyphs::total_attack[4];
    gs.glyphs.codes[5] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[2] = menu_glyphs::total_defense[2];
    gs.glyphs.codes[3] = menu_glyphs::total_defense[3];
    gs.glyphs.codes[5] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = 0x5b;
    gs.position.y = summary_y_origin;
    menu_format_number(player_state.experience, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_format_number(player_state.progress_state.level, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    gs.position.x = 0x4d;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    {
        s32 summary_row_step = STATUS_SUMMARY_ROW_STEP;

        if (player_state.base_magic < MENU_CLASS_MIDDLE_STAT_MIN) {
            glyph_index = 0;
        } else {
            glyph_index = 2;
            if (player_state.base_magic < MENU_CLASS_HIGH_STAT_MIN) {
                glyph_index = 1;
            }
        }
        if (player_state.base_physical_power > (MENU_CLASS_MIDDLE_STAT_MIN - 1)) {
            if (player_state.base_physical_power < MENU_CLASS_HIGH_STAT_MIN) {
                glyph_index += MENU_CLASS_MAGIC_TIER_COUNT;
            } else {
                glyph_index += 2 * MENU_CLASS_MAGIC_TIER_COUNT;
            }
        }
        glyph_index *= MENU_CLASS_LABEL_GLYPHS;
        gs.glyphs.codes[0] = glyph_index + MENU_CLASS_FIRST_GLYPH;
        gs.glyphs.codes[1] = glyph_index + MENU_CLASS_FIRST_GLYPH + 1;
        gs.glyphs.codes[2] = glyph_index + MENU_CLASS_FIRST_GLYPH + 2;
        gs.glyphs.codes[3] = glyph_index + MENU_CLASS_FIRST_GLYPH + 3;
        gs.glyphs.codes[4] = MENU_TEXT_END;
        menu_draw_string(&menu_assets.glyph_atlas, &gs);

        gs.position.x = 0x46;
        gs.position.y += summary_row_step;
        menu_draw_vital_fraction(gs, player_state.vitals.current_hp,
            player_state.vitals.maximum_hp, MENU_STATS_VITAL_DIGITS);

        gs.position.x = 0x46;
        gs.position.y += summary_row_step;
        menu_draw_vital_fraction(gs, player_state.vitals.current_mp,
            player_state.vitals.maximum_mp, MENU_STATS_VITAL_DIGITS);

        gs.position.x = 0x3f;
        gs.glyphs.codes[0] = MENU_TEXT_BLANK;
        gs.glyphs.codes[1] = MENU_TEXT_BLANK;
        gs.glyphs.codes[2] = MENU_TEXT_BLANK;
        gs.glyphs.codes[3] = MENU_TEXT_BLANK;
        gs.glyphs.codes[4] = MENU_TEXT_BLANK;
        gs.glyphs.codes[5] = MENU_TEXT_END;
        gs.position.y += summary_row_step;
        if (player_state.status_effect_flags == KF_PLAYER_STATUS_NONE) {
            gs.glyphs.codes[3] = menu_glyphs::healthy[0];
            gs.glyphs.codes[4] = menu_glyphs::healthy[1];
        }
        glyph_index = 4;
        if ((player_state.status_effect_flags & KF_PLAYER_STATUS_SLOWED) != KF_PLAYER_STATUS_NONE) {
            gs.glyphs.codes[4] = menu_glyphs::slowed[0];
            glyph_index = 3;
        }
        if ((player_state.status_effect_flags & KF_PLAYER_STATUS_POISON) != KF_PLAYER_STATUS_NONE) {
            gs.glyphs.codes[glyph_index] = menu_glyphs::poison[0];
            glyph_index--;
        }
        if ((player_state.status_effect_flags & KF_PLAYER_STATUS_DARKNESS) != KF_PLAYER_STATUS_NONE) {
            gs.glyphs.codes[glyph_index] = menu_glyphs::darkness[0];
            glyph_index--;
        }
        if ((player_state.status_effect_flags & KF_PLAYER_STATUS_CURSE) != KF_PLAYER_STATUS_NONE) {
            gs.glyphs.codes[glyph_index] = menu_glyphs::curse[0];
        }
        menu_draw_string(&menu_assets.glyph_atlas, &gs);

        gs.position.x = 0x5b;
        gs.position.y += summary_row_step;
        menu_format_number(player_state.gold, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
        menu_draw_number(&menu_assets.number_atlas, &gs);
        gs.position.y += summary_row_step;
        menu_format_number(player_state.physical_power, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
        menu_draw_number(&menu_assets.number_atlas, &gs);
        gs.position.y += summary_row_step;
        menu_format_number(player_state.magic, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
        menu_draw_number(&menu_assets.number_atlas, &gs);
        rating = (((u32)player_state.cutting_attack + player_state.striking_attack +
                   player_state.piercing_attack) * STATUS_PHYSICAL_ATTACK_MULTIPLIER
                   >> STATUS_PHYSICAL_ATTACK_DOWNSHIFT) +
                 (player_state.holy_attack + player_state.fire_attack)
                 * STATUS_ELEMENTAL_ATTACK_MULTIPLIER;
        rating = rating * STATUS_ATTACK_SCALE_NUMERATOR
            / STATUS_ATTACK_SCALE_DENOMINATOR;
        gs.position.y += summary_row_step;
        menu_format_number(rating, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
        menu_draw_number(&menu_assets.number_atlas, &gs);
        rating = player_state.cutting_defense + player_state.striking_defense +
                 player_state.piercing_defense + player_state.poison_resistance / STATUS_POISON_RESISTANCE_DIVISOR +
                 player_state.magic_defense + player_state.fire_defense;
        rating = rating * STATUS_DEFENSE_SCALE_NUMERATOR / STATUS_DEFENSE_SCALE_DENOMINATOR;
        gs.position.y += summary_row_step;
        menu_format_number(rating, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
        menu_draw_number(&menu_assets.number_atlas, &gs);
    }
    gs.position.x = 0xb5;
    gs.position.y = 0x1e;
    gs.glyphs.codes[0] = menu_glyphs::attack[0];
    gs.glyphs.codes[1] = menu_glyphs::attack[1];
    gs.glyphs.codes[2] = menu_glyphs::attack[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = MENU_TEXT_BLANK;
    gs.glyphs.codes[1] = menu_glyphs::cutting[1];
    gs.glyphs.codes[2] = menu_glyphs::cutting[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[1] = menu_glyphs::striking[1];
    gs.glyphs.codes[2] = menu_glyphs::striking[2];
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[1] = menu_glyphs::piercing[1];
    gs.glyphs.codes[2] = menu_glyphs::piercing[2];
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    menu_copy_row_tail(gs, menu_glyphs::holy);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[1] = menu_glyphs::fire[1];
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = menu_glyphs::defense[0];
    gs.glyphs.codes[1] = menu_glyphs::defense[1];
    gs.glyphs.codes[2] = menu_glyphs::defense[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += STATUS_SUMMARY_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[0] = MENU_TEXT_BLANK;
    gs.glyphs.codes[1] = menu_glyphs::cutting[1];
    gs.glyphs.codes[2] = menu_glyphs::cutting[2];
    gs.glyphs.codes[3] = MENU_TEXT_END;
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[1] = menu_glyphs::striking[1];
    gs.glyphs.codes[2] = menu_glyphs::striking[2];
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[1] = menu_glyphs::piercing[1];
    gs.glyphs.codes[2] = menu_glyphs::piercing[2];
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    menu_copy_row_tail(gs, menu_glyphs::poison_resistance);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    menu_copy_row_tail(gs, menu_glyphs::magic_defense);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.glyphs.codes[1] = menu_glyphs::fire[1];
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = 0xfb;
    gs.position.y = 0x2c;
    menu_format_number(player_state.cutting_attack, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.striking_attack, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.piercing_attack, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.holy_attack, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.fire_attack, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += 0x1e;
    menu_format_number(player_state.cutting_defense, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.striking_defense, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.piercing_defense, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.poison_resistance, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.magic_defense, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    gs.position.y += STATUS_COMPONENT_ROW_STEP;
    menu_format_number(player_state.fire_defense, MENU_STATS_VALUE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
}

enum {
    EQUIPMENT_NAME_FIRST_Y = 40,
    EQUIPMENT_NAME_ROW_STEP = 20
};

void menu_draw_name_list(void)
{
    MenuGlyphString gs;

    gs.position.x = MENU_ITEM_NAME_X;
    gs.position.y = EQUIPMENT_NAME_FIRST_Y;
    if (player_state.equipped_weapon_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_weapon_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.selected_magic_id != KF_MAGIC_NONE) {
        gs.glyphs =
            magic_name_rows[kf_enum_encode<u8>(player_state.selected_magic_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.equipped_shield_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_shield_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.equipped_head_armor_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_head_armor_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.equipped_body_armor_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_body_armor_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.equipped_arm_armor_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_arm_armor_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.equipped_leg_armor_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_leg_armor_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
    gs.position.y += EQUIPMENT_NAME_ROW_STEP;
    if (player_state.equipped_accessory_id != KF_OBJECT_NONE) {
        gs.glyphs =
            item_name_rows[kf_enum_encode<u8>(player_state.equipped_accessory_id)];
        menu_draw_string(&menu_assets.glyph_atlas, &gs);
    }
}

enum {
    MENU_INVENTORY_QUANTITY_LABEL_X = 230,
    MENU_INVENTORY_QUANTITY_VALUE_X = 279
};

// PC extension: equipment stats beside the item preview (inventory, equip and
// shop screens). While an equip screen is open, rows show the current total and
// the total with the highlighted item, tinted by whether it goes up or down.
namespace {
enum class MenuItemSlot { None, Weapon, Head, Body, Shield, Arm, Leg, Accessory };
KfEquipmentMenuCategory menu_stats_compare = KF_EQUIP_MENU_NONE;

enum {
    MENU_STATS_LABEL_X = 174,
    MENU_STATS_VALUE_X = 206,
    MENU_STATS_NEW_VALUE_X = 232,
    MENU_STATS_ROW_STEP = 13,
    MENU_STATS_DIGITS = 3,
    MENU_STATS_LABEL_CELLS = 2
};
constexpr s32 MENU_STATS_MODEL_X = 1624;
constexpr s32 MENU_STATS_MODEL_Y = 600;
constexpr s32 MENU_STATS_MODEL_Z = 2800;
constexpr s32 menu_preview_translation[3] = {
    MENU_ITEM_PREVIEW_TRANSLATION_X, MENU_ITEM_PREVIEW_TRANSLATION_Y, MENU_ITEM_PREVIEW_TRANSLATION_Z};

MenuItemSlot menu_item_slot(KfObjectId id)
{
    const s32 code = kf_enum_encode<u8>(id);
    auto in = [code](KfObjectId first, KfObjectId end) {
        return code >= kf_enum_encode<u8>(first) && code < kf_enum_encode<u8>(end);
    };
    if (id == KF_OBJECT_NONE)
        return MenuItemSlot::None;
    if (in(KF_ITEM_SHORT_SWORD, KF_ITEM_IRON_MASK)) return MenuItemSlot::Weapon;
    if (in(KF_ITEM_IRON_MASK, KF_ITEM_BREASTPLATE)) return MenuItemSlot::Head;
    if (in(KF_ITEM_BREASTPLATE, KF_ITEM_SMALL_SHIELD)) return MenuItemSlot::Body;
    if (in(KF_ITEM_SMALL_SHIELD, KF_ITEM_GAUNTLET)) return MenuItemSlot::Shield;
    if (in(KF_ITEM_GAUNTLET, KF_ITEM_IRON_BOOTS)) return MenuItemSlot::Arm;
    if (in(KF_ITEM_IRON_BOOTS, KF_ITEM_GOLD_COIN)) return MenuItemSlot::Leg;
    if (code >= kf_enum_encode<u8>(KF_ITEM_LIGHT_RING) && code <= kf_enum_encode<u8>(KF_ITEM_GOLD_CROSS))
        return MenuItemSlot::Accessory;
    return MenuItemSlot::None;
}

MenuItemSlot menu_category_slot(KfEquipmentMenuCategory category)
{
    switch (category) {
    case KF_EQUIP_MENU_WEAPON: return MenuItemSlot::Weapon;
    case KF_EQUIP_MENU_SHIELD: return MenuItemSlot::Shield;
    case KF_EQUIP_MENU_HEAD: return MenuItemSlot::Head;
    case KF_EQUIP_MENU_BODY: return MenuItemSlot::Body;
    case KF_EQUIP_MENU_ARM: return MenuItemSlot::Arm;
    case KF_EQUIP_MENU_LEG: return MenuItemSlot::Leg;
    case KF_EQUIP_MENU_ACCESSORY: return MenuItemSlot::Accessory;
    default: return MenuItemSlot::None;
    }
}

// Totals with `id` (or nothing) in `slot`, by the game's own formula.
KfPlayerState menu_stats_with(MenuItemSlot slot, KfObjectId id)
{
    KfPlayerState p = player_state;
    switch (slot) {
    case MenuItemSlot::Weapon: p.equipped_weapon_id = id; break;
    case MenuItemSlot::Head: p.equipped_head_armor_id = id; break;
    case MenuItemSlot::Body: p.equipped_body_armor_id = id; break;
    case MenuItemSlot::Shield: p.equipped_shield_id = id; break;
    case MenuItemSlot::Arm: p.equipped_arm_armor_id = id; break;
    case MenuItemSlot::Leg: p.equipped_leg_armor_id = id; break;
    case MenuItemSlot::Accessory: p.equipped_accessory_id = id; break;
    case MenuItemSlot::None: break;
    }
    player_compute_combat_stats(p);
    return p;
}

enum class MenuStatGroup { Attack, Defense, Power };
struct MenuStat {
    const s16 *const *label;   // a menu_glyphs sequence, switched with the language
    MenuStatGroup group;
    s32 (*value)(const KfPlayerState &);
};

const MenuStat menu_stats[] = {
    {&menu_glyphs::cutting, MenuStatGroup::Attack, [](const KfPlayerState &p) -> s32 { return p.cutting_attack; }},
    {&menu_glyphs::striking, MenuStatGroup::Attack, [](const KfPlayerState &p) -> s32 { return p.striking_attack; }},
    {&menu_glyphs::piercing, MenuStatGroup::Attack, [](const KfPlayerState &p) -> s32 { return p.piercing_attack; }},
    {&menu_glyphs::holy, MenuStatGroup::Attack, [](const KfPlayerState &p) -> s32 { return p.holy_attack; }},
    {&menu_glyphs::fire, MenuStatGroup::Attack, [](const KfPlayerState &p) -> s32 { return p.fire_attack; }},
    {&menu_glyphs::cutting, MenuStatGroup::Defense, [](const KfPlayerState &p) -> s32 { return p.cutting_defense; }},
    {&menu_glyphs::striking, MenuStatGroup::Defense, [](const KfPlayerState &p) -> s32 { return p.striking_defense; }},
    {&menu_glyphs::piercing, MenuStatGroup::Defense, [](const KfPlayerState &p) -> s32 { return p.piercing_defense; }},
    {&menu_glyphs::poison_resistance, MenuStatGroup::Defense, [](const KfPlayerState &p) -> s32 { return p.poison_resistance; }},
    {&menu_glyphs::magic_defense, MenuStatGroup::Defense, [](const KfPlayerState &p) -> s32 { return p.magic_defense; }},
    {&menu_glyphs::fire, MenuStatGroup::Defense, [](const KfPlayerState &p) -> s32 { return p.fire_defense; }},
    {&menu_glyphs::physical_power, MenuStatGroup::Power, [](const KfPlayerState &p) -> s32 { return p.physical_power; }},
    {&menu_glyphs::magic_power, MenuStatGroup::Power, [](const KfPlayerState &p) -> s32 { return p.magic; }},
};

// The first word of a status-screen label: the English patch pads some labels
// with blanks and a shared suffix that would not fit beside the preview.
void menu_stats_label(MenuGlyphString &gs, const s16 *sequence)
{
    s32 count = 0;
    for (s32 i = 0; i < MENU_GLYPHS_PER_ROW && sequence[i] != MENU_TEXT_END && count < MENU_STATS_LABEL_CELLS; i++) {
        if (sequence[i] == MENU_TEXT_BLANK) {
            if (count > 0)
                break;
            continue;
        }
        gs.glyphs.codes[count++] = sequence[i];
    }
    gs.glyphs.codes[count] = MENU_TEXT_END;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);
}
}

// Multiplies the colour of numbers drawn by menu_draw_number.
static float menu_number_tint[3] = {1.0f, 1.0f, 1.0f};

namespace {
void menu_stats_number(MenuGlyphString &gs, s32 x, s32 value, s32 compare_to, bool compare, bool dim = false)
{
    gs.position.x = x;
    if (!compare && dim) {
        menu_number_tint[0] = menu_number_tint[1] = menu_number_tint[2] = 0.72f;
    } else if (compare && value > compare_to) {
        menu_number_tint[0] = 0.55f; menu_number_tint[1] = 1.35f; menu_number_tint[2] = 0.55f;
    } else if (compare && value < compare_to) {
        menu_number_tint[0] = 1.45f; menu_number_tint[1] = 0.5f; menu_number_tint[2] = 0.45f;
    }
    menu_format_number(value < 0 ? 0 : value, MENU_STATS_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
    menu_number_tint[0] = menu_number_tint[1] = menu_number_tint[2] = 1.0f;
}

bool menu_stats_available(KfObjectId item_id)
{
    return menu_category_slot(menu_stats_compare) != MenuItemSlot::None || menu_item_slot(item_id) != MenuItemSlot::None;
}

// Draws the rows from y down; returns false when the item has nothing to show.
bool menu_draw_item_stats(KfObjectId item_id, s32 y)
{
    const MenuItemSlot compare_slot = menu_category_slot(menu_stats_compare);
    const bool compare = compare_slot != MenuItemSlot::None;
    const MenuItemSlot slot = compare ? compare_slot : menu_item_slot(item_id);
    if (slot == MenuItemSlot::None)
        return false;
    // One item: what it adds (totals with it minus without it). Compare: totals now and with it.
    const KfPlayerState before = compare ? menu_stats_with(MenuItemSlot::None, KF_OBJECT_NONE)
                                         : menu_stats_with(slot, KF_OBJECT_NONE);
    const KfPlayerState after = menu_stats_with(slot, item_id);
    const MenuStatGroup group = slot == MenuItemSlot::Weapon ? MenuStatGroup::Attack : MenuStatGroup::Defense;

    MenuGlyphString gs;
    gs.position.x = MENU_STATS_LABEL_X;
    gs.position.y = y;
    bool any = false;
    for (const auto &stat : menu_stats) {
        const s32 now = stat.value(before), with = stat.value(after);
        bool shown = now != with;
        // Weapons and armour in the equip screen list their whole group, so
        // the totals can be read even where this item adds nothing.
        if (compare && stat.group == group && slot != MenuItemSlot::Accessory)
            shown = now != 0 || with != 0;
        if (!shown)
            continue;
        if (!any) {
            menu_stats_label(gs, group == MenuStatGroup::Attack ? menu_glyphs::attack : menu_glyphs::defense);
            gs.position.y += MENU_STATS_ROW_STEP + 2;
            any = true;
        }
        gs.position.x = MENU_STATS_LABEL_X;
        menu_stats_label(gs, *stat.label);
        if (compare) {
            menu_stats_number(gs, MENU_STATS_VALUE_X, now, now, false, true);   // current, dimmed
            menu_stats_number(gs, MENU_STATS_NEW_VALUE_X, with, now, true);
        } else {
            menu_stats_number(gs, MENU_STATS_VALUE_X, with - now, 0, false);
        }
        gs.position.y += MENU_STATS_ROW_STEP;
    }
    return any;
}
}

void menu_set_equipment_compare(KfEquipmentMenuCategory category)
{
    menu_stats_compare = category;
}

void menu_item_model_preview(KfObjectId item_id)
{
    MenuGlyphString gs;
    MATRIX rot;
    MATRIX lsrc;
    MATRIX lres;
    const MenuGlyphRow *rows;
    const MenuGlyphRow *name;
    s32 i;

    // Stats share the panel: the model moves right and a little further away.
    const bool stats = menu_stats_available(item_id)
        && menu_draw_item_stats(item_id, MENU_ITEM_PREVIEW_NAME_Y + 2 * MENU_ITEM_PREVIEW_LINE_HEIGHT + 2);
    if (item_id != KF_OBJECT_NONE) {
        rot.t[0] = stats ? MENU_STATS_MODEL_X : menu_preview_translation[0];
        rot.t[1] = stats ? MENU_STATS_MODEL_Y : menu_preview_translation[1];
        rot.t[2] = stats ? MENU_STATS_MODEL_Z : menu_preview_translation[2];
        menu_item_preview_rotation.vy =
            (menu_item_preview_rotation.vy + MENU_ITEM_PREVIEW_YAW_STEP)
            & KF_ANGLE_WRAP_MASK;
        kf::matrix_set_rotation_xyz(menu_item_preview_rotation, rot);

        menu_preview_light_source(&lsrc);
        kf::matrix_multiply_rotation(lsrc, rot, lres);
        menu_render_item_model(&lres, &rot);

        rows = item_name_rows;
        name = &rows[kf_enum_encode<s32>(item_id)];

        gs.position.x = MENU_ITEM_NAME_X;
        gs.position.y = MENU_ITEM_PREVIEW_NAME_Y;
        for (i = 0; i < MENU_GLYPHS_PER_ROW; i++) {
            gs.glyphs.codes[i] = name->codes[i];
        }
        menu_draw_string(&menu_assets.glyph_atlas, &gs);

        gs.position.x = MENU_INVENTORY_QUANTITY_LABEL_X;
        gs.glyphs.codes[0] = menu_glyphs::quantity[0];
        gs.glyphs.codes[1] = menu_glyphs::quantity[1];
        gs.glyphs.codes[2] = MENU_TEXT_END;
        gs.position.y += MENU_ITEM_PREVIEW_LINE_HEIGHT;
        menu_draw_string(&menu_assets.glyph_atlas, &gs);

        gs.position.x = MENU_INVENTORY_QUANTITY_VALUE_X;
        menu_format_number(item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<s32>(item_id)], MENU_ITEM_PREVIEW_QUANTITY_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
        menu_draw_number(&menu_assets.number_atlas, &gs);
    }
}

enum {
    MENU_ITEM_DETAIL_PRICE_X = 200,
    MENU_ITEM_DETAIL_LABEL_X = 242,
    MENU_ITEM_DETAIL_QUANTITY_X = 284,
    MENU_ITEM_DETAIL_GOLD_X_OFFSET = 28,
    MENU_ITEM_DETAIL_PRICE_DIGITS = 6,
    MENU_ITEM_DETAIL_GOLD_DIGITS = 6,
    MENU_SAVE_SLOT0_QUAD = 2,
    MENU_SAVE_SLOT1_QUAD = 3,
    MENU_SAVE_SLOT2_QUAD = 4,
    MENU_SAVE_SUMMARY_ROW_HEIGHT = 65,
    MENU_SAVE_SUMMARY_LINE_HEIGHT = 14,
    MENU_SAVE_SUMMARY_LABEL_X = 181,
    MENU_SAVE_SUMMARY_VALUE_X = 230,
    MENU_SAVE_STATUS_DIGITS = 4
};

void menu_draw_item_detail(KfObjectId item_id, KfItemStockBank shop_bank, KfTradeMode price_mode)
{
    MenuGlyphString gs;
    MATRIX rot;
    MATRIX lsrc;
    MATRIX lres;
    s32 price;
    MenuGlyphRow *rows;
    s16 *name;
    s32 i;

    if (item_id == KF_OBJECT_NONE) {
        return;
    }

    // Shops show the price and owned rows first, then the item's stats.
    const bool stats = menu_draw_item_stats(item_id, MENU_ITEM_PREVIEW_NAME_Y + 3 * MENU_ITEM_PREVIEW_LINE_HEIGHT + 2);
    rot.t[0] = stats ? MENU_STATS_MODEL_X : menu_preview_translation[0];
    rot.t[1] = stats ? MENU_STATS_MODEL_Y : menu_preview_translation[1];
    rot.t[2] = stats ? MENU_STATS_MODEL_Z : menu_preview_translation[2];
    menu_item_preview_rotation.vy =
        (menu_item_preview_rotation.vy + MENU_ITEM_PREVIEW_YAW_STEP)
        & KF_ANGLE_WRAP_MASK;
    kf::matrix_set_rotation_xyz(menu_item_preview_rotation, rot);

    menu_preview_light_source(&lsrc);
    kf::matrix_multiply_rotation(lsrc, rot, lres);
    menu_render_item_model(&lres, &rot);

    gs.position.x = MENU_ITEM_NAME_X;
    gs.position.y = MENU_ITEM_PREVIEW_NAME_Y;
    rows = item_name_rows;
    name = rows[kf_enum_encode<s32>(item_id)].codes;
    {
        s16 *glyph = gs.glyphs.codes;

        for (i = 0; i < MENU_GLYPHS_PER_ROW; i++) {
            *glyph = *name++;
            glyph++;
        }
    }
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = MENU_ITEM_DETAIL_PRICE_X;
    gs.position.y += MENU_ITEM_PREVIEW_LINE_HEIGHT;
    if (price_mode == KF_TRADE_BUY) {
        price = item_buy_prices[kf_enum_encode<s32>(item_id)]
            [kf_enum_encode<s32>(shop_bank) - kf_enum_encode<s32>(KF_ITEM_STOCK_FIRST_SHOP)];
    } else {
        price = item_sell_prices[kf_enum_encode<s32>(item_id)]
            [kf_enum_encode<s32>(shop_bank) - kf_enum_encode<s32>(KF_ITEM_STOCK_FIRST_SHOP)];
    }
    menu_format_number(price, MENU_ITEM_DETAIL_PRICE_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    gs.position.x = MENU_ITEM_DETAIL_LABEL_X;
    gs.glyphs.codes[0] = menu_glyphs::gold_price[0];
    gs.glyphs.codes[1] = menu_glyphs::gold_price[1];
    gs.glyphs.codes[2] = menu_glyphs::gold_price[2];
    gs.glyphs.codes[3] = menu_glyphs::gold_price[3];
    gs.glyphs.codes[4] = MENU_TEXT_END;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = MENU_ITEM_DETAIL_LABEL_X;
    gs.glyphs.codes[0] = menu_glyphs::quantity[0];
    gs.glyphs.codes[1] = menu_glyphs::quantity[1];
    gs.glyphs.codes[2] = MENU_TEXT_END;
    gs.position.y += MENU_ITEM_PREVIEW_LINE_HEIGHT;
    menu_draw_string(&menu_assets.glyph_atlas, &gs);

    gs.position.x = MENU_ITEM_DETAIL_QUANTITY_X;
    menu_format_number(item_stock[kf_enum_encode<u8>(KF_ITEM_STOCK_PLAYER)][kf_enum_encode<s32>(item_id)], MENU_ITEM_PREVIEW_QUANTITY_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);

    menu_blit_sprite_translucent(
        &menu_assets.row_background,
        &menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_SHOP)].rows[KF_SHOP_ROW_GOLD].position);
    menu_draw_string(&menu_assets.glyph_atlas, &menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_SHOP)].rows[KF_SHOP_ROW_GOLD]);

    gs.position.x = menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_SHOP)].rows[KF_SHOP_ROW_GOLD].position.x + MENU_ITEM_DETAIL_GOLD_X_OFFSET;
    gs.position.y = menu_window_layouts[kf_enum_encode<s32>(KF_MENU_WINDOW_SHOP)].rows[KF_SHOP_ROW_GOLD].position.y;
    menu_format_number(player_state.gold, MENU_ITEM_DETAIL_GOLD_DIGITS, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
    menu_draw_number(&menu_assets.number_atlas, &gs);
}

void menu_add_marker_quad(void)
{
    menu_submit_template(menu_assets.mid_depth_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)], MENU_MARKER_OT_DEPTH);
}

void menu_add_frame_quad(void)
{
    menu_submit_template(menu_assets.foreground_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)], 0);
}

void menu_draw_dialog_frame(const KfSaveSlotSummary *summaries, KfSaveSlotOverlay slot_overlay)
{
    MenuGlyphString gs;
    s32 i;

    if (slot_overlay == KF_SAVE_OVERLAY_SKIP_FIRST) {
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT1_QUAD], MENU_CONTENT_OT_DEPTH);
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT2_QUAD], MENU_CONTENT_OT_DEPTH);
    }
    if (slot_overlay == KF_SAVE_OVERLAY_SKIP_SECOND) {
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT0_QUAD], MENU_CONTENT_OT_DEPTH);
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT2_QUAD], MENU_CONTENT_OT_DEPTH);
    }
    if (slot_overlay == KF_SAVE_OVERLAY_SKIP_THIRD) {
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT0_QUAD], MENU_CONTENT_OT_DEPTH);
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT1_QUAD], MENU_CONTENT_OT_DEPTH);
    }
    if (slot_overlay >= KF_SAVE_OVERLAY_ALL) {
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT0_QUAD], MENU_CONTENT_OT_DEPTH);
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT1_QUAD], MENU_CONTENT_OT_DEPTH);
        menu_submit_template(menu_assets.dialog_quads[kf_enum_encode<u8>(game_graphics_runtime.display_state.buffer_index)][MENU_SAVE_SLOT2_QUAD], MENU_CONTENT_OT_DEPTH);
    }

    if (summaries == NULL) {
        return;
    }

    for (i = 0; i < KF_SAVE_SLOT_COUNT; i++) {
        gs.position.y = i * MENU_SAVE_SUMMARY_ROW_HEIGHT + 30;
        if (summaries[i].state == KfSaveSlotState::Damaged ||
                summaries[i].state == KfSaveSlotState::Unavailable) {
            // A damaged/unreadable slot is not empty. Keep it selectable for
            // the existing explicit replacement prompt; loading reports the error.
            for (unsigned part = 0; part < 2; ++part) {
                kf::DrawFace warning{};
                const int y = gs.position.y + (part ? 18 : 0);
                render_face_rectangle(&warning, MENU_SAVE_SUMMARY_LABEL_X, y,
                    MENU_SAVE_SUMMARY_LABEL_X + 4, y + (part ? 4 : 14));
                for (auto &vertex : warning.vertices) {
                    vertex.r = 1; vertex.g = 0.15f; vertex.b = 0.1f; vertex.a = 1;
                }
                menu_submit_template(warning, MENU_CONTENT_OT_DEPTH);
            }
        } else if (summaries[i].state == KfSaveSlotState::Ready) {
            gs.position.x = MENU_SAVE_SUMMARY_LABEL_X;
            gs.glyphs.codes[0] = menu_glyphs::experience[0];
            gs.glyphs.codes[1] = menu_glyphs::experience[1];
            gs.glyphs.codes[2] = menu_glyphs::experience[2];
            gs.glyphs.codes[3] = MENU_TEXT_END;
            menu_draw_string(&menu_assets.glyph_atlas, &gs);

            gs.position.x = 251;
            menu_format_number(summaries[i].experience, 6, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
            menu_draw_number(&menu_assets.number_atlas, &gs);

            gs.position.x = MENU_SAVE_SUMMARY_LABEL_X;
            gs.glyphs.codes[0] = menu_glyphs::floor[0];
            gs.glyphs.codes[1] = menu_glyphs::floor[1];
            gs.glyphs.codes[2] = MENU_TEXT_END;
            gs.position.y += MENU_SAVE_SUMMARY_LINE_HEIGHT;
            menu_draw_string(&menu_assets.glyph_atlas, &gs);

            gs.position.x = 286;
            menu_format_number(kf_enum_encode<u32>(summaries[i].current_floor), 1, KF_FORMAT_PAD_SPACES, gs.glyphs.codes);
            menu_draw_number(&menu_assets.number_atlas, &gs);

            gs.position.x = MENU_SAVE_SUMMARY_LABEL_X;
            gs.glyphs.codes[0] = menu_glyphs::hp[0];
            gs.glyphs.codes[1] = menu_glyphs::hp[1];
            gs.glyphs.codes[2] = MENU_TEXT_END;
            gs.position.y += MENU_SAVE_SUMMARY_LINE_HEIGHT;
            menu_draw_string(&menu_assets.glyph_atlas, &gs);

            gs.position.x = MENU_SAVE_SUMMARY_VALUE_X;
            menu_draw_vital_fraction(gs, summaries[i].current_hp,
                summaries[i].maximum_hp, MENU_SAVE_STATUS_DIGITS);

            gs.position.x = MENU_SAVE_SUMMARY_LABEL_X;
            gs.glyphs.codes[0] = menu_glyphs::mp[0];
            gs.glyphs.codes[1] = menu_glyphs::mp[1];
            gs.glyphs.codes[2] = MENU_TEXT_END;
            gs.position.y += MENU_SAVE_SUMMARY_LINE_HEIGHT;
            menu_draw_string(&menu_assets.glyph_atlas, &gs);

            gs.position.x = MENU_SAVE_SUMMARY_VALUE_X;
            menu_draw_vital_fraction(gs, summaries[i].current_mp,
                summaries[i].maximum_mp, MENU_SAVE_STATUS_DIGITS);
        }
    }
}

enum {
    MENU_LIST_CONFIRM_ACCEPT_Y = 185,
    MENU_LIST_CONFIRM_DECLINE_Y = MENU_LIST_CONFIRM_ACCEPT_Y + MENU_CONFIRM_ROW_STEP
};

static KfMenuResult menu_list_interact_impl(
    const KfMenuList *list, KfMenuConfirmKind confirm_kind, KfMenuPreviewMode preview_mode,
    s32 preview_id, KfItemStockBank shop_bank, KfTradeMode price_mode)
{
    MenuGlyphString opt0;
    MenuGlyphString opt1;
    KfMenuConfirmChoice selected;
    KfEnumStorage<KfMenuConfirmState, u32> highlight;
    u32 pad;
    u32 prev_pad;
    KfMenuResult result;

    selected = KF_MENU_CHOICE_ACCEPT;
    highlight = KF_MENU_CONFIRM_IDLE;
    pad = 0;
    result = KF_MENU_RESULT_PENDING;
    kf::host_wait_buttons_released();

    opt0.position.x = MENU_CONFIRM_TEXT_X;
    opt0.position.y = MENU_LIST_CONFIRM_ACCEPT_Y;
    opt1.position.x = MENU_CONFIRM_TEXT_X;
    opt1.position.y = MENU_LIST_CONFIRM_DECLINE_Y;
    if (confirm_kind == KF_MENU_CONFIRM_USE) {
        opt0.glyphs.codes[0] = menu_glyphs::use[0];
        opt0.glyphs.codes[1] = menu_glyphs::use[1];
        opt0.glyphs.codes[2] = MENU_TEXT_END;
    } else if (confirm_kind == KF_MENU_CONFIRM_DROP) {
        opt0.glyphs.codes[0] = menu_glyphs::drop[0];
        opt0.glyphs.codes[1] = menu_glyphs::drop[1];
        opt0.glyphs.codes[2] = menu_glyphs::drop[2];
        opt0.glyphs.codes[3] = MENU_TEXT_END;
    } else if (confirm_kind == KF_MENU_CONFIRM_YES_NO) {
        opt0.glyphs.codes[0] = menu_glyphs::yes[0];
        opt0.glyphs.codes[1] = menu_glyphs::yes[1];
        opt0.glyphs.codes[2] = MENU_TEXT_END;
    } else if (confirm_kind == KF_MENU_CONFIRM_BUY) {
        opt0.glyphs.codes[0] = menu_glyphs::buy[0];
        opt0.glyphs.codes[1] = menu_glyphs::buy[1];
        opt0.glyphs.codes[2] = MENU_TEXT_END;
    } else if (confirm_kind == KF_MENU_CONFIRM_SELL) {
        opt0.glyphs.codes[0] = menu_glyphs::sell[0];
        opt0.glyphs.codes[1] = menu_glyphs::sell[1];
        opt0.glyphs.codes[2] = MENU_TEXT_END;
    } else {
        opt0.glyphs.codes[0] = menu_glyphs::equip[0];
        opt0.glyphs.codes[1] = menu_glyphs::equip[1];
        opt0.glyphs.codes[2] = MENU_TEXT_END;
    }
    if (confirm_kind == KF_MENU_CONFIRM_YES_NO) {
        opt1.glyphs.codes[0] = menu_glyphs::no[0];
        opt1.glyphs.codes[1] = menu_glyphs::no[1];
        opt1.glyphs.codes[2] = menu_glyphs::no[2];
    } else {
        opt1.glyphs.codes[0] = menu_glyphs::cancel[0];
        opt1.glyphs.codes[1] = menu_glyphs::cancel[1];
        opt1.glyphs.codes[2] = menu_glyphs::cancel[2];
    }
    opt1.glyphs.codes[3] = MENU_TEXT_END;

    menu_frame_begin();
    if (preview_mode == KF_MENU_PREVIEW_ITEM_MODEL) {
        menu_item_model_preview(kf_enum_decode<KfObjectId>(preview_id));
    } else if (preview_mode == KF_MENU_PREVIEW_ITEM_DETAIL) {
        menu_draw_item_detail(kf_enum_decode<KfObjectId>(preview_id), shop_bank, price_mode);
    } else if (preview_mode == KF_MENU_PREVIEW_MAGIC_ICON
            && preview_id != kf_enum_encode<s32>(KF_MAGIC_NONE)) {
        menu_add_marker_quad();
    }
    menu_list_render(list);
    menu_draw_two_option(&opt0, &opt1, selected, highlight);
    menu_present_frame();
    do {
        if (result != KF_MENU_RESULT_PENDING) {
            menu_frame_begin();
            if (preview_mode == KF_MENU_PREVIEW_ITEM_MODEL) {
                menu_item_model_preview(kf_enum_decode<KfObjectId>(preview_id));
            } else if (preview_mode == KF_MENU_PREVIEW_ITEM_DETAIL) {
                menu_draw_item_detail(kf_enum_decode<KfObjectId>(preview_id), shop_bank, price_mode);
            } else if (preview_mode == KF_MENU_PREVIEW_MAGIC_ICON
                    && preview_id != kf_enum_encode<s32>(KF_MAGIC_NONE)) {
                menu_add_marker_quad();
            }
            menu_list_render(list);
            menu_draw_two_option(&opt0, &opt1, selected, highlight);
            menu_present_frame();
            kf::host_wait_buttons_released();
            return result;
        }

        highlight = KF_MENU_CONFIRM_IDLE;
        menu_frame_begin();
        prev_pad = pad;
        pad = kf::host_read_buttons();
        if ((kf::button_pressed(pad, prev_pad, kf::Button::Up)) ||
            (kf::button_pressed(pad, prev_pad, kf::Button::Down))) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (selected == KF_MENU_CHOICE_ACCEPT) {
                selected = KF_MENU_CHOICE_DECLINE;
            } else {
                selected = KF_MENU_CHOICE_ACCEPT;
            }
        } else if (kf::button_pressed(pad, prev_pad, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            highlight = KF_MENU_CONFIRM_REQUESTED;
            result = menu_confirm_result_from_choice(selected);
        } else if (kf::button_pressed(pad, prev_pad, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            result = KF_MENU_RESULT_CANCELLED;
        }
        if (preview_mode == KF_MENU_PREVIEW_ITEM_MODEL) {
            menu_item_model_preview(kf_enum_decode<KfObjectId>(preview_id));
        } else if (preview_mode == KF_MENU_PREVIEW_ITEM_DETAIL) {
            menu_draw_item_detail(kf_enum_decode<KfObjectId>(preview_id), shop_bank, price_mode);
        } else if (preview_mode == KF_MENU_PREVIEW_MAGIC_ICON
                && preview_id != kf_enum_encode<s32>(KF_MAGIC_NONE)) {
            menu_add_marker_quad();
        }
        menu_list_render(list);
        menu_draw_two_option(&opt0, &opt1, selected, highlight);
        menu_present_frame();
    } while (1);
}

enum {
    MENU_PROMPT_ACCEPT_Y_OFFSET = 44,
    MENU_PROMPT_DECLINE_Y_OFFSET = MENU_PROMPT_ACCEPT_Y_OFFSET + MENU_CONFIRM_ROW_STEP
};

KfMenuResult menu_two_option_prompt(
    KfMenuWindowKind window_kind, s32 count, s32 highlight_row,
    const KfSaveSlotSummary *summaries)
{
    MenuGlyphString label_a;
    MenuGlyphString label_b;
    KfMenuConfirmChoice selected = KF_MENU_CHOICE_ACCEPT;
    KfMenuConfirmState highlight = KF_MENU_CONFIRM_IDLE;
    KfSaveSlotOverlay overlay = KF_SAVE_OVERLAY_NONE;
    s32 input = 0;
    s32 prev;
    KfMenuResult result = KF_MENU_RESULT_PENDING;

    kf::host_wait_buttons_released();

    if (window_kind == KF_MENU_WINDOW_SAVE || window_kind == KF_MENU_WINDOW_LOAD)
        overlay = kf_enum_decode<KfSaveSlotOverlay>(highlight_row);

    label_a.position.x = MENU_CONFIRM_TEXT_X;
    label_a.position.y = count * MENU_CONFIRM_ROW_STEP + MENU_PROMPT_ACCEPT_Y_OFFSET;
    label_a.glyphs.codes[0] = menu_glyphs::save_yes[0];
    label_a.glyphs.codes[1] = menu_glyphs::save_yes[1];
    label_a.glyphs.codes[2] = MENU_TEXT_END;
    label_b.position.x = MENU_CONFIRM_TEXT_X;
    label_b.position.y = count * MENU_CONFIRM_ROW_STEP + MENU_PROMPT_DECLINE_Y_OFFSET;
    label_b.glyphs.codes[0] = menu_glyphs::save_no[0];
    label_b.glyphs.codes[1] = menu_glyphs::save_no[1];
    label_b.glyphs.codes[2] = menu_glyphs::save_no[2];
    label_b.glyphs.codes[3] = MENU_TEXT_END;

    for (;;) {
        if (result != KF_MENU_RESULT_PENDING) {
            menu_frame_begin();
            menu_draw_dialog_frame(summaries, overlay);
            menu_draw_window(window_kind, count, highlight_row, KF_MENU_CONFIRM_REQUESTED);
            menu_draw_two_option(&label_a, &label_b, selected, highlight);
            menu_present_frame();
            kf::host_wait_buttons_released();
            return result;
        }

        highlight = KF_MENU_CONFIRM_IDLE;
        prev = input;
        input = kf::host_read_buttons();
        if ((kf::button_pressed(input, prev, kf::Button::Up)) ||
            (kf::button_pressed(input, prev, kf::Button::Down))) {
            menu_play_input_sound(MENU_SOUND_CURSOR);
            if (selected != KF_MENU_CHOICE_ACCEPT)
                selected = KF_MENU_CHOICE_ACCEPT;
            else
                selected = KF_MENU_CHOICE_DECLINE;
        } else if (kf::button_pressed(input, prev, kf::Button::Confirm)) {
            menu_play_input_sound(MENU_SOUND_CONFIRM);
            highlight = KF_MENU_CONFIRM_REQUESTED;
            result = menu_confirm_result_from_choice(selected);
        } else if (kf::button_pressed(input, prev, kf::Button::Back)) {
            menu_play_input_sound(MENU_SOUND_CANCEL_OR_ERROR);
            result = KF_MENU_RESULT_CANCELLED;
        }

        menu_frame_begin();
        menu_draw_dialog_frame(summaries, overlay);
        menu_draw_window(window_kind, count, highlight_row, KF_MENU_CONFIRM_REQUESTED);
        menu_draw_two_option(&label_a, &label_b, selected, highlight);
        menu_present_frame();
    }
}

void menu_draw_window(KfMenuWindowKind window_kind, s32 count, s32 highlight, KfMenuConfirmState confirmation)
{
    const MenuWindowLayout *layout;
    s32 row;

    layout = &menu_window_layouts[kf_enum_encode<s32>(window_kind)];
    if (layout->title.position.x != 0) {
        menu_blit_sprite_translucent(
            &menu_assets.row_background, &layout->title.position);
        menu_draw_string(&menu_assets.glyph_atlas, &layout->title);
    }
    if (count > 0) {
        row = 0;
        do {
            if (row == highlight && confirmation == KF_MENU_CONFIRM_REQUESTED) {
                menu_blit_sprite_translucent(&menu_assets.row_confirmed_background,
                    &layout->rows[row].position);
            } else {
                menu_blit_sprite_translucent(&menu_assets.row_background,
                    &layout->rows[row].position);
            }
            if (row == highlight) {
                menu_blit_sprite(&menu_assets.selection_cursor, &layout->rows[row].position);
            }
            if (window_kind == KF_MENU_WINDOW_CONFIG && row == KF_MENU_CONFIG_LANGUAGE_ROW) {
                // Port addition: "Language" and the language in use.
                MenuGlyphString text = layout->rows[row];
                menu_copy_label(text.glyphs.codes, menu_glyphs::language);
                menu_draw_string(&menu_assets.glyph_atlas, &text);
                text.position.x = CONFIG_OPTION_ON_X;
                menu_copy_label(text.glyphs.codes, menu_glyphs::language_name);
                menu_draw_string(&menu_assets.glyph_atlas, &text);
            } else {
                menu_draw_string(&menu_assets.glyph_atlas, &layout->rows[row]);
            }
            row++;
        } while (row < count);
    }
    if (window_kind != KF_MENU_WINDOW_CONFIG) {
        menu_draw_window_backdrop();
    }
}

enum {
    MENU_LIST_TEXT_INSET = 3,
    MENU_LIST_ROW_HEIGHT = 12,
    MENU_LIST_QUANTITY_X_OFFSET = 110,
    MENU_LIST_QUANTITY_Y_OFFSET = 1
};

static void menu_draw_list_tile(const MenuTileSprite *tile, s32 x, s32 y)
{
    auto face = menu_textured_quad(tile->material, x, y, tile->width, tile->height);
    render_face_uv_rectangle(&face, tile->u, tile->v, tile->u + tile->width, tile->v + tile->height);
    face.transparency = kf::FaceTransparency::Blend;
    menu_submit_template(face, MENU_WIDGET_OT_DEPTH);
}

void menu_list_render(const KfMenuList *list)
{
    MenuGlyphString gs;
    const MenuTileSprite *tile;
    s16 *src;
    u8 *counts;
    s32 row;
    s32 i;
    s32 yoff;
    u32 tens;
    u32 ones;

    src = list->glyph_rows;
    counts = list->quantities;

    if (list->title.position.x != 0) {
        menu_blit_sprite_translucent(
            &menu_assets.row_background, &list->title.position);
        menu_draw_string(
            &menu_assets.glyph_atlas, &list->title);
    }

    if (counts != NULL)
        counts += list->scroll_offset;
    src = src + list->scroll_offset * list->glyphs_per_entry;
    row = 0;
    if (row < list->visible_rows && row < list->entry_count) {
        do {
            gs.position.x = list->list_x + MENU_LIST_TEXT_INSET;
            gs.position.y = list->list_y + MENU_LIST_TEXT_INSET;
            gs.position.y += row * MENU_LIST_ROW_HEIGHT;
            for (i = 0; i < list->glyphs_per_entry; i++) {
                gs.glyphs.codes[i] = *src++;
            }
            menu_draw_string(
                &menu_assets.glyph_atlas, &gs);
            if (list->quantities != NULL) {
                tens = *counts / 10u;
                ones = *counts % 10u;
                gs.position.x += MENU_LIST_QUANTITY_X_OFFSET;
                gs.position.y += MENU_LIST_QUANTITY_Y_OFFSET;
                gs.glyphs.codes[0] = tens;
                if (tens == 0) {
                    gs.glyphs.codes[0] = MENU_NUMBER_BLANK;
                }
                gs.glyphs.codes[1] = ones;
                gs.glyphs.codes[2] = MENU_TEXT_END;
                menu_draw_number(
                    &menu_assets.number_atlas, &gs);
                counts++;
            }
        } while (++row < list->visible_rows && row < list->entry_count);
    }

    tile = &menu_assets.list_tiles[MENU_LIST_TILE_BACKDROP];
    menu_draw_list_tile(tile, list->list_x, list->list_y);

    row = 0;
    if (row < list->visible_rows) {
        yoff = 0;
        do {
            tile = &menu_assets.list_tiles[MENU_LIST_TILE_ROW];
            if (row == list->cursor_row) {
                tile = &menu_assets.list_tiles[MENU_LIST_TILE_SELECTED];
            }
            row++;
            menu_draw_list_tile(tile, list->list_x, list->list_y + yoff + MENU_LIST_TEXT_INSET);
            yoff += MENU_LIST_ROW_HEIGHT;
        } while (row < list->visible_rows);
    }

    tile = &menu_assets.list_tiles[MENU_LIST_TILE_END];
    menu_draw_list_tile(tile, list->list_x, list->list_y + list->visible_rows * MENU_LIST_ROW_HEIGHT + MENU_LIST_TEXT_INSET);

    menu_draw_window_backdrop();
}

enum {
    MENU_TRANSLUCENT_SPRITE_X_OFFSET = 4,
    MENU_TRANSLUCENT_SPRITE_Y_OFFSET = 3,
    MENU_OPAQUE_SPRITE_X_OFFSET = 18,
    MENU_OPAQUE_SPRITE_Y_OFFSET = 2,
    MENU_LIST_DEFAULT_VISIBLE_ROWS = 11,
    MENU_LIST_DEFAULT_GLYPHS_PER_ENTRY = 8,
    MENU_BACKDROP_LEFT_X = 166,
    MENU_BACKDROP_RIGHT_X = MENU_BACKDROP_LEFT_X + MENU_BACKDROP_COLUMN_STEP,
    MENU_PICKUP_BACKDROP_LEFT_X = 118,
    MENU_PICKUP_BACKDROP_RIGHT_X = MENU_PICKUP_BACKDROP_LEFT_X + MENU_BACKDROP_COLUMN_STEP
};

enum {
    MENU_FONT_COLUMNS = 16,
    MENU_FONT_CELL_WIDTH = 14,
    MENU_FONT_CELL_HEIGHT = 12,
    MENU_NUMBER_CELL_HEIGHT = 11,

    MENU_DAKUTEN_U = 14 * MENU_FONT_CELL_WIDTH,
    MENU_HANDAKUTEN_U = 15 * MENU_FONT_CELL_WIDTH,
    MENU_KANA_MARK_V = 2 * MENU_FONT_CELL_HEIGHT
};

void menu_draw_two_option(
    const MenuGlyphString *accept_label, const MenuGlyphString *decline_label,
    KfMenuConfirmChoice selected_choice, KfMenuConfirmState confirmation)
{
    if (selected_choice == KF_MENU_CHOICE_ACCEPT) {
        menu_blit_sprite(&menu_assets.selection_cursor, &accept_label->position);
    } else {
        menu_blit_sprite(&menu_assets.selection_cursor, &decline_label->position);
    }
    if (confirmation == KF_MENU_CONFIRM_REQUESTED) {
        if (selected_choice == KF_MENU_CHOICE_ACCEPT) {
            menu_blit_sprite_translucent(
                &menu_assets.option_highlight, &accept_label->position);
            menu_blit_sprite_translucent(
                &menu_assets.option_background, &decline_label->position);
        } else {
            menu_blit_sprite_translucent(
                &menu_assets.option_background, &accept_label->position);
            menu_blit_sprite_translucent(
                &menu_assets.option_highlight, &decline_label->position);
        }
    } else {
        menu_blit_sprite_translucent(
            &menu_assets.option_background, &accept_label->position);
        menu_blit_sprite_translucent(
            &menu_assets.option_background, &decline_label->position);
    }
    menu_draw_string(&menu_assets.glyph_atlas, accept_label);
    menu_draw_string(&menu_assets.glyph_atlas, decline_label);
}

void menu_draw_item_name_frame(KfObjectId item_id)
{
    MenuGlyphString string;
    MATRIX rotation;
    MATRIX light_source;
    MATRIX light_result;
    const MenuGlyphRow *rows;
    const MenuGlyphRow *name;
    s32 i;

    rotation.t[0] = pickup_preview_translation[0];
    rotation.t[1] = pickup_preview_translation[1];
    rotation.t[2] = pickup_preview_translation[2];
    menu_item_preview_rotation.vy =
        (menu_item_preview_rotation.vy + MENU_PICKUP_PREVIEW_YAW_STEP)
        & KF_ANGLE_WRAP_MASK;
    kf::matrix_set_rotation_xyz(menu_item_preview_rotation, rotation);

    menu_preview_light_source(&light_source);
    kf::matrix_multiply_rotation(light_source, rotation, light_result);
    menu_render_item_model(&light_result, &rotation);

    rows = item_name_rows;
    name = &rows[kf_enum_encode<s32>(item_id)];
    string.position.x = 0x80;
    string.position.y = 0x24;
    for (i = 0; i < MENU_GLYPHS_PER_ROW; i++) {
        string.glyphs.codes[i] = name->codes[i];
    }
    menu_draw_string(&menu_assets.glyph_atlas, &string);

    menu_draw_backdrop_tile(MENU_PICKUP_BACKDROP_LEFT_X, MENU_BACKDROP_TOP_Y, false, false);

    menu_draw_backdrop_tile(MENU_PICKUP_BACKDROP_RIGHT_X, MENU_BACKDROP_TOP_Y, true, false);

    menu_draw_backdrop_tile(MENU_PICKUP_BACKDROP_LEFT_X, MENU_BACKDROP_BOTTOM_Y, false, true);

    menu_draw_backdrop_tile(MENU_PICKUP_BACKDROP_RIGHT_X, MENU_BACKDROP_BOTTOM_Y, true, true);

    menu_enqueue_background();
}

void menu_blit_sprite_translucent(
    const MenuSpriteDef *sprite, const MenuPoint *position)
{
    auto face = menu_textured_quad(sprite->material,
        position->x - MENU_TRANSLUCENT_SPRITE_X_OFFSET,
        position->y - MENU_TRANSLUCENT_SPRITE_Y_OFFSET, sprite->width, sprite->height);
    render_face_uv_rectangle(&face, sprite->u, sprite->v, sprite->u + sprite->width, sprite->v + sprite->height);
    face.transparency = kf::FaceTransparency::Blend;
    menu_submit_template(face, MENU_WIDGET_OT_DEPTH);
}

void menu_blit_sprite(
    const MenuSpriteDef *sprite, const MenuPoint *position)
{
    auto face = menu_textured_quad(sprite->material,
        position->x - MENU_OPAQUE_SPRITE_X_OFFSET,
        position->y - MENU_OPAQUE_SPRITE_Y_OFFSET, sprite->width, sprite->height);
    render_face_uv_rectangle(&face, sprite->u, sprite->v, sprite->u + sprite->width, sprite->v + sprite->height);
    menu_submit_template(face, MENU_WIDGET_OT_DEPTH);
}

static kf::DrawFace menu_text_glyph(
    const MenuSpriteDef *font, const MenuGlyphString *string, s32 x_offset)
{
    return menu_textured_quad(font->material, string->position.x + x_offset,
        string->position.y, font->width, font->height);
}

void menu_draw_string(
    const MenuSpriteDef *font, const MenuGlyphString *string)
{
    s32 i;
    s32 x_offset;

    for (i = 0; i < MENU_GLYPHS_PER_ROW && string->glyphs.codes[i] != MENU_TEXT_END; i++) {
        s32 glyph;

        x_offset = i * MENU_FONT_CELL_WIDTH;
        auto face = menu_text_glyph(font, string, x_offset);
        glyph = string->glyphs.codes[i] & MENU_TEXT_GLYPH_MASK;
        const s32 u = (glyph % MENU_FONT_COLUMNS) * MENU_FONT_CELL_WIDTH;
        const s32 v = (glyph / MENU_FONT_COLUMNS) * MENU_FONT_CELL_HEIGHT;
        render_face_uv_rectangle(&face, u, v, u + font->width, v + font->height);
        menu_submit_template(face, MENU_CONTENT_OT_DEPTH);

        if (string->glyphs.codes[i] & MENU_TEXT_DAKUTEN) {
            face = menu_text_glyph(font, string, x_offset);
            render_face_uv_rectangle(&face, MENU_DAKUTEN_U, MENU_KANA_MARK_V,
                MENU_DAKUTEN_U + font->width, MENU_KANA_MARK_V + font->height);
            menu_submit_template(face, MENU_CONTENT_OT_DEPTH);
        }

        if (string->glyphs.codes[i] & MENU_TEXT_HANDAKUTEN) {
            face = menu_text_glyph(font, string, x_offset);
            render_face_uv_rectangle(&face, MENU_HANDAKUTEN_U, MENU_KANA_MARK_V,
                MENU_HANDAKUTEN_U + font->width, MENU_KANA_MARK_V + font->height);
            menu_submit_template(face, MENU_CONTENT_OT_DEPTH);
        }
    }
}

void menu_draw_number(
    const MenuSpriteDef *font, const MenuGlyphString *string)
{
    const MenuSpriteDef *atlas = font;
    const MenuGlyphString *label = string;
    s32 i;
    s32 xoff;

    for (i = 0; i < MENU_GLYPHS_PER_ROW && label->glyphs.codes[i] != MENU_TEXT_END; i++) {
        xoff = i * MENU_NUMBER_ADVANCE;
        auto face = menu_text_glyph(atlas, label, xoff);
        const s32 v = label->glyphs.codes[i] * MENU_NUMBER_CELL_HEIGHT;
        render_face_uv_rectangle(&face, atlas->u, v, atlas->u + atlas->width, v + atlas->height);
        for (auto &vertex : face.vertices) {
            vertex.r *= menu_number_tint[0];
            vertex.g *= menu_number_tint[1];
            vertex.b *= menu_number_tint[2];
        }
        menu_submit_template(face, MENU_CONTENT_OT_DEPTH);
    }
}

void menu_draw_window_backdrop(void)
{
    menu_draw_backdrop_tile(MENU_BACKDROP_LEFT_X, MENU_BACKDROP_TOP_Y, false, false);

    menu_draw_backdrop_tile(MENU_BACKDROP_RIGHT_X, MENU_BACKDROP_TOP_Y, true, false);

    menu_draw_backdrop_tile(MENU_BACKDROP_LEFT_X, MENU_BACKDROP_BOTTOM_Y, false, true);

    menu_draw_backdrop_tile(MENU_BACKDROP_RIGHT_X, MENU_BACKDROP_BOTTOM_Y, true, true);

    menu_enqueue_background();
}

void menu_frame_begin(void)
{
    game_graphics_runtime.display_state.buffer_index = display_next_buffer(game_graphics_runtime.display_state.buffer_index);
    kf::host_begin_frame();
}

void menu_present_frame(void)
{
    kf::host_wait_frame();
    kf::host_present_frame(game_graphics_runtime.display_state.frame_style);
}

void menu_list_init(KfMenuList *list, KfMenuWindowKind window_kind, s32 row)
{
    s32 i;

    list->title.position.x = 12;
    list->title.position.y = 19;
    for (i = 0; i < MENU_GLYPHS_PER_ROW; i++) {
        list->title.glyphs.codes[i] = menu_window_layouts[kf_enum_encode<s32>(window_kind)].rows[row].glyphs.codes[i];
    }
    list->list_x = 0x16;
    list->list_y = 0x26;
    list->entry_count = 0;
    list->visible_rows = MENU_LIST_DEFAULT_VISIBLE_ROWS;
    list->scroll_offset = 0;
    list->selected_index = 0;
    list->cursor_row = 0;
    list->glyphs_per_entry = MENU_LIST_DEFAULT_GLYPHS_PER_ENTRY;
}

void menu_format_number(s32 value, s32 count, KfFormatPaddingMode padding_mode, s16 *out)
{
    s32 i = 0;
    s32 blank;

    blank = (padding_mode == KF_FORMAT_PAD_SPACES) ? MENU_NUMBER_BLANK : 0;
    for (; i < count; i++) {
        out[i] = blank;
    }
    out[count] = MENU_TEXT_END;
    for (i = count - 1; i >= 0; i--) {
        out[i] = value % 10;
        value /= 10;
        if (value == 0) {
            i = -1;
        }
    }
}

KfResourceLoadResult menu_load_item_model(KfObjectId item_id)
{
    u8 *asset;
    std::size_t asset_size;

    menu_release_item_model();
    if (item_id != KF_OBJECT_NONE) {
        if (resource_file_load_item_model(&asset, kf_enum_encode<s32>(item_id), &asset_size) != KF_RESOURCE_LOADED) {
            return KF_RESOURCE_LOAD_FAILED;
        }
        tmd_register(tmd_context(), KF_TMD_SLOT_MENU_ITEM, asset, asset_size);
        menu_item_model_allocation_pending = KF_MENU_MODEL_ALLOCATED;
    }
    menu_item_preview_rotation.vy = 0;
    return KF_RESOURCE_LOADED;
}

void menu_release_item_model(void)
{
    if (menu_item_model_allocation_pending == KF_MENU_MODEL_ALLOCATED) {
        tmd_release_last_allocation(tmd_context(), memory_arena, KF_TMD_SLOT_MENU_ITEM);
        menu_item_model_allocation_pending = KF_MENU_MODEL_RELEASED;
    }
}

KfResourceLoadResult menu_load_item_texture(KfMenuTextureId texture_id)
{
    char name[menu_image_path_capacity] = "TIM/M000.";
    u8 *destination;
    s32 number;

    if (texture_id != KF_MENU_TEXTURE_NONE) {
        number = kf_enum_encode<s32>(texture_id) + 1;
        resource_path_write_decimal3(&name[menu_image_number_offset], number);
        destination = game_graphics_runtime.display_state.asset_load_buffer;
        std::size_t image_size;
        if (resource_file_load_into(destination,
                game_graphics_runtime.display_state.asset_load_capacity, name, &image_size) != KF_RESOURCE_LOADED) {
            return KF_RESOURCE_LOAD_FAILED;
        }
        tim_upload_images(destination, image_size);
    }
    return KF_RESOURCE_LOADED;
}

KfMenuResult menu_list_interact(
    const KfMenuList *list, KfMenuConfirmKind confirmation, KfMenuPreviewMode preview,
    KfObjectId id, KfItemStockBank bank, KfTradeMode trade)
{
    return menu_list_interact_impl(list, confirmation, preview, static_cast<s32>(id), bank, trade);
}

KfMenuResult menu_list_interact(
    const KfMenuList *list, KfMenuConfirmKind confirmation, KfMenuPreviewMode preview,
    KfEffectKind id, KfItemStockBank bank, KfTradeMode trade)
{
    return menu_list_interact_impl(list, confirmation, preview, static_cast<s32>(id), bank, trade);
}

void menu_runtime_reset_module_state(void)
{
    kf::restore_initial_value<menu_item_model_allocation_pending>();
    kf::restore_initial_value<menu_item_preview_rotation>();
}
