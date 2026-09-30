#pragma once
#include <kf/lib/types.h>
#include <string>
#include <vector>

namespace kf {
// The game's two resource sets: the Japanese disc and John Osborne's English
// translation v1.0. Each is an extracted folder of game files; saves are shared.
enum class Language : u8 { Japanese, English };
const char *language_name(Language language);
const char *language_code(Language language);   // "ja" or "en" (settings.json)
bool language_parse(const char *code, Language *language);

// The language of the files in use; the game's menu labels follow it.
Language game_language();
// The player's choice (settings.json), used at start when both are available.
inline Language language_preference = Language::English;

// `library` is where new language folders are written ("<library>/ja" and
// "<library>/en"); existing ones there are picked up.
void languages_configure(const std::string &library);
// The language of an extracted folder, read from its GAME.EXE.
Language language_of_folder(const std::string &directory);
// A folder given explicitly (--data), as the language of its GAME.EXE.
Language languages_add_folder(const std::string &directory);
// Sets `language` as the one in use at start and returns its folder, or the
// other language when it is missing. Empty when no game files exist yet.
std::string languages_start(Language wanted);
bool language_available(Language language);
std::string language_folder(Language language);

// Switching during play: a request is kept until the game applies it where no
// menu or dialogue holds text from the old language (see game_apply_language).
void language_request(Language language);
Language language_requested();
// Points the game files at the requested language. False when nothing changed.
bool language_apply_pending();

// Adding a language. These block; the tasks below run them in the background.
// Imports a disc image (Japanese, or with the English patch applied).
bool languages_add_disc(const char *image, Language *added, std::string *message);
// Builds English from the Japanese files and the translator's patch.
bool languages_make_english(const std::vector<u8> &ppf, std::string *message);

enum class LanguageTask : u8 { Idle, Working, Done, Failed };
void language_task_download_english();
void language_task_patch_file(const std::string &path);
void language_task_add_disc(const std::string &path);
LanguageTask language_task_state(std::string *message);
void language_task_clear();
}
