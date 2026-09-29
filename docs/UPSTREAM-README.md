# King's Field — Linux / WebAssembly source port

A direct source port of the original Japanese King's Field (SLPS-00017).
The original game logic runs through portable rendering, audio and platform
interfaces. Supply your own disc image; game data is not bundled.

## Play on Linux

On x86_64 Linux with Nix flakes enabled:

```sh
KF_DISC="/path/to/King's Field (Japan).iso" nix run github:sushi-shi/kings-field-decomp/port
```

Supported images: 2048-byte-sector ISO, raw MODE2/2352 BIN, or a single-track
MODE2/2352 CUE with INDEX 01 at 00:00:00. Keep a CUE's referenced BIN beside it.
`KF_DISC` is read at runtime: no `--impure` or manual extraction is needed.
The launcher never uploads the disc or adds it to the Nix store.

The first launch verifies and extracts the resources; later launches reuse
`$XDG_CACHE_HOME/kings-field/SLPS-00017/resources-v1`. If `XDG_CACHE_HOME` is
unset or relative, it defaults to `$HOME/.cache`. Saves are stored separately
in the SDL user-preference directory, in three `.kfs` slots.

The game starts from the opening. Append options after `--`:

| Option | Purpose |
| --- | --- |
| `--skip-intro` | Start gameplay directly |
| `--saves DIRECTORY` | Use an existing save directory |
| `--data DIRECTORY` | Use an extracted disc tree instead of `KF_DISC` |

For example, to use existing extracted files:

```sh
nix run github:sushi-shi/kings-field-decomp/port -- --data /path/to/extracted/disc
```

The flake also exposes `packages.x86_64-linux.default` for installation or use
from another flake. The installed `kings-field` command uses the same `KF_DISC`
variable. In a local checkout of `port`, use `nix run .`.

## Controls

| Action | Keyboard / mouse |
| --- | --- |
| Move / strafe | WASD |
| Move / turn | Arrow keys |
| Look | Mouse or Page Up / Page Down |
| Attack | Space or left mouse button |
| Magic | Q or right mouse button |
| Interact / confirm | E or Enter |
| Inventory / skip intro | Tab |
| Back | Backspace or Escape in menus |
| Pause | P or Escape during gameplay; fresh input resumes |

Controllers are also supported. Click the window to capture the mouse; this
initial click does not attack. Focus loss pauses the game and releases capture.
Linux logical-key remapping, including Caps-to-Escape, is respected. Browsers
may require another click to restore pointer lock.

## Build from source

Use the `port` branch and its pinned development shell:

```sh
nix develop
cmake --preset linux
cmake --build --preset linux
emcmake cmake --preset wasm
cmake --build --preset wasm
```

The native executable is `build/linux/kings-field`. Unlike the Nix launcher,
it expects explicit `--data DIRECTORY` or `--disc IMAGE --extract-to NEW_DIRECTORY`
arguments; extraction destinations must not already exist.

Code uses C++20 as C with classes: plain structures, functions and scoped enums,
without inheritance, RTTI or exceptions.

## Browser

After the WASM build, serve `build/wasm` over localhost or HTTPS and open
`kings-field.html`. Select an ISO or BIN (with its CUE if applicable), then
press Play. Extraction stays local; resources and saves use separate IndexedDB
stores. Browser storage can be cleared or evicted.

## Status

Linux inputs, combat, sound and general rendering have been user-checked.
Browser audible playback, full-browser-restart persistence/capture, and natural
ending/re-entry still need verification.

See [remaining work](port-status.md), [implementation notes](../PORTING.md)
and [detailed findings](port-findings.md).
