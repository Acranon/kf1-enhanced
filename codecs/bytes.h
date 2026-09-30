#ifndef KF_CODECS_BYTES_H
#define KF_CODECS_BYTES_H

#include <kf/lib/codec.h>

#include <bit>
#include <cstdio>
#include <limits>
#include <source_location>
#include <string>

namespace kf::codec {

using Bytes = std::span<const u8>;

struct Error {
    KfCodecResult result;
    std::string message;
    std::source_location location;
};

inline void require(bool valid, const char *message,
    std::source_location location = std::source_location::current())
{
    if (!valid)
        throw Error {KF_CODEC_INVALID, message, location};
}

inline void output_fits(bool fits,
    std::source_location location = std::source_location::current())
{
    if (!fits)
        throw Error {KF_CODEC_OUTPUT_FULL, "codec output is full", location};
}

template<class Function>
KfCodecResult decode(Function function)
{
    try {
        function();
        return KF_CODEC_OK;
    } catch (const Error &error) {
        if (error.result != KF_CODEC_END)
            std::fprintf(stderr, "kf-codec: %s:%u:%u: %s\n", error.location.file_name(),
                error.location.line(), error.location.column(), error.message.c_str());
        return error.result;
    }
}

inline std::size_t product(std::size_t count, std::size_t width,
    std::source_location location = std::source_location::current())
{
    require(width == 0 || count <= std::numeric_limits<std::size_t>::max() / width,
        "byte count overflow", location);
    return count * width;
}

inline Bytes slice(Bytes bytes, std::size_t at, std::size_t count,
    std::source_location location = std::source_location::current())
{
    if (at > bytes.size() || count > bytes.size() - at) {
        throw Error {KF_CODEC_INVALID,
            "truncated input at " + std::to_string(at) + ": need " + std::to_string(count) +
                " bytes, have " + std::to_string(at > bytes.size() ? 0 : bytes.size() - at),
            location};
    }
    return bytes.subspan(at, count);
}

// Reads values, never aligned references into a file. The cursor advances only
// after a successful bounds check; callers provide the diagnostic source location.
class Reader {
public:
    explicit Reader(Bytes bytes) : bytes_(bytes) {}
    std::size_t remaining() const { return bytes_.size() - position_; }
    std::size_t position() const { return position_; }
    Bytes take(std::size_t count,
        std::source_location location = std::source_location::current())
    {
        auto result = slice(bytes_, position_, count, location);
        position_ += count;
        return result;
    }
    void skip(std::size_t count,
        std::source_location location = std::source_location::current()) { take(count, location); }
    u8 byte(std::source_location location = std::source_location::current()) { return take(1, location)[0]; }
    u16 u16_le(std::source_location location = std::source_location::current())
    {
        const auto b = take(2, location);
        return u16(b[0]) | (u16(b[1]) << 8);
    }
    s16 s16_le(std::source_location location = std::source_location::current())
    {
        return std::bit_cast<s16>(u16_le(location));
    }
    u32 u32_le(std::source_location location = std::source_location::current())
    {
        const auto b = take(4, location);
        return u32(b[0]) | (u32(b[1]) << 8) | (u32(b[2]) << 16) | (u32(b[3]) << 24);
    }
    u16 u16_be(std::source_location location = std::source_location::current())
    {
        const auto b = take(2, location);
        return (u16(b[0]) << 8) | u16(b[1]);
    }
    u32 u24_be(std::source_location location = std::source_location::current())
    {
        const auto b = take(3, location);
        return (u32(b[0]) << 16) | (u32(b[1]) << 8) | u32(b[2]);
    }
    u32 u32_be(std::source_location location = std::source_location::current())
    {
        const auto b = take(4, location);
        return (u32(b[0]) << 24) | (u32(b[1]) << 16) | (u32(b[2]) << 8) | u32(b[3]);
    }
private:
    Bytes bytes_;
    std::size_t position_ = 0;
};

inline Reader reader_at(Bytes bytes, std::size_t offset,
    std::source_location location = std::source_location::current())
{
    slice(bytes, offset, 0, location);
    return Reader(bytes.subspan(offset));
}

template<u8 Start, u8 Width>
constexpr u8 field(u16 value)
{
    static_assert(Width > 0 && Width <= 8 && Start + Width <= 16);
    return static_cast<u8>((value >> Start) & ((1u << Width) - 1));
}

} // namespace kf::codec

#endif // KF_CODECS_BYTES_H
