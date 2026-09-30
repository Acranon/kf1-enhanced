#include <kf/platform/prelude.h>
#include <kf/lib/debug.h>

#include <array>
#include <stdarg.h>

static constexpr unsigned format_number_capacity = 24;
static std::array<char, format_number_capacity> format_number_storage;

char *format_int_dec(s32 value)
{
    s32 divisor = KF_FORMAT_DECIMAL_HIGHEST_PLACE;
    char *out = format_number_storage.data() + KF_FORMAT_LEADING_PAD_BYTES;
    KfFormatDigitState digit_state = KF_FORMAT_DIGITS_LEADING;
    u8 i;

    if (value < 0) {
        *out++ = '-';
        value = -value;
    }
    for (i = 0; i < KF_FORMAT_DECIMAL_DIGITS; i++) {
        s32 digit = value / divisor;
        value = value % divisor;
        if (digit != 0 || digit_state != KF_FORMAT_DIGITS_LEADING || i == KF_FORMAT_DECIMAL_DIGITS - 1) {
            *out++ = digit + '0';
            digit_state = KF_FORMAT_DIGITS_EMITTED;
        }
        divisor /= 10;
    }
    *out = '\0';
    return format_number_storage.data() + KF_FORMAT_LEADING_PAD_BYTES;
}

char *format_int_hex(u32 value)
{
    u32 divisor = KF_FORMAT_HEX_HIGHEST_PLACE;
    KfFormatDigitState digit_state = KF_FORMAT_DIGITS_LEADING;
    char *out = format_number_storage.data() + KF_FORMAT_LEADING_PAD_BYTES;
    u8 i;

    for (i = 0; i < KF_FORMAT_HEX_DIGITS; i++) {
        u32 digit = value / divisor;
        value = value % divisor;
        if (digit != 0 || digit_state != KF_FORMAT_DIGITS_LEADING || i == KF_FORMAT_HEX_DIGITS - 1) {
            if (digit < 10) {
                *out++ = digit + '0';
            } else {
                *out++ = digit + ('A' - 10);
            }
            digit_state = KF_FORMAT_DIGITS_EMITTED;
        }
        divisor >>= 4;
    }
    *out = '\0';
    return format_number_storage.data() + KF_FORMAT_LEADING_PAD_BYTES;
}

char *format_pad_left(char *string, char pad, u8 width)
{
    u8 len = 0;
    char *p = string + 1;

    if (*string != 0) {
        do {
            len++;
        } while (*p++ != 0);
    }
    if (len < width) {
        width -= len;
        while (width-- != 0) {
            *--string = pad;
        }
    }
    return string;
}

static s32 format_copy_string(u8 *&out, const char *string)
{
    s32 count = 0;
    u8 c;

    while ((c = *string++) != 0) {
        *out++ = c;
        count++;
    }
    return count;
}

s32 format_vsprintf(u8 *out, u8 *format, va_list args)
{
    s32 count = 0;
    KfFormatParserState parser_state = KF_FORMAT_PARSER_TEXT;
    u8 width;
    KfFormatPaddingMode padding_mode;
    u8 c;
    char *s;

    while ((c = *format++) != 0) {
        if (c >= '1' && c <= '8') {
            if (parser_state != KF_FORMAT_PARSER_TEXT) {
                width = c - '0';
                continue;
            }
        } else {
            switch (c) {
            case '%':
                parser_state = KF_FORMAT_PARSER_CONVERSION;
                padding_mode = KF_FORMAT_PAD_SPACES;
                width = KF_FORMAT_WIDTH_UNSPECIFIED;
                continue;
            case '0':
                if (parser_state == KF_FORMAT_PARSER_TEXT) {
                    break;
                }
                padding_mode = KF_FORMAT_PAD_ZEROES;
                continue;
            case 'D':
            case 'd':
            case 'X':
            case 'x':
                if (parser_state == KF_FORMAT_PARSER_TEXT) {
                    break;
                }
                parser_state = KF_FORMAT_PARSER_TEXT;
                s = (c == 'D' || c == 'd')
                    ? format_int_dec(va_arg(args, s32))
                    : format_int_hex(va_arg(args, u32));
                if (width != KF_FORMAT_WIDTH_UNSPECIFIED) {
                    if (padding_mode == KF_FORMAT_PAD_SPACES) {
                        s = format_pad_left(s, ' ', width);
                    } else {
                        s = format_pad_left(s, '0', width);
                    }
                }
                count += format_copy_string(out, s);
                continue;
            case 'S':
            case 's':
                if (parser_state == KF_FORMAT_PARSER_TEXT) {
                    break;
                }
                parser_state = KF_FORMAT_PARSER_TEXT;
                s = va_arg(args, char *);
                count += format_copy_string(out, s);
                continue;
            case '\n':
                *out++ = '\r';
                count++;
                continue;
            }
        }
        *out++ = c;
        count++;
    }
    *out = '\0';
    return count + 1;
}

// Retail intentionally discards these variadic diagnostics.
void debug_printf_sink(const char *, ...)
{
}

void format_reset_module_state(void)
{
    kf::restore_initial_value<format_number_storage>();
}
