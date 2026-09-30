#ifndef KF_OPEN_CONTROLLER_H
#define KF_OPEN_CONTROLLER_H

#include <kf/cutscene/playback.h>
#include <kf/lib/types.h>

#include <array>

enum { KF_OPENING_INITIAL_TIM_PATH_BYTES = 7 };

extern std::array<char, KF_OPENING_INITIAL_TIM_PATH_BYTES> opening_initial_tim_path;

extern void opening_run(Cutscene scene);

#endif // KF_OPEN_CONTROLLER_H
