#ifndef KF_NOTIFY_H
#define KF_NOTIFY_H

#include <kf/game/render.h>
#include <kf/lib/notify_types.h>

#include <array>

enum {
    KF_NOTIFICATION_CAPACITY = 8,
    KF_NOTIFICATION_SPRITE_COUNT = 6,
    KF_NOTIFICATION_DIGIT_CAPACITY = 12,
    KF_NOTIFICATION_TEXT_SPRITE = 0,
    KF_NOTIFICATION_GOLD_SPRITE = 1,
    KF_NOTIFICATION_ONES_SPRITE = 2,
    KF_NOTIFICATION_TENS_SPRITE = 3,
    KF_NOTIFICATION_HUNDREDS_SPRITE = 4,
    KF_NOTIFICATION_THOUSANDS_SPRITE = 5
};

typedef struct KfNotificationSprite {
    KfSpriteState active;
    u8 unknown_01;
    KfSpriteQuad sprite;
} KfNotificationSprite;

typedef struct KfNotificationControl {
    u8 queue_tail;
    u8 queue_head;
    KfNotificationPhase effect_phase;
    u8 hold_frames;
    u16 effect_angle_x;
} KfNotificationControl;

typedef struct KfNotificationState {
    std::array<u16, KF_NOTIFICATION_CAPACITY> message_payloads;
    KfNotificationControl control;
} KfNotificationState;

extern std::array<KfNotificationSprite, KF_NOTIFICATION_SPRITE_COUNT> notification_sprites;
extern void notify_enqueue(KfNotificationArgument message_id, ...);
extern void notify_effect_update(void);

#endif // KF_NOTIFY_H
