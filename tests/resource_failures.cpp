#include "../src/lib/resource_file.cpp"
#include "../src/platform/files.cpp"
#include <cassert>
#include <kf/platform/prelude.h>

// A missing file must fail before allocating or releasing any game storage.
void *memory_allocate(KfMemoryArena &, std::size_t) { std::abort(); }
void memory_release_last(KfMemoryArena &) { std::abort(); }

namespace kf
{
[[noreturn]] void host_fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    // Distinguish the visible host error path from a bare exit(1).
    std::exit(77);
}
} // namespace kf

int main(int argc, char **argv)
{
    assert(argc == 3 && kf::data_files_set_root(argv[2]));
    KfMemoryArena arena{};
    u8 *data = nullptr;
    std::size_t size = 123;
    if (std::strcmp(argv[1], "required") == 0)
        resource_file_load_allocated(arena, &data, "missing.dat", &size);
    else {
        assert(resource_file_try_load_allocated(arena, &data, "missing.dat", &size) ==
               KF_RESOURCE_LOAD_FAILED);
        assert(data == nullptr && size == 0);
    }
}
