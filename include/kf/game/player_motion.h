#ifndef KF_GAME_PLAYER_MOTION_H
#define KF_GAME_PLAYER_MOTION_H

#include <kf/lib/types.h>

enum class KfPlayerVerticalState : u8 {
    KF_PLAYER_VERTICAL_GROUNDED = 0,
    KF_PLAYER_VERTICAL_FALLING = 0x10,
    KF_PLAYER_VERTICAL_STEP_UP = 0x20
};
using enum KfPlayerVerticalState;

inline constexpr s32 KF_PLAYER_CAMERA_HEIGHT = 1500;
inline constexpr s32 KF_PLAYER_CAMERA_PITCH_LIMIT = 191;

#endif // KF_GAME_PLAYER_MOTION_H
