{
  description = "ZMK firmware for a SplitKB Halcyon Kyria (wireless)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    zmk-nix = {
      url = "github:lilyinstarlight/zmk-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, zmk-nix }: let
    inherit (nixpkgs) lib;
    forAllSystems = lib.genAttrs (lib.attrNames zmk-nix.packages);
  in {
    packages = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      inherit (zmk-nix.legacyPackages.${system}) buildKeyboard;

      src = lib.sourceFilesBySuffices self [
        ".board" ".c" ".cmake" ".conf" ".defconfig" ".dts" ".dtsi" ".h"
        ".json" ".keymap" ".overlay" ".shield" ".yml" "_defconfig"
        "CMakeLists.txt" "Kconfig"
      ];

      common = {
        inherit src;

        board = "halcyon_wireless//zmk";

        zephyrDepsHash = "sha256-npvEASa46KYvO7SCwrBoDsLQSA7KOWAyd3ur5xGTa9g=";

        meta = {
          description = "ZMK firmware for a SplitKB Halcyon Kyria (wireless)";
          license = lib.licenses.mit;
          platforms = lib.platforms.all;
        };
      };

      # One West workspace fetch shared by both asymmetric halves.
      westDeps = (buildKeyboard (common // {
        name = "zmk";
        shield = "halcyon_kyria_left";
      })).westDeps;

      left = buildKeyboard (common // {
        name = "zmk-left";
        shield = "halcyon_kyria_left mod_battery_lipo mod_display_epaper_forest";
        inherit westDeps;
      });

      right = buildKeyboard (common // {
        name = "zmk-right";
        shield = "halcyon_kyria_right mod_battery_lipo";
        inherit westDeps;
      });
    in rec {
      default = firmware;

      inherit left right;

      # `parts` is what zmk-nix's flash script iterates over: without it the
      # flasher globs every *.uf2 onto whichever half is plugged in.
      firmware = pkgs.runCommand "firmware" {
        inherit left right;
        parts = [ "left" "right" ];
      } ''
        mkdir $out
        ln -s ${left}/zmk.uf2 $out/zmk_left.uf2
        ln -s ${right}/zmk.uf2 $out/zmk_right.uf2
      '';

      flash = zmk-nix.packages.${system}.flash.override { inherit firmware; };
      update = zmk-nix.packages.${system}.update;
    });

    devShells = forAllSystems (system: {
      default = zmk-nix.devShells.${system}.default;
    });
  };
}
