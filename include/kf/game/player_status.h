#ifndef KF_PLAYER_STATUS_H
#define KF_PLAYER_STATUS_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>

enum class KfPlayerStatusFlags : u16 {
    KF_PLAYER_STATUS_NONE = 0,
    KF_PLAYER_STATUS_CURSE = 1 << 0,
    KF_PLAYER_STATUS_DARKNESS = 1 << 1,
    KF_PLAYER_STATUS_POISON = 1 << 2,
    KF_PLAYER_STATUS_SLOWED = 1 << 3,
    KF_PLAYER_STATUS_FIRE_DEFENSE_BOOST = 1 << 4,
    KF_PLAYER_STATUS_KEEP_UPPER = 0xfff0
}; using enum KfPlayerStatusFlags;
constexpr KfPlayerStatusFlags operator|(KfPlayerStatusFlags lhs, KfPlayerStatusFlags rhs)
    { return static_cast<KfPlayerStatusFlags>(static_cast<u16>(lhs) | static_cast<u16>(rhs)); }
    constexpr KfPlayerStatusFlags operator&(KfPlayerStatusFlags lhs, KfPlayerStatusFlags rhs)
    { return static_cast<KfPlayerStatusFlags>(static_cast<u16>(lhs) & static_cast<u16>(rhs)); }
    constexpr KfPlayerStatusFlags operator^(KfPlayerStatusFlags lhs, KfPlayerStatusFlags rhs)
    { return static_cast<KfPlayerStatusFlags>(static_cast<u16>(lhs) ^ static_cast<u16>(rhs)); }
    constexpr KfPlayerStatusFlags operator~(KfPlayerStatusFlags value)
    { return static_cast<KfPlayerStatusFlags>(~static_cast<u16>(value)); }
    inline KfPlayerStatusFlags& operator|=(KfPlayerStatusFlags& lhs, KfPlayerStatusFlags rhs) { return lhs = lhs | rhs; }
    inline KfPlayerStatusFlags& operator&=(KfPlayerStatusFlags& lhs, KfPlayerStatusFlags rhs) { return lhs = lhs & rhs; }
    inline KfPlayerStatusFlags& operator^=(KfPlayerStatusFlags& lhs, KfPlayerStatusFlags rhs) { return lhs = lhs ^ rhs; }

#endif // KF_PLAYER_STATUS_H
