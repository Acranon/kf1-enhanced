#include <kf/platform/prelude.h>
#include <kf/lib/memory.h>
#include <kf/lib/resource_file.h>
#include <kf/platform/files.h>

#include <array>

static constexpr std::size_t resource_path_capacity = 128;

kf::FileResult resource_file_open(kf::DataFile *file, const char *relative_path)
{
    std::array<char, resource_path_capacity> path;
    const int count = snprintf(path.data(), path.size(), "KF/%s", relative_path);
    if (count < 0 || static_cast<std::size_t>(count) >= path.size())
        return kf::FileResult::InvalidPath;
    return kf::data_file_open(file, path.data());
}

KfResourceLoadResult resource_file_try_load_allocated(KfMemoryArena &arena, u8 **destination, const char *relative_path,
                                                    std::size_t *loaded_size)
{
    *destination = NULL;
    if (loaded_size)
        *loaded_size = 0;
    kf::DataFile file {};
    auto result = resource_file_open(&file, relative_path);
    if (result == kf::FileResult::Ok) {
        if (!file.size) {
            result = kf::FileResult::IoError;
        } else {
            auto *data = static_cast<u8 *>(memory_allocate(arena, file.size));
            if (!data) {
                result = kf::FileResult::OutOfMemory;
            } else {
                result = kf::data_file_read(&file, data, file.size);
                if (result == kf::FileResult::Ok) {
                    *destination = data;
                    if (loaded_size)
                        *loaded_size = file.size;
                } else
                    memory_release_last(arena);
            }
        }
    }
    kf::data_file_close(&file);
    if (result == kf::FileResult::Ok)
        return KF_RESOURCE_LOADED;
    kf::data_file_report_error(result, relative_path);
    return KF_RESOURCE_LOAD_FAILED;
}

[[noreturn]] void resource_file_fail(const char *relative_path)
{
    std::array<char, 192> message;
    std::snprintf(message.data(), message.size(), "Cannot load required resource KF/%s.", relative_path);
    kf::host_fail(message.data());
}

void resource_file_load_allocated(KfMemoryArena &arena, u8 **destination, const char *relative_path, std::size_t *loaded_size)
{
    // These callers immediately parse required resources. Do not continue with stale data.
    if (resource_file_try_load_allocated(arena, destination, relative_path, loaded_size) != KF_RESOURCE_LOADED)
        resource_file_fail(relative_path);
}

KfResourceLoadResult resource_file_load_into(void *destination, std::size_t capacity, const char *relative_path,
                                           std::size_t *loaded_size)
{
    if (loaded_size)
        *loaded_size = 0;
    kf::DataFile file {};
    auto result = resource_file_open(&file, relative_path);
    if (result == kf::FileResult::Ok)
        result = kf::data_file_read(&file, destination, capacity);
    if (result == kf::FileResult::Ok && loaded_size)
        *loaded_size = file.size;
    kf::data_file_close(&file);
    if (result == kf::FileResult::Ok)
        return KF_RESOURCE_LOADED;
    kf::data_file_report_error(result, relative_path);
    return KF_RESOURCE_LOAD_FAILED;
}
