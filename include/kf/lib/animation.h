#ifndef KF_ANIMATION_H
#define KF_ANIMATION_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>

enum class KfAnimationClip : u8 {
    KF_ANIMATION_CLIP_FIRST = 0,
    KF_ANIMATION_CLIP_SECOND = 1,
    KF_ANIMATION_CLIP_THIRD = 2,
    KF_ANIMATION_CLIP_FOURTH = 3,
    KF_ANIMATION_CLIP_NONE = 0xff
}; using enum KfAnimationClip;

#endif // KF_ANIMATION_H
