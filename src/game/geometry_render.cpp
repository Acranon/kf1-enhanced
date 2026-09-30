#include <kf/platform/prelude.h>
#include <kf/game/asset.h>
#include <kf/game/graphics.h>
#include <kf/game/menu.h>
#include <kf/game/notify.h>
#include <kf/game/player.h>
#include <kf/game/render.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/math.h>
#include <kf/lib/null.h>

#include <array>
#include <stdarg.h>

std::array<KfHudSprite, KF_HUD_TABLE_ROWS> hud_sprites = {
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0x50, 0x03, 1, 5, 0x1f, 0x15, 0x32, 5}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0x50, 0x03, 1, 5, 0x1f, 0x23, 0x32, 5}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0x50, 0x15, 1, 2, 0x81, 0x16, 0x32, 2}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0x50, 0x15, 1, 2, 0x81, 0x24, 0x32, 2}},
    KfHudSprite{KF_SPRITE_HIDDEN, 0, {0, 0x50, 0x38, 0x10, 0xc7, 0x16, 0x38, 0x10}},
    KfHudSprite{KF_SPRITE_HIDDEN, 0, {0, 0x60, 0x38, 0x10, 0xc7, 0x16, 0x38, 0x10}},
    KfHudSprite{KF_SPRITE_HIDDEN, 0, {0, 0x40, 0x38, 0x10, 0xc7, 0x16, 0x38, 0x10}},
    KfHudSprite{KF_SPRITE_HIDDEN, 0, {0, 0x70, 0x38, 0x10, 0xc7, 0x16, 0x38, 0x10}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0, 0, 0x4a, 0x0b, 0x0a, 0x12, 0x4a, 0x0b}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0, 0x10, 0x4a, 0x0b, 0x0a, 0x20, 0x4a, 0x0b}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0, 0x20, 0x5c, 8, 0x5e, 0x13, 0x5c, 8}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0, 0x30, 0x5c, 8, 0x5e, 0x21, 0x5c, 8}},
    KfHudSprite{KF_SPRITE_VISIBLE, 0, {0, 0x80, 0x21, 0x20, 0x10d, 0x12, 0x21, 0x20}},
    KfHudSprite{KF_SPRITE_END, 0, {0xff, 0xff, 0xff, 0xff, 0xffff, 0xffff, 0xffff, 0xffff}},
};

std::array<KfNotificationSprite, KF_NOTIFICATION_SPRITE_COUNT> notification_sprites = {
    KfNotificationSprite{KF_SPRITE_HIDDEN, 0, {0, 0, 0x7f, 0x0f, 0xffc0, 0xffa0, 0x7f, 0x0f}},
    KfNotificationSprite{KF_SPRITE_HIDDEN, 0, {0, 0, 0x7f, 0x0f, 0xffc4, 0xffa0, 0x7f, 0x0f}},
    KfNotificationSprite{KF_SPRITE_HIDDEN, 0, {0xf0, 0, 7, 0x0b, 0xffc4, 0xffa3, 7, 0x0b}},
    KfNotificationSprite{KF_SPRITE_HIDDEN, 0, {0xf0, 0, 7, 0x0b, 0xffba, 0xffa3, 7, 0x0b}},
    KfNotificationSprite{KF_SPRITE_HIDDEN, 0, {0xf0, 0, 7, 0x0b, 0xffb0, 0xffa3, 7, 0x0b}},
    KfNotificationSprite{KF_SPRITE_HIDDEN, 0, {0xf0, 0, 7, 0x0b, 0xffa6, 0xffa3, 7, 0x0b}},
};

std::array<KfHudModel, KF_HUD_MODEL_TABLE_ROWS> hud_models = {
    KfHudModel{KF_SPRITE_VISIBLE, KF_ANIMATION_CLIP_FIRST, 0, 0x33, 0x11e, 0x22, 0xc8, {0, 0}, {0, 0, 0, 0}, {0, 0}, NULL},
    KfHudModel{KF_SPRITE_END, {}, 0, 0, 0, 0, 0, {}, {}, {}, nullptr},
};

enum {
    WEAPON_PROJECTED_DEPTH_SHIFT = 3,
    WEAPON_DEPTH_BIAS_SHIFT = 5,
    WEAPON_BASE_DEPTH_BIAS = 50
};

void render_weapon(void)
{
    MATRIX model;
    KfWeaponRecord *weapon;
    KfTmdObject *object;
    s32 depth_bias;

    if (player_state.weapon_attack_phase == KF_WEAPON_ATTACK_INACTIVE) {
        return;
    }
    auto projection = game_graphics_runtime.render_state.projection;
    projection.distance = player_state.equipped_weapon_record->projection_distance;
    weapon = player_state.equipped_weapon_record;
    model.t[0] = weapon->render_translation.x;
    model.t[1] = weapon->render_translation.y;
    model.t[2] = weapon->render_translation.z;
    kf::matrix_set_rotation_xyz(weapon->render_rotation, model);
    asset_registry_select(KF_ASSET_WEAPON);
    object = tmd_get_object(tmd_context(), 0);
    if (render_bind_instance_vertices(
            &player_state.weapon_animation_cache, KF_ASSET_WEAPON, KF_ANIMATION_CLIP_FIRST,
            player_state.weapon_attack_phase,
            object->vertex_count)) {
        tmd_project_vertices_depth_shift(tmd_context(), object->vertex_count, WEAPON_PROJECTED_DEPTH_SHIFT, &model, projection);
        depth_bias =
            player_state.equipped_weapon_record->render_translation.z >> WEAPON_DEPTH_BIAS_SHIFT;
        render_enqueue_tmd(0, -depth_bias + WEAPON_BASE_DEPTH_BIAS, &render_light_matrices[KF_RENDER_LIGHT_WEAPON]);
    }
}

void render_hud_models(const MATRIX *lights)
{
    MATRIX model;
    VECTOR scale;
    MATRIX saved_color_matrix;
    KfHudModel *entry;
    KfTmdObject *object;
    u16 scale_numerator;

    saved_color_matrix = game_graphics_runtime.render_state.lighting.color_matrix;
    game_graphics_runtime.render_state.lighting.color_matrix.m = (game_graphics_runtime.hud_model_color_matrix).m;
    scale.vz = KF_FIXED12_ONE;
    entry = hud_models.data();
    while (entry->state == KF_SPRITE_VISIBLE) {
        model.t[0] = entry->translation_x;
        model.t[1] = entry->translation_y;
        model.t[2] = entry->translation_z;
        kf::matrix_set_rotation_xyz(entry->rotation, model);
        scale_numerator = entry->scale;
        scale.vy = scale_numerator;
        scale.vx = scale_numerator;
        kf::matrix_scale_axes(model, scale);
        asset_registry_select(KF_ASSET_HUD_MODELS);
        object = tmd_get_object(tmd_context(), 0);
        if (render_bind_instance_vertices(
                &entry->animation_cache, KF_ASSET_HUD_MODELS,
                entry->animation_clip, entry->animation_phase,
                object->vertex_count)) {
            tmd_transform_vertices(tmd_context(), object->vertex_count, &model);
            render_enqueue_tmd(0, 0, lights);
        }
        entry++;
    }
    game_graphics_runtime.render_state.lighting.color_matrix.m = (saved_color_matrix).m;
}

void render_hud_sprites(KfHudSprite *table)
{
    KfHudSprite *entry;

    entry = table;
    if (entry->state == KF_SPRITE_END) {
        return;
    }
    do {
        if (entry->state == KF_SPRITE_VISIBLE) {
            render_screen_sprite(&entry->sprite);
        }
        entry++;
    } while (entry->state != KF_SPRITE_END);
}

enum {
    NOTIFICATION_HOLD_FRAMES = 15,
    NOTIFICATION_EXIT_ANGLE_STEP = 128,
    NOTIFICATION_GOLD_DIGITS = 4,
    NOTIFICATION_DIGIT_ROW_HEIGHT = 11,
    NOTIFICATION_ATLAS_COLUMN_MASK = 0xf0,
    NOTIFICATION_ATLAS_COLUMN_SHIFT = 3,
    NOTIFICATION_ATLAS_ROW_MASK = 0xf,
    NOTIFICATION_ATLAS_ROW_SHIFT = 4
};

void notify_enqueue(KfNotificationArgument message_id, ...)
{
    u8 *head;

    if (message_id == KF_NOTIFICATION_NONE) {
        return;
    }
    head = &game_graphics_runtime.notification_state.control.queue_head;
    if (game_graphics_runtime.notification_message_ids[*head] == KF_NOTIFICATION_NONE) {
        game_graphics_runtime.notification_message_ids[*head] = message_id;
        if (message_id == KF_NOTIFICATION_GOLD) {
            u16 *payload = game_graphics_runtime.notification_state.message_payloads.data();
            va_list arguments;
            va_start(arguments, message_id);
            payload[*head] = va_arg(arguments, s32);
            va_end(arguments);
        }
        *head = (*head + 1) & (KF_NOTIFICATION_CAPACITY - 1);
    }
}

void notification_digit_set_v(KfSpriteQuad *sprite, s32 digit)
{
    sprite->v = digit * NOTIFICATION_DIGIT_ROW_HEIGHT;
}

static inline void notify_dequeue_group(void)
{
    KfNotificationControl *control;
    KfNotificationId id;

    control = &game_graphics_runtime.notification_state.control;
    id = game_graphics_runtime.notification_message_ids[game_graphics_runtime.notification_state.control.queue_tail];
    do {
        game_graphics_runtime.notification_message_ids[control->queue_tail] = KF_NOTIFICATION_NONE;
        control->queue_tail = (control->queue_tail + 1) & (KF_NOTIFICATION_CAPACITY - 1);
    } while (id == game_graphics_runtime.notification_message_ids[control->queue_tail]
        && id != KF_NOTIFICATION_GOLD);
    control->effect_phase = KF_NOTIFICATION_IDLE;
}

void notify_effect_update(void)
{
    KfNotificationPhase *phase = &game_graphics_runtime.notification_state.control.effect_phase;

    switch (*phase) {
    case KF_NOTIFICATION_IDLE: {
        u8 tail = game_graphics_runtime.notification_state.control.queue_tail;
        KfNotificationId id = game_graphics_runtime.notification_message_ids[tail];
        if (id == KF_NOTIFICATION_NONE) {
            return;
        }
        *phase = KF_NOTIFICATION_HOLD;
        game_graphics_runtime.notification_state.control.effect_angle_x = 0;
        game_graphics_runtime.notification_state.control.hold_frames = NOTIFICATION_HOLD_FRAMES;
        if (id == KF_NOTIFICATION_GOLD) {
            KfNotificationSprite *sprite_records = notification_sprites.data();
            std::array<s16, KF_NOTIFICATION_DIGIT_CAPACITY> digits;
            sprite_records[KF_NOTIFICATION_TEXT_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_GOLD_SPRITE].active = KF_SPRITE_VISIBLE;
            notification_sprites[KF_NOTIFICATION_GOLD_SPRITE].sprite.u =
                (kf_enum_encode<u8>(id) & NOTIFICATION_ATLAS_COLUMN_MASK) << NOTIFICATION_ATLAS_COLUMN_SHIFT;
            notification_sprites[KF_NOTIFICATION_GOLD_SPRITE].sprite.v =
                (kf_enum_encode<u8>(id) & NOTIFICATION_ATLAS_ROW_MASK) << NOTIFICATION_ATLAS_ROW_SHIFT;
            menu_format_number(
                game_graphics_runtime.notification_state.message_payloads[tail],
                NOTIFICATION_GOLD_DIGITS, KF_FORMAT_PAD_SPACES, digits);
            notification_sprites[KF_NOTIFICATION_ONES_SPRITE].active = KF_SPRITE_VISIBLE;
            notification_digit_set_v(
                &sprite_records[KF_NOTIFICATION_ONES_SPRITE].sprite, (u16)digits[3]);
            notification_sprites[KF_NOTIFICATION_TENS_SPRITE].active = KF_SPRITE_VISIBLE;
            notification_digit_set_v(
                &sprite_records[KF_NOTIFICATION_TENS_SPRITE].sprite, (u16)digits[2]);
            notification_sprites[KF_NOTIFICATION_HUNDREDS_SPRITE].active = KF_SPRITE_VISIBLE;
            notification_digit_set_v(
                &sprite_records[KF_NOTIFICATION_HUNDREDS_SPRITE].sprite, (u16)digits[1]);
            notification_sprites[KF_NOTIFICATION_THOUSANDS_SPRITE].active = KF_SPRITE_VISIBLE;
            notification_digit_set_v(
                &sprite_records[KF_NOTIFICATION_THOUSANDS_SPRITE].sprite, (u16)digits[0]);
        } else {
            notification_sprites[KF_NOTIFICATION_TEXT_SPRITE].active = KF_SPRITE_VISIBLE;
            notification_sprites[KF_NOTIFICATION_TEXT_SPRITE].sprite.u =
                (kf_enum_encode<u8>(id) & NOTIFICATION_ATLAS_COLUMN_MASK) << NOTIFICATION_ATLAS_COLUMN_SHIFT;
            notification_sprites[KF_NOTIFICATION_TEXT_SPRITE].sprite.v =
                (kf_enum_encode<u8>(id) & NOTIFICATION_ATLAS_ROW_MASK) << NOTIFICATION_ATLAS_ROW_SHIFT;
            notification_sprites[KF_NOTIFICATION_THOUSANDS_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_HUNDREDS_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_TENS_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_ONES_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_GOLD_SPRITE].active = KF_SPRITE_HIDDEN;
        }
        break;
    }
    case KF_NOTIFICATION_HOLD: {
        u8 counter = game_graphics_runtime.notification_state.control.hold_frames - 1;
        game_graphics_runtime.notification_state.control.hold_frames = counter;
        if (counter == 0) {
            *phase = KF_NOTIFICATION_ROTATE_OUT;
        }
        break;
    }
    case KF_NOTIFICATION_ROTATE_OUT: {
        s16 angle_x = game_graphics_runtime.notification_state.control.effect_angle_x + NOTIFICATION_EXIT_ANGLE_STEP;
        game_graphics_runtime.notification_state.control.effect_angle_x = angle_x;
        if (angle_x >= KF_ANGLE_EIGHTH_TURN) {
            game_graphics_runtime.notification_state.control.effect_angle_x = KF_ANGLE_EIGHTH_TURN;
            notification_sprites[KF_NOTIFICATION_THOUSANDS_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_HUNDREDS_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_TENS_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_ONES_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_GOLD_SPRITE].active = KF_SPRITE_HIDDEN;
            notification_sprites[KF_NOTIFICATION_TEXT_SPRITE].active = KF_SPRITE_HIDDEN;
            notify_dequeue_group();
        }
        break;
    }
    }
}

void display_flip_buffer_index(void)
{
    game_graphics_runtime.display_state.buffer_index = display_next_buffer(game_graphics_runtime.display_state.buffer_index);
}

void geometry_render_reset_module_state(void)
{
    kf::restore_initial_value<hud_sprites>();
    kf::restore_initial_value<notification_sprites>();
    kf::restore_initial_value<hud_models>();
}
