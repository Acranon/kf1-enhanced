#include <kf/renderer/textures.hpp>
#include <kf/renderer/renderer.hpp>
#include <cstdlib>
#include <limits>

namespace kf {
static constexpr std::size_t initial_texture_capacity = 32;
static constexpr std::size_t texture_word_count = texture_store_width * texture_store_height;

bool texture_store_upload_tim(TextureStore *store, const u8 *bytes, std::size_t size) {
    if (!store->words) {
        store->words = static_cast<u16 *>(std::calloc(texture_word_count, sizeof(u16)));
        if (!store->words)
            return false;
    }
    // The codec validates the entire stream before changing any texels, and
    // copies them before the original loader can rewind its resource arena.
    if (kf_tim_compose(bytes, size, store->words, texture_word_count) != KF_CODEC_OK)
        return false;
    for (std::size_t i = 0; i < store->count; ++i)
        store->entries[i].dirty = true;
    return true;
}

bool texture_store_translate_tim(TextureStore *store, const u8 *before, std::size_t before_size,
                                 const u8 *after, std::size_t after_size) {
    if (!store->words)
        return false;
    auto *old_words = static_cast<u16 *>(std::calloc(texture_word_count * 2, sizeof(u16)));
    if (!old_words)
        return false;
    auto *new_words = old_words + texture_word_count;
    const bool valid = kf_tim_compose(before, before_size, old_words, texture_word_count) == KF_CODEC_OK &&
        kf_tim_compose(after, after_size, new_words, texture_word_count) == KF_CODEC_OK;
    if (valid) {
        for (std::size_t i = 0; i < texture_word_count; ++i)
            if (old_words[i] != new_words[i])
                store->words[i] = new_words[i];
        for (std::size_t i = 0; i < store->count; ++i)
            store->entries[i].dirty = true;
    }
    std::free(old_words);
    return valid;
}

bool texture_decode(Image *image, TextureSource source, const u16 *words, std::size_t count) {
    const auto mode = static_cast<unsigned>(source.format);
    if (!words || count < texture_word_count || source.format > TextureFormat::Direct16 || source.x >= texture_store_width ||
        source.y >= texture_store_height || source.palette_x >= texture_store_width || source.palette_y >= texture_store_height)
        return false;
    Image decoded{texture_page_extent, texture_page_extent, {}};
    if (!buffer_resize(&decoded.rgba, texture_page_extent * texture_page_extent * 4))
        return false;
    const unsigned pixels_per_word = 4 >> mode;
    for (unsigned y = 0; y < texture_page_extent; ++y) {
        for (unsigned x = 0; x < texture_page_extent; ++x) {
            auto value = words[((source.y + y) & (texture_store_height - 1)) * texture_store_width +
                               ((source.x + x / pixels_per_word) & (texture_store_width - 1))];
            if (source.format < TextureFormat::Direct16) {
                const unsigned bits = source.format == TextureFormat::Indexed4 ? 4 : 8;
                const unsigned index =
                    (value >> ((x % pixels_per_word) * bits)) & ((1u << bits) - 1);
                value = words[source.palette_y * texture_store_width + ((source.palette_x + index) & (texture_store_width - 1))];
            }
            auto *pixel = decoded.rgba.data + (y * texture_page_extent + x) * 4;
            for (unsigned c = 0; c < 3; ++c) {
                const unsigned component = (value >> (c * color5_bits)) & color5_max;
                pixel[c] = static_cast<u8>((component << color5_color8_shift) | (component >> color5_replication_shift));
            }
            // Preserve the per-texel transparency category, not ordinary opacity.
            pixel[3] = value == 0 ? 0 : ((value & texture_semitransparent_bit) ? texture_alpha_semitransparent_marker : texture_alpha_opaque_marker);
        }
    }
    image_release(image);
    *image = decoded;
    return true;
}

static bool same_source(TextureSource a, TextureSource b) {
    return a.x == b.x && a.y == b.y && a.palette_x == b.palette_x &&
           a.palette_y == b.palette_y && a.format == b.format;
}

u32 texture_store_resolve(TextureStore *store, TextureSource source) {
    if (source.format == TextureFormat::Direct16)
        source.palette_x = source.palette_y = 0;
    std::size_t index = 0;
    while (index < store->count && !same_source(store->entries[index].source, source))
        ++index;
    if (index == store->count) {
        if (store->count == store->capacity) {
            if (store->capacity > std::numeric_limits<std::size_t>::max() / 2 / sizeof(TextureEntry))
                return 0;
            const auto capacity = store->capacity ? store->capacity * 2 : initial_texture_capacity;
            auto *entries = static_cast<TextureEntry *>(std::realloc(store->entries, capacity * sizeof(TextureEntry)));
            if (!entries)
                return 0;
            store->entries = entries;
            store->capacity = capacity;
        }
        store->entries[store->count++] = {source, 0, true};
    }
    auto *entry = &store->entries[index];
    if (entry->dirty) {
        Image decoded{};
        if (!texture_decode(&decoded, source, store->words, texture_word_count))
            return 0;
        const auto texture = renderer_upload(&decoded);
        image_release(&decoded);
        if (!texture)
            return 0;
        renderer_delete_texture(entry->texture);
        entry->texture = texture;
        entry->dirty = false;
    }
    return entry->texture;
}

void texture_store_release(TextureStore *store) {
    for (std::size_t i = 0; i < store->count; ++i)
        renderer_delete_texture(store->entries[i].texture);
    std::free(store->entries);
    std::free(store->words);
    *store = {};
}
}
