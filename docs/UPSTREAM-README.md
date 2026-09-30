# King's Field — Linux / WebAssembly source port

A Linux and browser port of the original Japanese King's Field (SLPS-00017).
Supply your own disc image; game data is not bundled.

## Branches

```text
              master
                 |
     +-----------+-----------+
     |                       |
     v                       v
  source                  classic
     |
     v
   port (you are here)
```

| Branch | Purpose |
| --- | --- |
| `master` | Reconstruction and matching |
| `source` | C++ PS1 build, codecs, and base for porting |
| `classic` | C PS1 build |
| `port` | Linux and browser port |

## Play on Linux

On x86_64 Linux with Nix flakes enabled:

```sh
KF_DISC="/path/to/King's Field (Japan).iso" nix run github:sushi-shi/kings-field-decomp/port
```

Supply your own disc image of the Japanese King's Field (SLPS-00017).
Tested image: `King's Field (Japan).bin`, SHA-256:

```text
ae74beba377d686bfaa292ea40df8ade4454ec3139c2b5152364e02aac90b3d9
```

The first launch extracts and caches game data locally. Saves use three separate
slots.

The game starts in English. Switch languages during play in **Configuration → Language**.
English uses John Osborne's translation, prepared from your Japanese disc.
See [translation details](docs/english-resources.md).

With `nix run`, pass game options after `--`. For example:

```sh
KF_DISC=/path/to/disc.iso nix run . -- --saves /path/to/saves
```

| Option | Purpose |
| --- | --- |
| `--saves DIRECTORY` | Use an existing save directory |
| `--language ja\|en` | Choose the starting language |

In a local checkout, use `KF_DISC=/path/to/disc.iso nix run .`.

## Install with a NixOS flake

For an x86_64 Linux system, add the game and a local directory containing your
disc to your flake inputs:

```nix
inputs.kings-field.url = "github:sushi-shi/kings-field-decomp/port";
inputs.kings-field-disc = {
  url = "path:/path/to/disc-directory";
  flake = false;
};
```

Import the module and set your disc's filename:

```nix
outputs = { nixpkgs, kings-field, kings-field-disc, ... }: {
  nixosConfigurations."<host>" = nixpkgs.lib.nixosSystem {
    modules = [
      ./configuration.nix
      kings-field.nixosModules.default
      {
        programs.kings-field = {
          enable = true;
          disc = "${kings-field-disc}/King's Field (Japan).iso";
        };
      }
    ];
  };
};
```

Nix verifies and extracts the disc into its store during installation. Rebuild
your configuration, replacing `<host>` with your host's name, then launch:

```sh
sudo nixos-rebuild switch --flake '.#<host>'
kings-field
```

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
| Pause | P or Escape during gameplay |

## Build from source

From the `port` branch:

```sh
nix develop
cmake --preset linux
cmake --build --preset linux
build/linux/kings-field --data /path/to/extracted/disc
```

To extract a disc with this executable, replace `--data` with
`--disc IMAGE --extract-to NEW_DIRECTORY`. The destination must not already exist.

## Browser

Inside `nix develop`:

```sh
emcmake cmake --preset wasm
cmake --build --preset wasm
python3 -m http.server --directory build/wasm
```

Open [the game](http://localhost:8000/kings-field.html), select your disc and press
Play. The language selector also works during play. Data and saves stay in
browser storage; clearing it removes them.
