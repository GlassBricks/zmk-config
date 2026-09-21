#!/usr/bin/env bash
# Regenerates compile_commands.json for clangd from the West deps `nix build`
# already pinned, without copying the 2.5G workspace out of the store.
set -euo pipefail

root=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)
ws=$root/build/ws
deps=$(nix build --no-link --print-out-paths "$root#left.westDeps")

mkdir -p "$ws/.west"
printf '[manifest]\npath = config\nfile = west.yml\n' >"$ws/.west/config"
ln -sfn "$root/config" "$ws/config"
for d in "$deps"/*; do ln -sfn "$d" "$ws/$(basename "$d")"; done

# West shells out to git, and the pinned checkouts are owned by the store.
export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*'

# ZMK caches ZMK_CONFIG and silently ignores a later -DZMK_CONFIG, leaving
# KEYMAP_FILE pinned to the old path, so a changed value needs a pristine build
# dir. West's `-p auto` only notices the app source dir moving, not this.
cached_config=$(sed -n 's/^CACHED_ZMK_CONFIG:STRING=//p' "$ws/build/CMakeCache.txt" 2>/dev/null || true)
if [[ -n $cached_config && $cached_config != "$root/config" ]]; then
  rm -rf "$ws/build"
fi

# zmk/app and zephyr must be store paths, not the symlinks above: Zephyr derives
# library target names from paths relative to the source root, and realpaths
# that escape through a symlink mangle them into unbuildable targets.
cd "$ws"
nix develop "$root" -c west build -p auto -d build -s "$deps/zmk/app" -b 'halcyon_wireless//zmk' -- \
  -DSHIELD='halcyon_kyria_left mod_battery_lipo mod_display_epaper_forest' \
  -DZMK_CONFIG="$root/config" \
  -DZMK_EXTRA_MODULES="$root" \
  -DCMAKE_PREFIX_PATH="$deps/zephyr" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DBUILD_VERSION="$(cat "$deps/zephyr/.git/HEAD")"

ln -sfn build/ws/build/compile_commands.json "$root/compile_commands.json"
