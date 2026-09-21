ZMK firmware config for a SplitKB Halcyon Kyria (wireless).

Built with Nix via [`zmk-nix`](https://github.com/lilyinstarlight/zmk-nix) instead of the usual West/GitHub-Actions flow.

## Commands

```sh
nix build                 # both halves -> result/{zmk_left, zmk_right}.uf2
nix build .#left          # one half only
nix build .#right
nix run .#flash           # flash whichever half is plugged in (uses `parts` from #firmware)
nix run .#update          # zmk-nix helper for refreshing pinned West dependencies
nix develop               # zmk-nix dev shell (west, Zephyr SDK) for manual west builds
scripts/gen-compile-commands.sh   # refresh compile_commands.json for clangd
```

No tests.

## Architecture

### This repo is a Zephyr module

Root: `zephyr/module.yml` + `Kconfig` + `CMakeLists.txt` make the repo a Zephyr module.

- `src/` is compiled into the firmware. `src/Kconfig` gates everything behind
  `CONFIG_HLC_STATUS_SCREEN`. `src/CMakeLists.txt` adds sources when it is set.

### Asymmetric halves

Each has own shield list:

- left (central, has the e-paper module): `halcyon_kyria_left mod_battery_lipo mod_display_epaper_forest`
- right (no module): `halcyon_kyria_right mod_battery_lipo`

Config layering:

- `config/halcyon_kyria.conf` - both halves
- `config/halcyon_kyria_left.conf` - left only; turns off the stock status widget and turns on ours.
- `config/halcyon_kyria_right.overlay` - right only; enables `&right_soldered_encoder`.

### Files

- Custom status screen: `src/display/`
- Keymap: `config/halcyon_kyria.keymap`
