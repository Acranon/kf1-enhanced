#include <kf/platform/prelude.h>
#include <kf/lib/graphics.h>

void display_begin_frame(KfDisplayState &display)
{
    display.buffer_index = display_next_buffer(display.buffer_index);
    kf::host_begin_frame();
}

void display_present_frame(const KfDisplayState &display)
{
    kf::host_wait_frame();
    kf::host_present_frame(display.frame_style);
}
