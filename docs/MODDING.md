# Modding (Windows PC extensions)

Mods are Lua 5.4 scripts that the game loads, and reloads, while it runs. The
workflow follows [Verdite2](https://github.com/Voicedrew11/verdite2): edit a
file, save it, and the running game picks it up.

## Where mods live

`%APPDATA%\KingsField\SLPS00017\mods\<folder>\` (F1 → Mods → **Open mods folder**).

```
mods/stathud/mod.json
mods/stathud/main.lua
```

`mod.json`:

```json
{
  "id": "stathud",
  "name": "Stat HUD",
  "version": "1.0",
  "author": "you",
  "description": "Shown in the Mods panel.",
  "entry": "main.lua"
}
```

Only `main.lua` is required; `id` defaults to the folder name and `entry` to
`main.lua`. `require("other")` loads `other.lua` from the same folder.

## Live reload

* The game scans the mods folder twice a second. A new folder appears in the
  Mods panel immediately; mods start **disabled**, like Verdite2.
* Saving any `.lua` file or `mod.json` in an enabled mod reloads it: `on_unload`
  runs, the Lua state is discarded, and the mod starts fresh.
* An error stops only that mod. The message and stack trace go to the Console
  (F1 → Console) and appear under the mod in the Mods panel. Fix the file and
  save; it reloads on its own.
* The enabled flag and `config` are stored in `settings.json` next to the mods folder.

## Callbacks

Define any of these as globals:

| function | when |
|---|---|
| `on_load()` | after the script runs, on every (re)load |
| `on_frame()` | once per frame the game presents (gameplay, menus, cutscenes) |
| `on_draw()` | every presented frame, for drawing over the game with `ui.draw_*` or a `ui.begin_window` |
| `on_settings()` | while the mod's **Settings** node is open in the Mods panel |
| `on_unload()` | before a reload or when the mod is disabled |

The game is paused while the F1 overlay is open (Settings → Interface), so
`on_frame` does not run then; `on_draw` still does.

## `config`

A table that persists per mod. Values may be numbers, booleans, strings or
nested tables with string keys. It is saved when a `ui.*` widget changes inside
`on_settings`, when the mod unloads, or when you call `kf.save_config()`.

```lua
config.speed = config.speed or 1.5          -- default on first run
function on_settings()
  config.speed = ui.slider_float("Speed", config.speed, 0.5, 3)
end
```

## `kf`

| call | returns |
|---|---|
| `kf.log(...)` | prints `[id] ...` to the console and `kf.log` |
| `kf.context()` | `"opening"`, `"gameplay"`, `"menu"` or `"scripted"` |
| `kf.frame()` | presented-frame counter |
| `kf.time()` | seconds since start |
| `kf.key_down(name)` / `kf.key_pressed(name)` | SDL key names: `"F5"`, `"G"`, `"Left Shift"`; `pressed` is true on the first frame only. Both are false while the overlay is open. |
| `kf.view()` | `x, y, w, h` of the whole game picture in window pixels (wider than 4:3 in widescreen) |
| `kf.view_original()` | `x, y, w, h` of the original 320×240 area, centred in the picture |
| `kf.player.<field>` | read/write player state (below) |
| `kf.player_fields()` | list of field names |
| `kf.save_config()` | writes `config` now |
| `kf.api_version` | `"0.1"` |

`kf.player` fields: `hp max_hp mp max_mp gold experience next_level_experience
level physical_power magic base_physical_power base_magic
physical_power_training magic_training magic_charge status_flags
cutting_attack striking_attack piercing_attack holy_attack fire_attack
cutting_defense striking_defense piercing_defense poison_resistance
magic_defense fire_defense curse_timer darkness_timer poison_timer
slowed_timer`, plus read-only `floor`. Writes go straight into the game's
`player_state`; the game may recompute derived stats later.

## `ui` (Dear ImGui)

Only inside `on_draw` and `on_settings`. Colours are 0–1 floats. Sliders show a
number box for typing a value. Mod drawing is not affected by the overlay's
interface scale.

| call | notes |
|---|---|
| `ui.text(s)`, `ui.text_colored(r, g, b, s)` | |
| `ui.button(label)` → bool | true when clicked |
| `ui.checkbox(label, value)` → value | |
| `ui.slider_int(label, v, min, max)` → v | |
| `ui.slider_float(label, v, min, max [, fmt])` → v | |
| `ui.combo(label, index, {items})` → index | 1-based |
| `ui.separator()`, `ui.same_line()`, `ui.spacing()`, `ui.tooltip(s)` | |
| `ui.begin_window(title [, click_through])` → visible; `ui.end_window()` | always call `end_window` |
| `ui.draw_text(x, y, s [, r, g, b, a [, size]])` | window pixels, drawn over everything; `size` in pixels (default 18) |
| `ui.draw_rect(x, y, w, h [, r, g, b, a [, filled]])` | |
| `ui.game_to_screen(x, y)` → x, y | converts PS1 320×240 coordinates (the original area, where the game's HUD and menus are) |

## Examples

`mods/stathud` (HUD drawing + settings) and `mods/quickheal` (hotkey + writing
player state) in this repository. Copy them into the mods folder to try them.
