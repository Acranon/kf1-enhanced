# Port technical notes

Non-obvious asset and behavior constraints. Addresses below identify the original
retail GAME.EXE or OPEN.EXE; deliberate port corrections are not matching claims.
Track remaining work in [issues](https://github.com/sushi-shi/kings-field-decomp/issues).

## Ownership and update order

Gameplay and cutscenes own separate arenas and state. Re-entry resets initialized
globals as well as BSS; borrowed views must not survive transitions.

- Movement updates Z before testing X. Pools visit live slots in ascending order,
  so a later slot activated during an update can run that tick.
- Armor regeneration precedes drain in head/body/shield/arm/leg order. Combining
  HP deltas changes clamping and death handling.
- Gameplay camera paths fetch then increment; cutscene paths increment then fetch.
  `matrix_interpolate` changes rotation only, preserving translation.

## Resource boundaries

Two COM table copies intentionally cross nominal chunk boundaries:

| Table | Payload offset | Nominal bytes | Copied bytes |
| --- | ---: | ---: | ---: |
| Armor | `0x1134` | 756 | 1,176 |
| Map-object definitions | `0x1610` | 1,128 | 1,280 |

Armor includes the next four-byte header and 416 magic bytes; object definitions
include the next header and 148 growth bytes. `resource_stream_tail` checks the
file extent while retaining the normal next-chunk cursor. Restricting these copies
to the nominal chunk breaks startup.

`KfTmdResource` borrows the loaded pointer and extent; standalone menu TMDs have
no preceding MIX header. Embedded TMD extent is the containing asset's tail, not
an independently proved subregion. Registration checks the 12-byte header and
28-byte object records. Gameplay enqueuers retain distinct 12/4/2 accepted-case
sets and advance by `input_length * 4` even for skipped modes. Cutscene enqueuers
use the same bounded decoder with their own accepted modes.

The codecs decode animation clips, keyframes and morphs when an asset is registered.
The game owns the decoded arrays; animation caches retain morph indices rather
than resource pointers and are invalidated when their asset is replaced.
The shared byte reader in `codecs/bytes.h` checks bounds and reads explicit
endianness without aligned source casts. Gameplay and opening placements
use the shared decoders; map grids are copied from bounded bytes without aligned
source casts. Placement decoders return counted records and validate their grid
and definition indices. Actor definitions retain their resource layout with a checked
table extent. Cell-window dimensions and map orientations still need load-time
validation; further resource work is tracked in
[issue #40](https://github.com/sushi-shi/kings-field-decomp/issues/40).

The floor-item appearance byte packs facing in the high nibble and frame count
in the low nibble. B1 MIXA base-sprite 4 records use `0x23`/`0x13`; base-sprite 0
records use `0x04`. Using the full byte for random frame selection can overflow
the seven-entry sprite table. The port separates facing/count, preserves random
call count and scaling, and accepts zero-frame records. Attribution of the earlier
unmasked expression needs separate GAME/OPEN evidence. The port's seed-one startup
policy is not a proved retail boot seed.

## Collision results

World queries return an explicit collision kind: terrain, below floor, ceiling,
missing attribute, player, actor, map object, map event, rejected cell flags, or
no hit. The detail carries a cell kind, pool index or rejected flags where needed.
Effect queries also distinguish obstruction when no collision targets are selected.

Queries check terrain, rejected cell flags, then player, actors, objects and
events, subject to skip flags and occupancy. Distance/index probes use signed
`-1` and have a separate domain.

Door-closing probes ignore terrain and map objects, checking a 3,000-unit radius at
the lift-door origin or the cardinally offset hinged-door position.

## Rendering

| Retail GAME.EXE evidence | Retained behavior |
| --- | --- |
| `tmd_project_vertices`, `0x8001c60c` | Full current `ReadSZ2` depth; projection flags do not reject vertices. |
| `render_enqueue_map`, `0x8001de18` | Projected winding, signed depth arithmetic, wall bias 200. |
| `render_enqueue_tmd`, `0x8001c7f8` | Averaged depth plus signed bias reaches five before 14-bit masking. |
| `render_map_object`, `0x8001ebb8` | Hinged-door bias 15; lift-door bias 180. |
| `AddPrim`, `0x80054290` | Equal-depth head insertion. |

Sort whole faces before the original quad split. Reject triangles spanning more
than 1023 in X or 511 in Y before viewport clipping; this rule prevents
behind-camera door intrusion. The starting-room plaque edge and reported flame
behavior also occur in retail.

Projection uses integer reciprocal refinement, Q16 scale capped at `0x1ffff`,
signed coordinate rounding/saturation and signed fog arithmetic. `SetFogNear`
(GAME `0x8004f4b4`, OPEN `0x8002f288`) uses wrapped `-320 * near`, signed division
by reference distance 200, a signed 16-bit coefficient and intercept `0x01400000`.
Weapon projection retains that fog reference. Lighting preserves Q12 stages,
intermediate saturation and byte-color quantization.

Shaders retain raw/modulated and flat/Gouraud properties, RGB5 writeback,
dithering and integer blending. Zero texture words are transparent; STP black
is visible, and blended textured faces blend only STP texels. Each blended
triangle reads a separate clipped framebuffer snapshot. Modal calls accumulate
on one target; resize/expose re-presents stored pixels. Remaining modulation,
coverage, interpolation, presentation-chronology and performance questions are in
[the rendering issue](https://github.com/sushi-shi/kings-field-decomp/issues/38).

The fixed sine samples in `src/lib/fixed_math.cpp` derive from Psy-Q Release 2.5
`LIBGTE.LIB` / `GEO.OBJ`, also identified in GAME and OPEN. The 2,048-byte payload
SHA-256 is `74743e361fc4d78cbd41bd99b2acf5c75c9b5b0268ccd753ed282ec4394fb25a`.
Do not regenerate them with host trigonometry or combine the XYZ constructor
with the game's separate Y-X-Z helper.

## Timing

Gameplay world rendering owns the three-tick deadline, including scripted
redraws; callers must not add another full interval. Cutscene scene0 uses a local
22-update/s policy: 490 retail emulator intervals took 22.313425276 seconds
(about 21.96 updates/s). Other cutscene loops, menu waits, explicit holds and
sample-based audio keep their own timing. Focus pauses are excluded; long stalls
rebase deadlines without catch-up steps.

## Audio and menu quirks

GAME `0x80032f8c` masks the tone with `0x0f`;
OPEN passes the selector unchanged and uses distance gain on its near/equal-pan
path.

MP labels reuse HP's second glyph; total defense reuses total attack's prefix.
Whole-row replacements can change these labels. Effect update IDs passed to
`player_apply_damage` are damage multipliers in tenths, not actor-owner identities.

## Deliberate behavior corrections

- **Spatial sound panning:** GAME `audio_play_spatial` masks SoundRef byte one
  with `0x80` then compares to `1` (`0x80032e64..0x80032e78`), so the flagged
  pan-narrowing adjustment cannot run. Floor-five actor definitions 5–7 set this
  flag in all three sound references. The port tests the flag for nonzero and
  applies the existing 36-point panning gain adjustment after distance attenuation.
- **Jump landing:** GAME compares the collision result's shifted kind with the
  ceiling detail `0xfff1` at `0x80030168..0x80030184`, so its fall-recovery branch
  cannot run. The port checks `Ceiling` directly and resumes gravity.
- **Return home:** GAME action 33 in `actor_update_current_action` (`0x8002fa88`)
  uses uninitialized home registers at `0x80030500`; its proven caller supplies
  residue `(1, 65535)`. The port computes actual placement coordinates.
- **Player-homing pitch:** GAME effect kind 20 in `effect_update_dispatch`
  (`0x80038a38`) reads an unwritten stack distance at `0x80039760` on the player
  path. The port computes horizontal player distance with the existing fixed-point
  length operation, retaining pitch sign, offset, angle mask and turn limit.

These are intentional port policies. The affected combat paths still need direct
runtime verification.

Codec errors retain parser source locations through `std::source_location`.
