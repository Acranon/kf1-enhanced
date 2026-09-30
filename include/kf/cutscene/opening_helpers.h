#ifndef KF_OPEN_OPENING_HELPERS_H
#define KF_OPEN_OPENING_HELPERS_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>

enum class KfOpeningInputAction : u32 {
    KF_OPENING_INPUT_NONE = 0,
    KF_OPENING_INPUT_ADVANCE = 1,
    KF_OPENING_INPUT_SKIP = 2
}; using enum KfOpeningInputAction;

extern KfOpeningInputAction opening_input_action;

extern void opening_poll_input(void);
extern s16 angle_shortest_delta(s32 first, s32 second);

#endif // KF_OPEN_OPENING_HELPERS_H
