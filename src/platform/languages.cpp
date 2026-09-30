#include <kf/platform/languages.hpp>
#include <kf/platform/disc.hpp>
#include <kf/platform/files.hpp>
#include <kf/platform/translation.hpp>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>

namespace kf {
namespace {
std::mutex lock;
std::string library;
std::array<std::string, 2> folders;
Language current = Language::English;
Language requested = Language::English;

LanguageTask task_state = LanguageTask::Idle;
std::string task_message;

std::size_t slot(Language language) { return language == Language::English ? 1 : 0; }

std::filesystem::path utf8_path(const std::string &path) {
    return std::filesystem::path(reinterpret_cast<const char8_t *>(path.c_str()));
}

std::string utf8(const std::filesystem::path &path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

// The English patch changes GAME.EXE's menu code; this word is its "Use" label.
bool english_game_exe(const u8 *exe, std::size_t size) {
    constexpr std::size_t exe_header_bytes = 0x800;
    constexpr std::uint32_t load_address = 0x80012000;
    constexpr std::uint32_t use_label_address = 0x800283fc;
    constexpr std::uint32_t english_use_label = 0x34020000;   // ori $v0, $zero, 0
    constexpr std::size_t offset = exe_header_bytes + (use_label_address - load_address);
    if (size < offset + 4)
        return false;
    const std::uint32_t word = exe[offset] | exe[offset + 1] << 8 | exe[offset + 2] << 16 |
        static_cast<std::uint32_t>(exe[offset + 3]) << 24;
    return word == english_use_label;
}

bool has_game_files(const std::filesystem::path &directory) {
    std::error_code error;
    return std::filesystem::is_regular_file(directory / "GAME.EXE", error);
}

Language folder_language(const std::filesystem::path &directory) {
    std::ifstream stream(directory / "GAME.EXE", std::ios::binary);
    std::vector<u8> exe(512 * 1024);
    stream.read(reinterpret_cast<char *>(exe.data()), static_cast<std::streamsize>(exe.size()));
    return english_game_exe(exe.data(), static_cast<std::size_t>(stream.gcount())) ? Language::English : Language::Japanese;
}

std::string library_folder(Language language) {
    return library.empty() ? std::string() : utf8(utf8_path(library) / language_code(language));
}

void run_task(std::function<bool(std::string *)> work) {
    {
        std::lock_guard guard(lock);
        if (task_state == LanguageTask::Working)
            return;
        task_state = LanguageTask::Working;
        task_message = "Working...";
    }
    // Detached: quitting mid-task is safe, as new folders only appear complete.
    std::thread([work = std::move(work)] {
        std::string message;
        const bool ok = work(&message);
        std::lock_guard guard(lock);
        task_state = ok ? LanguageTask::Done : LanguageTask::Failed;
        task_message = message;
    }).detach();
}

void task_progress(const char *message) {
    std::lock_guard guard(lock);
    task_message = message;
}
}

const char *language_name(Language language) {
    return language == Language::English ? "English" : "Japanese";
}
const char *language_code(Language language) {
    return language == Language::English ? "en" : "ja";
}
bool language_parse(const char *code, Language *language) {
    if (std::strcmp(code, "en") == 0)
        *language = Language::English;
    else if (std::strcmp(code, "ja") == 0)
        *language = Language::Japanese;
    else
        return false;
    return true;
}

Language game_language() { return current; }

void languages_configure(const std::string &directory) {
    std::lock_guard guard(lock);
    library = directory;
    for (const auto language : {Language::Japanese, Language::English}) {
        const auto folder = library_folder(language);
        if (!folder.empty() && folders[slot(language)].empty() && has_game_files(utf8_path(folder)))
            folders[slot(language)] = folder;
    }
}

Language language_of_folder(const std::string &directory) {
    return folder_language(utf8_path(directory));
}

Language languages_add_folder(const std::string &directory) {
    const auto language = folder_language(utf8_path(directory));
    std::lock_guard guard(lock);
    folders[slot(language)] = directory;
    return language;
}

std::string languages_start(Language wanted) {
    std::lock_guard guard(lock);
    if (folders[slot(wanted)].empty())
        wanted = wanted == Language::English ? Language::Japanese : Language::English;
    current = requested = wanted;
    return folders[slot(wanted)];
}

bool language_available(Language language) {
    std::lock_guard guard(lock);
    return !folders[slot(language)].empty();
}

std::string language_folder(Language language) {
    std::lock_guard guard(lock);
    return folders[slot(language)];
}

void language_request(Language language) {
    std::lock_guard guard(lock);
    requested = language;
}

Language language_requested() {
    std::lock_guard guard(lock);
    return requested;
}

bool language_apply_pending() {
    std::string folder;
    Language target;
    {
        std::lock_guard guard(lock);
        if (requested == current)
            return false;
        target = requested;
        folder = folders[slot(target)];
        if (folder.empty()) {
            requested = current;
            return false;
        }
    }
    if (!data_files_set_root(folder.c_str())) {
        std::fprintf(stderr, "Cannot switch to the %s game files in %s\n", language_name(target), folder.c_str());
        const auto previous = language_folder(current);
        data_files_set_root(previous.c_str());
        std::lock_guard guard(lock);
        requested = current;
        return false;
    }
    std::lock_guard guard(lock);
    current = target;
    std::printf("Language: %s (%s)\n", language_name(target), folder.c_str());
    return true;
}

bool languages_add_disc(const char *image, Language *added, std::string *message) {
    AssetTable assets{};
    DiscMatch match;
    if (!disc_import_image(image, &assets, &match)) {
        *message = "That file could not be read as King's Field (Japan, SLPS-00017).";
        return false;
    }
    Language language;
    if (match == DiscMatch::Japanese)
        language = Language::Japanese;
    else if (match == DiscMatch::English)
        language = Language::English;
    else {
        // Other modified discs are used as they are, as the language their menus use.
        const auto *exe = assets_find(&assets, "GAME.EXE");
        language = exe && english_game_exe(exe->bytes.data, exe->bytes.size) ? Language::English : Language::Japanese;
    }
    const auto folder = library_folder(language);
    bool ok = false;
    if (language_available(language))
        *message = std::string("The ") + language_name(language) + " game files are already set up.";
    else if (folder.empty())
        *message = "There is no folder to copy the game files to.";
    else if (!resource_tree_write(assets, folder.c_str()))
        *message = "The game files could not be written to " + folder + ".";
    else {
        ok = true;
        *added = language;
        *message = std::string(language_name(language)) + " is ready.";
        std::lock_guard guard(lock);
        folders[slot(language)] = folder;
    }
    assets_release(&assets);
    return ok;
}

bool languages_make_english(const std::vector<u8> &ppf, std::string *message) {
    const auto japanese = language_folder(Language::Japanese);
    const auto folder = library_folder(Language::English);
    if (japanese.empty()) {
        *message = "The English patch needs the Japanese game files. Add your Japanese disc first.";
        return false;
    }
    if (language_available(Language::English)) {
        *message = "English is already set up.";
        return false;
    }
    if (folder.empty()) {
        *message = "There is no folder to write the English game files to.";
        return false;
    }
    AssetTable assets{};
    bool ok = resource_tree_read(japanese.c_str(), &assets);
    if (!ok)
        *message = "The Japanese game files in " + japanese + " are incomplete.";
    ok = ok && translation_apply_ppf(&assets, ppf, message);
    if (ok && !resource_tree_write(assets, folder.c_str())) {
        *message = "The English game files could not be written to " + folder + ".";
        ok = false;
    }
    assets_release(&assets);
    if (ok) {
        *message = "English is ready.";
        std::lock_guard guard(lock);
        folders[slot(Language::English)] = folder;
    }
    return ok;
}

void language_task_download_english() {
    run_task([](std::string *message) {
        task_progress("Downloading the English patch...");
        std::vector<u8> ppf;
        if (!translation_download(&ppf, message))
            return false;
        task_progress("Applying the English patch...");
        return languages_make_english(ppf, message);
    });
}

void language_task_patch_file(const std::string &path) {
    run_task([path](std::string *message) {
        task_progress("Reading the patch...");
        std::vector<u8> ppf;
        if (!translation_read_patch(path.c_str(), &ppf, message))
            return false;
        task_progress("Applying the English patch...");
        return languages_make_english(ppf, message);
    });
}

void language_task_add_disc(const std::string &path) {
    run_task([path](std::string *message) {
        task_progress("Copying the game files from the disc image...");
        Language added;
        return languages_add_disc(path.c_str(), &added, message);
    });
}

LanguageTask language_task_state(std::string *message) {
    std::lock_guard guard(lock);
    if (message)
        *message = task_message;
    return task_state;
}

void language_task_clear() {
    std::lock_guard guard(lock);
    if (task_state != LanguageTask::Working) {
        task_state = LanguageTask::Idle;
        task_message.clear();
    }
}
}
