#include <kf/platform/prelude.h>
#include <kf/lib/debug.h>
#include <kf/lib/types.h>

#include <stdarg.h>

bool debug_stop_flag = false;

void debug_stop(void)
{
    debug_printf_sink("DEBUG STOP !!!\n");
    debug_stop_flag = debug_stop_flag == false;
}

void func_8003ac4c(void)
{

}

void debug_text_reset_module_state(void)
{
    kf::restore_initial_value<debug_stop_flag>();
}
