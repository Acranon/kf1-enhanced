#pragma once
#include <kf/platform/assets.hpp>
#include <cstddef>

namespace kf {
// The supported retail CUE is one MODE2/2352 track at file offset zero.
bool disc_cue_image(const char *text, char *filename, std::size_t capacity);
// Reads every game file of a disc image into memory and says which release it is.
bool disc_import_image(const char *source, AssetTable *assets, DiscMatch *match);
// Writes the files into a new directory (never over an existing one).
bool resource_tree_write(const AssetTable &assets, const char *destination);
// Reads the retail file list back from an extracted directory.
bool resource_tree_read(const char *directory, AssetTable *assets);
// Verify first, then write unchanged files into a new directory. Never overwrite.
bool disc_extract(const char *source, const char *destination);
}
