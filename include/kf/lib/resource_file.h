#ifndef KF_RESOURCE_FILE_H
#define KF_RESOURCE_FILE_H

#include <kf/lib/memory.h>
#include <kf/lib/types.h>
#include <kf/platform/files.h>

#include <cstddef>

enum class KfResourceLoadResult : s32 {
    KF_RESOURCE_LOADED = 0,
    KF_RESOURCE_LOAD_FAILED = 1
}; using enum KfResourceLoadResult;

constexpr void resource_path_write_decimal3(char *digits, s32 number)
{
    digits[0] = static_cast<char>(number / 100 + '0');
    digits[1] = static_cast<char>((number % 100) / 10 + '0');
    digits[2] = static_cast<char>((number % 100) % 10 + '0');
}

// Resource paths are relative to the extracted KF directory.
[[noreturn]] void resource_file_fail(const char *relative_path);
kf::FileResult resource_file_open(kf::DataFile *file, const char *relative_path);
KfResourceLoadResult resource_file_try_load_allocated(KfMemoryArena &arena, u8 **destination, const char *relative_path,
                                                    std::size_t *loaded_size = nullptr);
void resource_file_load_allocated(KfMemoryArena &arena, u8 **destination, const char *relative_path,
                                 std::size_t *loaded_size = nullptr);
KfResourceLoadResult resource_file_load_into(
    void *destination, std::size_t capacity, const char *relative_path,
    std::size_t *loaded_size = nullptr);

#endif // KF_RESOURCE_FILE_H
