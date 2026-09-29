#include <kf/lib/math.h>
#include <kf/open/opening_helpers.h>
#include <kf/platform/input.hpp>

KfOpeningInputAction opening_input_action;

void opening_poll_input(void)
{
    u32 input = kf::host_read_buttons();

    // PC extension: F4 skips straight into the game, which then quick loads.
    if (kf::host_opening_quick_load()) {
        opening_input_action = KF_OPENING_INPUT_SKIP;
        return;
    }
    if (input != 0) {
        if ((input & kf::Button::Start) != 0) {
            opening_input_action = KF_OPENING_INPUT_SKIP;
        } else {
            opening_input_action = KF_OPENING_INPUT_ADVANCE;
        }
    }
}

void opening_helpers_reset_module_state(void)
{
    kf::restore_initial_value<opening_input_action>();
}
