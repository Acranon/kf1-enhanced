#ifndef KF_LIB_SOUND_TYPES_H
#define KF_LIB_SOUND_TYPES_H

#include <kf/lib/types.h>

inline constexpr int KF_SOUND_TONE_INDEX_MASK = 0x0f;
inline constexpr int KF_SOUND_PAN_NARROWING_FLAG = 0x80;

typedef struct SoundRef {
    u8 program;
    // GAME spatial playback masks off its panning flag; other callers pass
    // the selector byte unchanged. This remains the three-byte resource record.
    u8 tone_and_flags;
    u8 note;
} SoundRef;

#endif // KF_LIB_SOUND_TYPES_H
