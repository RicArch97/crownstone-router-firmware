{
  description = "Flake used to setup development environment for Zephyr on ESP32";

  # Allows for downloads in derivation
  nixConfig.sandbox = "relaxed";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    # to work with older version of flakes
    lastModifiedDate = self.lastModifiedDate or self.lastModified or "19700101";

    # Generate a user-friendly version number
    version = builtins.substring 0 8 lastModifiedDate;

    # System types to support
    supportedSystems = ["x86_64-linux" "aarch64-linux"];

    # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

    # Nixpkgs instantiated for supported system types
    nixpkgsFor = forAllSystems (system: import nixpkgs {inherit system;});

    lib = nixpkgs.lib;
  in {
    devShells = forAllSystems (
      system: let
        pkgs = nixpkgsFor.${system};

        # Zephyr Python requirements
        zephyrDeps = pkgs.python3.withPackages (python-packages:
          with python-packages; [
            # base
            pyelftools
            pyyaml
            pykwalify
            canopen
            packaging
            progress
            psutil
            pylink-square
            anytree
            intelhex
            west
            # build-test
            colorama
            ply
            gcovr
            coverage
            pytest
            mypy
            mock
            # compliance
            python-magic
            junitparser
            pylint
            # doc
            breathe
            sphinx
            sphinx-rtd-theme
            sphinx-copybutton
            # run-test
            pyserial
            tabulate
            cbor
          ]);

        # Download Zephyr SDK / setup toolchain
        zephyrSdk = let
          pname = "zephyr-sdk";
          version = "0.15.2";
          toolchain = "xtensa-espressif_esp32_zephyr-elf";

          system_fixup = {
            x86_64-linux = "linux-x86_64";
            aarch64-linux = "linux-aarch64";
          };

          # Use the minimal installer, select toolchain after
          url = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${version}/${pname}-${version}_${system_fixup.${system}}_minimal.tar.gz";
        in
          pkgs.stdenv.mkDerivation {
            # Allows derivation to access network, run with --option sandbox relaxed
            __noChroot = true;

            inherit pname version;

            src = builtins.fetchurl {
              inherit url;
              sha256 = "1szxb1xvhnmf96a0flri950pf0kbpycv8gpx5rjbh92bkq8dqcdf";
            };

            sourceRoot = ".";

            buildInputs = [
              pkgs.stdenv.cc.cc
              pkgs.python38
            ];

            nativeBuildInputs = [
              pkgs.autoPatchelfHook
              pkgs.cacert
              pkgs.which
              pkgs.wget
              pkgs.cmake
            ];

            # Required to prevent CMake running in configuration phases
            dontUseCmakeConfigure = true;

            # Run setup script, install ESP32 toolchain
            buildPhase = ''
              bash ${pname}-${version}/setup.sh -t ${toolchain}
            '';

            installPhase = ''
              mkdir -p $out
              mv $name $out/
            '';
          };

        # udev rules for microcontrollers
        openocdEsp32 = let
          pname = "openocd-esp32";
          version = "0.11.0-esp32-20221026";

          system_fixup = {
            x86_64-linux = "linux-amd64";
            aarch64-linux = "linux-arm64";
          };

          url = "https://github.com/espressif/openocd-esp32/releases/download/v${version}/${pname}-${system_fixup.${system}}-${version}.tar.gz";
        in
          pkgs.stdenv.mkDerivation {
            inherit pname version;

            src = builtins.fetchurl {
              inherit url;
              sha256 = "02vx9rs2dc3yg0f6fx7y5b23cw5s4b6aphjxv9icqq5bvyqyjqyf";
            };

            nativeBuildInputs = [pkgs.autoPatchelfHook];

            buildInputs = [pkgs.zlib pkgs.hidapi pkgs.libftdi1 pkgs.libusb1 pkgs.libgpiod];

            installPhase = ''
              mkdir -p $out
              cp -r bin share $out

              mkdir -p "$out/etc/udev/rules.d"
              rules="$out/share/openocd/contrib/60-openocd.rules"
              if [ ! -f "$rules" ]; then
                  echo "$rules is missing, must update the Nix file."
                  exit 1
              fi
              ln -s "$rules" "$out/etc/udev/rules.d/"
            '';
          };

        esp32DebugBuild = pkgs.writeShellScriptBin "esp32-debug-build" ''
          west build -b esp32 $1 -- -DOPENOCD=${openocdEsp32}/bin/openocd -DOPENOCD_DEFAULT_PATH=${openocdEsp32}/share/openocd/scripts
        '';
      in {
        default = pkgs.mkShell {
          name = "zephyr-shell";
          # Combine all required dependencies into the buildInputs (system + Python + Zephyr SDK)
          buildInputs = with pkgs; [
            cmake
            python3Full
            python3Packages.pip
            python3Packages.setuptools
            dtc
            git
            ninja
            gperf
            ccache
            dfu-util
            wget
            xz
            file
            gnumake
            gcc
            gcc_multi
            SDL2
            pyocd
            clang-tools
            zephyrDeps
            zephyrSdk
            openocdEsp32
            esp32DebugBuild
          ];

          # When shell is created, start with a few Zephyr related environment variables defined.
          shellHook = ''
            export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
            export ZEPHYR_SDK_INSTALL_DIR=${zephyrSdk}/${zephyrSdk.pname}-${zephyrSdk.version}
          '';
        };
      }
    );
  };
}
