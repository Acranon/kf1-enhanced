#include <kf/platform/language_runtime.h>
#include <kf/platform/translation.h>
#include <kf/platform/disc.h>
#include <kf/platform/files.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
bool payload = true, prepare_ok = true;
unsigned preparations;
std::string active_root, generated, status;
}
namespace kf {
bool translation_available() { return payload; }
void host_language_status(const char *message) { status = message; }
bool data_files_set_root(const char *path) {
    if (std::string(path) == "unreadable") return false;
    active_root = path;
    return true;
}
bool disc_verify_directory(const char *path, Language, Language *) { return std::string(path) != "corrupt"; }
bool disc_prepare_directory(const char *, const char *destination, Language language) {
    assert(language == Language::English);
    ++preparations;
    generated = destination;
    std::filesystem::create_directory(destination);
    std::ofstream(generated + "/partial") << "fixture";
    return prepare_ok;
}
}

int main(int argc, char **argv) {
    assert(argc == 2);
    using kf::Language;
    const std::string scenario = argv[1];
    if (scenario == "switch") {
        assert(kf::language_resources_start("japanese", Language::Japanese, nullptr));
        assert(kf::language_request(Language::English));
        assert(kf::game_language() == Language::Japanese && active_root == "japanese");
        assert(kf::language_apply_pending());
        assert(kf::game_language() == Language::English && active_root == generated);
        assert(kf::language_request(Language::Japanese) && kf::language_apply_pending());
        assert(active_root == "japanese");
        assert(kf::language_request(Language::English) && kf::language_apply_pending());
        assert(preparations == 1);
        assert(!kf::language_apply_pending());
        kf::language_resources_stop();
        assert(!std::filesystem::exists(std::filesystem::path(generated).parent_path()));
    } else if (scenario == "failed-generation") {
        prepare_ok = false;
        assert(kf::language_resources_start("japanese", Language::Japanese, nullptr));
        assert(kf::language_request(Language::English));
        assert(!kf::language_apply_pending());
        assert(kf::game_language() == Language::Japanese && active_root == "japanese");
        assert(kf::language_requested() == Language::Japanese);
        assert(!std::filesystem::exists(std::filesystem::path(generated).parent_path()));
        prepare_ok = true;
        assert(kf::language_request(Language::English) && kf::language_apply_pending());
        assert(preparations == 2);
    } else if (scenario == "unavailable") {
        payload = false;
        assert(kf::language_resources_start("japanese", Language::Japanese, nullptr));
        assert(!kf::language_request(Language::English));
        assert(!kf::language_apply_pending() && preparations == 0);
        assert(kf::language_resources_start("english", Language::English, nullptr));
        assert(!kf::language_request(Language::Japanese));
        assert(kf::game_language() == Language::English && active_root == "english");
    } else if (scenario == "invalid-alternate") {
        for (const char *path : {"corrupt", "unreadable"}) {
            assert(kf::language_resources_start("english", Language::English, path));
            assert(kf::language_request(Language::Japanese));
            assert(!kf::language_apply_pending());
            assert(kf::game_language() == Language::English && active_root == "english");
            assert(!status.empty());
        }
    } else if (scenario == "cancel-pending") {
        assert(kf::language_resources_start("japanese", Language::Japanese, nullptr));
        assert(kf::language_request(Language::English));
        assert(kf::language_request(Language::Japanese));
        assert(!kf::language_apply_pending() && preparations == 0);
    } else {
        return 2;
    }
    kf::language_resources_stop();
}
