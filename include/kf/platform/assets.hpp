#pragma once
#include <kf/lib/codec.h>
#include <kf/lib/types.h>
#include <cstddef>

namespace kf {
inline constexpr std::size_t asset_path_capacity = 128;
inline constexpr std::size_t sha256_state_words = 8;
inline constexpr std::size_t sha256_block_bytes = 64;
inline constexpr std::size_t sha256_hex_capacity = 65;
inline constexpr std::size_t disc_directory_capacity = 128;
inline constexpr std::size_t disc_file_capacity = 4096;
inline constexpr std::size_t disc_status_capacity = 256;
inline constexpr std::size_t retail_resource_file_count = 428;
struct ByteBuffer {
    u8 *data;
    std::size_t size, capacity;
};
bool buffer_resize(ByteBuffer *buffer, std::size_t size);
void buffer_release(ByteBuffer *buffer);

struct Image {
    u32 width, height;
    ByteBuffer rgba;
};
bool image_decode_tim(Image *image, const u8 *data, std::size_t size, std::size_t offset = 0,
                      u32 palette = 0);
void image_release(Image *image);
bool asset_path(char *output, std::size_t capacity, const char *input);

struct Sha256 {
    u32 state[sha256_state_words];
    u8 pending[sha256_block_bytes];
    std::uint64_t length;
    std::size_t used;
};
void sha256_init(Sha256 *hash);
void sha256_update(Sha256 *hash, const u8 *bytes, std::size_t size);
void sha256_finish(const Sha256 *hash, char output[sha256_hex_capacity]);

struct Asset {
    char path[asset_path_capacity];
    ByteBuffer bytes;
};
struct AssetTable {
    Asset *entries;
    std::size_t count, capacity;
};
bool assets_append(AssetTable *table, const char *path, ByteBuffer *bytes);
const Asset *assets_find(const AssetTable *table, const char *path);
void assets_release(AssetTable *table);

inline constexpr std::size_t disc_import_limit = 128 * 1024 * 1024;

enum class ImportState : u8 { idle, layout, volume, directory, file, complete, failed };
struct ReadRequest {
    std::uint64_t offset;
    u32 length;
};
struct DiscExtent {
    char path[asset_path_capacity];
    u32 sector, length;
};
struct DiscImporter {
    ImportState state;
    ReadRequest request;
    std::uint64_t disc_size;
    u32 sector_size, volume_sectors;
    std::size_t file_bytes;
    char message[disc_status_capacity];
    DiscExtent directories[disc_directory_capacity];
    std::size_t directory_count;
    u32 visited_directories[disc_directory_capacity];
    std::size_t visited_count;
    DiscExtent files[disc_file_capacity];
    std::size_t file_count, file_index;
    DiscExtent current;
    AssetTable assets;
};
// Sorted paths: LE32 path length, LE32 file length, path bytes, then file bytes.
// Identity metadata only; extracted resources remain ordinary, unchanged files.
inline constexpr const char *retail_files_sha256 =
    "450b9f09ca34bedc1c8bc150e01b79bfe108c700dad2b2783246b926953deee2";
// The same digest over every file except GAME.EXE, for the English translation
// v1.0: GAME.EXE differs between v1.0 and v1.0 with HwitVlf's save fix, and the
// port never runs it (its menu labels are native, see menu_glyphs.h).
inline constexpr const char *english_resources_sha256 =
    "3fafe2518f907b6feb13fe083975a2713ad678e5e65b36336a1161d2acb9ba76";
// Japanese: the untouched disc. English: the translation patch v1.0 (with or
// without the save fix). Modified: the same file set with other changes.
// Foreign: anything else. Sorts the table by path.
enum class DiscMatch { Japanese, English, Modified, Foreign };
DiscMatch assets_classify(AssetTable *table);
void disc_import_start(DiscImporter *importer, std::uint64_t disc_size);
bool disc_import_waiting(const DiscImporter *importer);
void disc_import_supply(DiscImporter *importer, const u8 *bytes, std::size_t size);
void disc_import_fail(DiscImporter *importer, const char *message);
double disc_import_progress(const DiscImporter *importer);
void disc_import_release(DiscImporter *importer);
} // namespace kf
