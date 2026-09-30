#ifndef KF_DEBUG_H
#define KF_DEBUG_H

#include <kf/lib/enum.h>
#include <kf/lib/types.h>

enum class KfFormatDigitState : u8 {
    KF_FORMAT_DIGITS_LEADING = 0,
    KF_FORMAT_DIGITS_EMITTED = 1
}; using enum KfFormatDigitState;

enum class KfFormatParserState : u8 {
    KF_FORMAT_PARSER_TEXT = 0,
    KF_FORMAT_PARSER_CONVERSION = 1
}; using enum KfFormatParserState;

enum class KfFormatPaddingMode : u8 {
    KF_FORMAT_PAD_SPACES = 0,
    KF_FORMAT_PAD_ZEROES = 1
}; using enum KfFormatPaddingMode;

enum {
    KF_FORMAT_LEADING_PAD_BYTES = 8,
    KF_FORMAT_DECIMAL_DIGITS = 10,
    KF_FORMAT_DECIMAL_HIGHEST_PLACE = 1000000000,
    KF_FORMAT_HEX_DIGITS = 8,
    KF_FORMAT_HEX_HIGHEST_PLACE = 0x10000000,
    KF_FORMAT_WIDTH_UNSPECIFIED = 0xff
};

extern void debug_printf_sink(const char *format, ...);

void format_reset_module_state(void);

#endif // KF_DEBUG_H
