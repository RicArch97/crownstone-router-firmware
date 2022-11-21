{
  description = "Flake used to setup development environment for Zephyr on ESP32";

  # Allows for downloads in derivation
  nixConfig.sandbox = "relaxed";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # Dependency override
    junitparser = {
      url = "github:weiwei/junitparser/2.8.0";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    junitparser,
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
    nixpkgsFor = forAllSystems (system:
      import nixpkgs {
        inherit system;
        overlays = [
          # this package is not up to date in nixpkgs
          (final: prev: {
            python = prev.python.override {
              packageOverrides = python-final: python-prev: {
                junitparser = python-prev.junitparser.overrideAttrs (oldAttrs: {
                  src = junitparser;
                });
              };
            };
          })
        ];
      });
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
          version = "0.15.1";
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
              sha256 = "1i3ah6pwrs7kv2h1bzbhcpfv9yb08j6rizyajgfvyw1mwmjbg2zf";
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
        zephyrRules = let
          name = "zephyr-udev-rules";
          url = "https://sf.net/p/openocd/code/ci/master/tree/contrib/60-openocd.rules?format=raw";
        in
          pkgs.stdenvNoCC.mkDerivation {
            inherit name;

            src = builtins.fetchurl {
              inherit url;
              sha256 = "093ihdz706kw65z4ddihgnm2sql9s1zlb9a128pb5h5l8l7jhmfj";
            };

            # not an archive
            dontUnpack = true;

            installPhase = ''
              mkdir -p $out/etc/udev/rules.d
              cp $src $out/etc/udev/rules.d/60-openocd.rules
            '';
          };
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
            zephyrDeps
            zephyrSdk
            zephyrRules
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
