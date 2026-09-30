#include <kf/platform/prelude.h>
#include <kf/game/resource_file.h>
#include <kf/game/resources.h>

#include <array>

static constexpr s32 item_models_per_directory = 30;

std::array<KfItemModelFile, KF_ITEM_COUNT> item_model_files;

void resource_file_index_item_models()
{
    for (s32 i = 0; i < KF_ITEM_COUNT; ++i) {
        auto *entry = &item_model_files[i];
        snprintf(entry->path.data(), entry->path.size(), "ITEM%d/I%03d.TMD", i / item_models_per_directory + 1, i + 1);
        entry->size = 0;
        kf::DataFile file {};
        const auto result = resource_file_open(&file, entry->path.data());
        if (result == kf::FileResult::Ok)
            entry->size = file.size;
        else if (result != kf::FileResult::NotFound)
            kf::data_file_report_error(result, entry->path.data());
        kf::data_file_close(&file);
    }
}

KfResourceLoadResult resource_file_load_item_model(u8 **destination, s32 index, std::size_t *loaded_size)
{
    *destination = NULL;
    *loaded_size = 0;
    if (index < 0 || index >= KF_ITEM_COUNT || !item_model_files[index].size)
        return KF_RESOURCE_LOAD_FAILED;
    return resource_file_try_load_allocated(memory_arena, destination, item_model_files[index].path.data(), loaded_size);
}

void resource_file_reset_module_state(void)
{
    kf::restore_initial_value<item_model_files>();
}
