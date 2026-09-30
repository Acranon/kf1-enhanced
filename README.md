# King's Field Enhanced

A Windows PC version of the original **King's Field** (Japan, 1994, SLPS-00017),
playable in English with the fan translation or in the original Japanese
(switchable in game), and with optional modern
improvements: widescreen, high resolutions, high frame rates, mouse look,
dynamic lighting, an automap, quick saves, an in-game settings overlay and Lua
mods.

It is built on [sushi-shi's source port](https://github.com/sushi-shi/kings-field-decomp)
of the game, which runs the original game logic natively. The settings menus,
the live-reloading mod system and the console are inspired by
[Verdite2](https://github.com/Voicedrew11/verdite2), Voicedrew11's excellent PC
port of the US King's Field. Nothing from the game is included here: you need
your own copy of the disc.

> **Pre-release.** The first floor has been played through for several hours;
> later floors, the ending and some edge cases are less tested. Please report
> problems in the issue tracker, with `kf.log` attached (see *Files* below).

## What you need

- Windows 10 or 11 (64-bit) and a GPU with OpenGL support.
- A disc image of **King's Field (Japan) SLPS-00017** that you dumped yourself,
  either of:
  - **The original Japanese disc.** On the first launch the game offers to add
    the English translation by downloading the translator's own patch (see
    *Languages*), so you can play in both.
  - **A disc with the [English translation patch](https://www.romhacking.net/translations/7308/)
    already applied** (v1.0, with or without the save-bug fix).
  - Accepted formats: `.cue` + `.bin` (single track, MODE2/2352) or `.iso`.

## Playing

1. Download the latest zip from [Releases](../../releases) and unzip it anywhere.
2. Run `kings-field.exe`. On the first launch it asks for your disc image and
   copies the game files into a `data` folder beside the exe. This takes a few
   seconds and happens only once; the image itself is not changed.
   - Instead of picking the image, you can drop your `.cue` (or `.iso`) onto
     `kings-field.exe`, or put it in the same folder before the first launch.
   - With the Japanese disc, it then offers to add English (see below).
3. Press **F1** at any time for settings. The game keeps running while the
   overlay is open.

To use a different disc image later, delete the `data` folder and launch again.
Versions before 0.1.0-pre.3 kept the files directly in `data`; they are moved
into `data\en` (or `data\ja`) automatically.

### Languages

The game can run in English (John Osborne's fan translation v1.0) or the
original Japanese, and switch between them at any time: in the game's own
**Options** menu (new *Language* row) or in **F1 > Settings > Language**. Menus
and item names change at once; dialogue and signs follow as they are next
shown. Saves work in both.

Each language needs its own game files, kept in `data\en` and `data\ja`:

- **English from a Japanese disc:** nothing translated ships with this project.
  The game downloads the translator's original release
  (`Kings_Field_Jap_to_Eng_v1.0.rar`, about 6 MB) from the
  [Internet Archive's copy](https://web.archive.org/web/2021/https://www.angelfire.com/art3/weissvulf/)
  of his site, checks it, applies it to your Japanese files and verifies the
  result against the known English files. If you already have the `.rar` (or
  the `.ppf` inside it), choose it instead, or drop it onto `kings-field.exe`.
- **Japanese when you only have a patched disc:** add your original Japanese
  disc image in F1 > Settings > Language (or drop it onto `kings-field.exe`).
  A patched disc can't be turned back into Japanese.

### Controls

| Action | Keyboard / mouse |
| --- | --- |
| Move / strafe | W A S D |
| Look | Mouse (click the window to capture it), Page Up / Page Down |
| Turn | Left / Right arrows |
| Run | Shift (hold, or toggle in Settings > Controls) |
| Attack | Space, left mouse button |
| Magic | Q, right mouse button |
| Interact / confirm | E, Enter |
| Menu | Tab (Tab or Backspace backs out) |
| Pause | P, Escape |
| Full map | M |
| Quick save / quick load | F2 / F4 (F4 also works on the title screen) |
| Settings overlay | F1 |
| Fullscreen | F11, Alt+Enter |

Controllers are supported (left stick moves, click it to run).

## Features

Everything below can be switched off in the F1 settings to get closer to the
original PlayStation presentation.

**Display**
- Widescreen (16:9, 16:10, 21:9 or window-shaped) and native-resolution
  rendering (for example 2560x1440), or whole multiples of 320x240.
- Frame smoothing up to your monitor's refresh rate (game speed unchanged).
- Stable polygons and perspective-correct textures ("Polygon wiggle" restores
  the PS1 look), smooth colour without 15-bit banding, optional texture
  filtering, anti-aliasing (FXAA or 4x/9x supersampling), optional depth buffer.

**Lighting**
- Torches and fire or lightning spells light nearby surfaces, with flicker;
  walls block the light. Flame glow, torch shadows and ambient occlusion.
  Shadows only remove added light, so nothing is darker than in the original.

**Gameplay and quality of life**
- Mouse look with a wider up/down range (adjustable, original is about 17 degrees).
- Run key with adjustable speed.
- Equipment stats beside the item picture in menus and shops, and on the equip
  screens the current totals next to the totals with the highlighted item
  (green if better, red if worse).
- Quick save and load (F2/F4) using the game's own save format, kept separate
  from the three normal save slots.
- Automap: a north-up minimap and a full floor map (M), which pauses the game
  while open (or not, if you prefer to walk with it open). Fog of war shows only
  what you have seen (or turn it off); carrying the Keeper's or Magician's Map
  reveals the whole floor. Doors, warps (labelled with their floor), save points,
  fountains and people are marked once seen. Items, chests and enemies can be
  shown too, as can the key each locked door or chest needs; those are off by
  default because they are cheats. On the full map, scroll to zoom, drag to move
  and point at a marker to see what it is (a chest lists its contents).
- English or Japanese, switchable during play (see *Languages*).
- Optional fixes for bugs in the original game (Settings > Gameplay, on by
  default): enemies that bump a ceiling mid-jump fall back down, and a stereo
  panning adjustment the original never applied.

**Overlay and mods**
- F1 overlay with settings, a mod manager and a console showing the game's log.
- Lua mods with live reload: edit a mod while the game runs and it reloads
  instantly. Two example mods (a stat panel and a quick-heal cheat) are included,
  switched off; enable them in F1 > Mods. See [docs/MODDING.md](docs/MODDING.md).

## Files

Everything the game writes goes to `%APPDATA%\KingsField\SLPS00017`:

| File | Contents |
| --- | --- |
| `slot1.kfs` ... `slot3.kfs` | The game's save slots |
| `quicksave.kfs` (+ `.bak`) | Quick save and the one before it |
| `*.kfs.map` | Explored map areas for each save |
| `settings.json` | Settings, including each mod's options |
| `mods\` | Installed mods |
| `kf.log` | Log of the last session (attach it to bug reports) |

## Building from source

Windows build:

- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) (the
  `ucrt-x86_64` package), with its `bin` folder on `PATH`.
- [Rust](https://rustup.rs) with the `x86_64-pc-windows-gnullvm` host toolchain
  (`rustup toolchain install stable-x86_64-pc-windows-gnullvm` and make it the
  default), used for the original codecs.
- CMake 3.25 or newer and Ninja.

```sh
cmake --preset windows
cmake --build --preset windows
```

SDL3 is downloaded automatically on the first configure. The result is
`build/windows/kings-field.exe` with `SDL3.dll` beside it. Run it with no
arguments for the first-launch setup above, or pass `--data DIRECTORY` for an
already extracted disc (`--data-ja` and `--data-en` give one of each language;
`--language ja|en` picks the starting one; `--english-patch FILE|download`
builds English into `data\en` beside the exe). `scripts/package-windows.ps1`
builds the release zip.

Linux and browser builds are unchanged from upstream; see
[docs/UPSTREAM-README.md](docs/UPSTREAM-README.md).

## Credits

- **[sushi-shi/kings-field-decomp](https://github.com/sushi-shi/kings-field-decomp)**:
  the decompilation and source port this project is built on (CC0). All of the
  game logic, the original renderer and audio code, the disc extractor and the
  save system come from it, as do the approach to building English from the
  translator's patch (with his table of the disc's file layout), the language
  switch and several bug fixes.
- **[Verdite2](https://github.com/Voicedrew11/verdite2)** by Voicedrew11 (MIT):
  the PC port of the US King's Field. Its in-game menus, live-reloading Lua mod
  system and console window inspired the F1 overlay, the mod system and the
  console here, and its map inspired the automap. The run key and quick save
  follow the [Verdite2 mods](https://github.com/Acranon/verdite2-mods) of the
  same names.
- **John Osborne (weissvulf / HwitVlf)**: the
  [English translation](https://www.romhacking.net/translations/7308/) of
  King's Field (v1.0, 2006) and its save-bug fix. This project reads the
  translated text from your patched disc, or applies his original patch to your
  Japanese disc; the translation itself is not redistributed.
- **[Dear ImGui](https://github.com/ocornut/imgui)** (MIT), **[Lua](https://www.lua.org)**
  (MIT), **[cJSON](https://github.com/DaveGamble/cJSON)** (MIT),
  **[SDL3](https://github.com/libsdl-org/SDL)** (zlib) and the
  **[Khronos OpenGL ES headers](https://github.com/KhronosGroup/OpenGL-Registry)** (MIT).
- **FromSoftware**, who made King's Field. This is an unofficial fan project
  and is not affiliated with or endorsed by FromSoftware or Sony.

## License

The code in this repository is released under [CC0](LICENSE), like the port it
is based on. Third-party code in `vendor/` keeps its own license; see the
license file in each folder. King's Field and its assets belong to
FromSoftware and are not included.
