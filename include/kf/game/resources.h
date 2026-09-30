#ifndef KF_GAME_RESOURCES_H
#define KF_GAME_RESOURCES_H

#include <kf/lib/memory.h>

extern KfMemoryArena memory_arena;

extern void common_resources_load(void);
// Applies a language change requested from Configuration or the F1 overlay:
// switches the game files, the translated font texels and the menu text. Call
// only where no menu or dialogue holds text from the old language.
extern bool game_apply_language(void);
// Points the menu labels at the language in use.
extern void game_use_language_labels(void);

#endif
