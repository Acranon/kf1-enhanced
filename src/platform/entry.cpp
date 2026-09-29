#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <kf/platform/host.hpp>
#include <kf/platform/input.hpp>
#include <kf/platform/files.hpp>
#include <kf/platform/disc.hpp>
#include <kf/platform/saves.hpp>
#include <kf/platform/prelude.hpp>
// Game sources compile inside namespace kf::game (cmake/OriginalSources.cmake), so
// the label pointers the menus read are kf::game::menu_glyphs.
namespace kf::game {
#include <kf/game/menu_glyphs.h>
}
#if KF_EXT
#include <kf/ext/ext.hpp>
#endif
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <kf/platform/assets.hpp>

extern "C" EMSCRIPTEN_KEEPALIVE int kf_extract_disc(const char *source, const char *destination) {
    return kf::disc_extract(source, destination);
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *kf_retail_files_hash() {
    return kf::retail_files_sha256;
}
EM_JS(void, browser_game_started, (), { Module['gameStarted'](); });
#endif

// The English patch rewrites menu label immediates in GAME.EXE, which the source
// port does not execute. Detect it by the "use" label at 0x800283FC.
static bool english_patch_present() {
    constexpr std::size_t exe_header_bytes = 0x800;
    constexpr std::uint32_t load_address = 0x80012000;
    constexpr std::uint32_t use_label_address = 0x800283fc;
    constexpr std::uint32_t english_use_label = 0x34020000; // ori $v0, $zero, 0
    constexpr std::size_t offset = exe_header_bytes + (use_label_address - load_address);
    std::vector<unsigned char> exe(512 * 1024);
    std::size_t size = 0;
    if (kf::data_file_read_into("GAME.EXE", exe.data(), exe.size(), &size) != kf::FileResult::Ok || size < offset + 4)
        return false;
    const std::uint32_t word = exe[offset] | exe[offset + 1] << 8 | exe[offset + 2] << 16 |
        static_cast<std::uint32_t>(exe[offset + 3]) << 24;
    return word == english_use_label;
}

#if defined(_WIN32)
#include <filesystem>
#include <string>
// Release layout on Windows: the game data is copied out of the player's disc
// image into a "data" folder beside the executable on first launch.
namespace {
std::filesystem::path executable_directory() {
    const char *base = SDL_GetBasePath();
    return base ? std::filesystem::path(reinterpret_cast<const char8_t *>(base)) : std::filesystem::path(".");
}

bool is_disc_image(const std::filesystem::path &path) {
    auto extension = path.extension().u8string();
    for (auto &c : extension)
        c = static_cast<char8_t>(std::tolower(static_cast<unsigned char>(c)));
    return extension == u8".cue" || extension == u8".bin" || extension == u8".iso";
}

std::string utf8(const std::filesystem::path &path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

// A .cue beside the executable wins over a .bin or .iso.
std::string disc_beside(const std::filesystem::path &directory) {
    std::error_code error;
    std::string found;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error) || !is_disc_image(entry.path()))
            continue;
        if (entry.path().extension() == ".cue" || entry.path().extension() == ".CUE")
            return utf8(entry.path());
        if (found.empty())
            found = utf8(entry.path());
    }
    return found;
}

std::string ask_for_disc() {
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "King's Field",
        "Choose your King's Field (Japan) disc image with the English patch applied "
        "(.cue, .bin or .iso).\n\nThe game files are copied out of it once, into the "
        "\"data\" folder beside the game. The image itself is not changed.", nullptr);
    struct Pick { std::string path; bool done = false; } pick;
    static const SDL_DialogFileFilter filters[] = {{"Disc images", "cue;bin;iso"}};
    SDL_ShowOpenFileDialog([](void *data, const char *const *files, int) {
        auto *result = static_cast<Pick *>(data);
        if (files && files[0])
            result->path = files[0];
        result->done = true;
    }, &pick, nullptr, filters, 1, nullptr, false);
    while (!pick.done) {
        SDL_PumpEvents();
        SDL_Delay(20);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return pick.path;
}

void show_error(const char *text) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "King's Field", text, nullptr);
}

// Finds or creates the data folder. Returns false when the player cancelled or
// extraction failed (already reported).
bool prepare_release_data(const char *dropped_disc, std::string &data) {
    const auto directory = executable_directory() / "data";
    data = utf8(directory);
    std::error_code error;
    if (std::filesystem::exists(directory / "GAME.EXE", error))
        return true;
    std::string disc = dropped_disc ? dropped_disc : disc_beside(executable_directory());
    if (disc.empty())
        disc = ask_for_disc();
    if (disc.empty())
        return false;
    // A folder without GAME.EXE is a failed earlier extraction.
    std::filesystem::remove_all(directory, error);
    std::printf("Extracting game files from %s\n", disc.c_str());
    if (!kf::disc_extract(disc.c_str(), data.c_str())) {
        show_error("That disc image could not be read as King's Field (Japan, SLPS-00017).\n\n"
                   "Use the original Japanese disc with the English patch applied, as a .cue/.bin pair "
                   "or an .iso. Details are in kf.log in %APPDATA%\\KingsField\\SLPS00017.");
        std::filesystem::remove_all(directory, error);
        return false;
    }
    return true;
}
}
#endif

extern "C" kf::AppMode kf_run_game();
extern "C" void kf_run_opening(kf::AppMode mode);

int main(int argc, char **argv) {
#if KF_EXT
    kf::ext::console_start();
#endif
    const char *data = "data";
    const char *saves = nullptr;
    const char *disc = nullptr;
    const char *extracted = nullptr;
    bool data_selected = false, extract_only = false;
    bool skip_intro = false;
    const char *dropped_disc = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            data = argv[++i];
            data_selected = true;
        }
        else if (std::strcmp(argv[i], "--disc") == 0 && i + 1 < argc)
            disc = argv[++i];
        else if (std::strcmp(argv[i], "--extract-to") == 0 && i + 1 < argc)
            extracted = argv[++i];
        else if (std::strcmp(argv[i], "--extract-only") == 0)
            extract_only = true;
        else if (std::strcmp(argv[i], "--saves") == 0 && i + 1 < argc)
            saves = argv[++i];
        else if (std::strcmp(argv[i], "--skip-intro") == 0)
            skip_intro = true;
#if defined(_WIN32)
        // A disc image dropped onto the executable.
        else if (!dropped_disc && argv[i][0] != '-' && is_disc_image(std::filesystem::path(reinterpret_cast<const char8_t *>(argv[i]))))
            dropped_disc = argv[i];
#endif
        else {
            std::fprintf(stderr, "Usage: kings-field [--data DIRECTORY | --disc IMAGE [--extract-to NEW_DIRECTORY] [--extract-only]] [--saves DIRECTORY] [--skip-intro]\n");
            return 1;
        }
    }
    if ((disc && data_selected) || (!disc && (extracted || extract_only))) {
        std::fprintf(stderr, "Use --data for existing files, or --disc with optional --extract-to/--extract-only.\n");
        return 1;
    }
#if defined(_WIN32)
    std::string release_data;
    if (!disc && !data_selected) {
        if (!prepare_release_data(dropped_disc, release_data))
            return 1;
        data = release_data.c_str();
    }
#endif
    if (disc) {
        data = extracted ? extracted : "data";
        if (!kf::disc_extract(disc, data))
            return 1;
        if (extract_only)
            return 0;
    }
    if (!kf::data_files_set_root(data))
        return 1;
    if (english_patch_present()) {
        kf::game::menu_glyphs::menu_glyphs_use_english();
        std::printf("English patch detected: using translated menu labels\n");
    }
    // Create browser audio within the launch gesture, before asynchronous storage.
    if (!kf::host_start())
        return 1;
    if (!kf::save_storage_start(saves))
        kf::host_fail("Cannot initialize save storage. Check browser storage permissions or the selected save directory.");
#ifdef __EMSCRIPTEN__
    browser_game_started();
#endif
    kf::AppMode mode = skip_intro ? kf::AppMode::Gameplay : kf::AppMode::Opening;
    for (;;) {
        switch (mode) {
        case kf::AppMode::Gameplay:
            kf::host_set_input_context(kf::InputContext::Gameplay);
            mode = kf_run_game();
            break;
        case kf::AppMode::Opening:
        case kf::AppMode::Ending:
            kf::host_set_input_context(kf::InputContext::Opening);
            kf_run_opening(mode);
            mode = kf::AppMode::Gameplay;
            break;
        }
    }
}
