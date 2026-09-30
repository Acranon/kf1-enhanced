#include "bytes.h"

#include <algorithm>
#include <optional>

namespace {
using namespace kf::codec;

enum class PixelFormat : u8 { Indexed4, Indexed8, Direct16, Direct24 };
constexpr u32 tim_magic = 0x10;
constexpr u32 clut_flag = 1 << 3;
constexpr u16 texture_width = 1024, texture_height = 512;

struct Block {
    s16 x, y;
    u16 width, height;
    Bytes pixels;
};
struct Image {
    PixelFormat format;
    std::optional<Block> clut;
    Block pixels;
    u32 encoded_bytes;
};

Block read_block(Reader &input)
{
    // Low two size bits are ignored by the authored transfer format.
    const u32 size = input.u32_le() & ~3u;
    require(size >= 12, "invalid TIM block size");
    Reader block(input.take(size - 4));
    const auto x = block.s16_le(), y = block.s16_le();
    const auto width = block.s16_le(), height = block.s16_le();
    require(width >= 0 && height >= 0, "invalid TIM rectangle");
    return {x, y, static_cast<u16>(width), static_cast<u16>(height),
        block.take(product(product(width, height), 2))};
}

std::optional<Image> read_image(Reader &input)
{
    if (input.remaining() < 4 || input.u32_le() != tim_magic)
        return std::nullopt;
    const auto start = input.position() - 4;
    const auto mode = input.u32_le();
    require((mode & 7) <= 3 && (mode & ~(7u | clut_flag)) == 0, "unsupported TIM mode");
    const auto format = static_cast<PixelFormat>(mode & 7);
    std::optional<Block> clut;
    if (mode & clut_flag)
        clut = read_block(input);
    const auto pixels = read_block(input);
    const auto size = input.position() - start;
    require(size <= UINT32_MAX, "TIM size exceeds format limit");
    return Image {format, clut, pixels, static_cast<u32>(size)};
}

Image image_at(Bytes bytes, std::size_t offset)
{
    auto reader = reader_at(bytes, offset);
    auto image = read_image(reader);
    if (!image)
        throw Error {KF_CODEC_END, {}, std::source_location::current()};
    return *image;
}

std::pair<u32, u32> dimensions(const Image &image)
{
    u32 width = image.pixels.width;
    const u32 height = image.pixels.height;
    switch (image.format) {
    case PixelFormat::Indexed4: width *= 4; break;
    case PixelFormat::Indexed8: width *= 2; break;
    case PixelFormat::Direct16: break;
    case PixelFormat::Direct24:
        require(width * 2 % 3 == 0, "invalid TIM dimensions");
        width = width * 2 / 3;
        break;
    }
    require(width != 0 && height != 0, "empty TIM image");
    return {width, height};
}

void validate_rectangle(const Block &block)
{
    require(block.x >= 0 && block.x < texture_width && block.y >= 0 && block.y < texture_height &&
        block.width <= texture_width && block.height <= texture_height, "invalid texture rectangle");
}

void copy_block(const Block &block, std::span<u16> words)
{
    Reader pixels(block.pixels);
    for (std::size_t y = 0; y < block.height; ++y) {
        for (std::size_t x = 0; x < block.width; ++x) {
            // Authored CLUT transfers cross row 511 and wrap at the image edges.
            const auto row = (std::size_t(block.y) + y) % texture_height;
            const auto column = (std::size_t(block.x) + x) % texture_width;
            words[row * texture_width + column] = pixels.u16_le();
        }
    }
}
} // namespace

KfCodecResult kf_tim_info(std::span<const u8> bytes, std::size_t offset, KfTimInfo &info)
{
    return decode([&] {
        const auto image = image_at(bytes, offset);
        const auto [width, height] = dimensions(image);
        info = {u32(image.format) | (image.clut ? clut_flag : 0), width, height, image.encoded_bytes,
            image.pixels.x, image.pixels.y, image.clut ? image.clut->x : 0, image.clut ? image.clut->y : 0};
    });
}

KfCodecResult kf_tim_rgba(std::span<const u8> bytes, std::size_t offset, u32 palette_row,
    std::span<u8> rgba)
{
    return decode([&] {
        const auto image = image_at(bytes, offset);
        const auto [width, height] = dimensions(image);
        const auto count = product(width, height);
        output_fits(rgba.size() >= product(count, 4));
        Bytes palette;
        if (image.format == PixelFormat::Indexed4 || image.format == PixelFormat::Indexed8) {
            require(image.clut.has_value(), "indexed TIM has no palette");
            const std::size_t colors = image.format == PixelFormat::Indexed4 ? 16 : 256;
            palette = slice(image.clut->pixels, product(palette_row, colors * 2), colors * 2);
        }
        Reader pixels(image.pixels.pixels);
        u8 packed = 0;
        for (std::size_t index = 0; index < count; ++index) {
            auto output = rgba.subspan(index * 4, 4);
            if (image.format == PixelFormat::Direct24) {
                std::ranges::copy(pixels.take(3), output.begin());
                output[3] = 255;
                continue;
            }
            u16 word;
            if (image.format == PixelFormat::Direct16) {
                word = pixels.u16_le();
            } else {
                u8 entry;
                if (image.format == PixelFormat::Indexed4) {
                    if (index % 2 == 0)
                        packed = pixels.byte();
                    entry = field<0, 4>(packed >> ((index % 2) * 4));
                } else {
                    entry = pixels.byte();
                }
                word = reader_at(palette, std::size_t(entry) * 2).u16_le();
            }
            const std::array channels {field<0, 5>(word), field<5, 5>(word), field<10, 5>(word)};
            for (std::size_t channel = 0; channel < channels.size(); ++channel)
                output[channel] = (channels[channel] << 3) | (channels[channel] >> 2);
            output[3] = word == 0 ? 0 : 255;
        }
    });
}

KfCodecResult kf_tim_compose(std::span<const u8> bytes, std::span<u16> words)
{
    return decode([&] {
        output_fits(words.size() >= texture_width * texture_height);
        Reader validation(bytes);
        bool found = false;
        while (auto image = read_image(validation)) {
            require(image->format != PixelFormat::Direct24, "unsupported texture format");
            validate_rectangle(image->pixels);
            if (image->clut)
                validate_rectangle(*image->clut);
            found = true;
        }
        require(found, "texture contains no TIM images");
        Reader input(bytes);
        while (auto image = read_image(input)) {
            if (image->clut)
                copy_block(*image->clut, words);
            copy_block(image->pixels, words);
        }
    });
}
