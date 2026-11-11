{
  description = "Oblio: firmware for the Yunzii B68 (SH68F90A + BK3632)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    utils.url = "github:numtide/flake-utils";
    sinowisp = {
      url = "github:karstx/sinowisp";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, utils, sinowisp }:
    utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # uCsim (SDCC's 8051 simulator) with the SH68F90 added as a CPU variant (-t sh68f90): the chip's USB controller and interrupt
        # vectors, timers, flash self-programming, watchdog and power-down. Taken from the SMK port (tools/ucsim/, GPLv2, see NOTICE.md).
        # It installs as `ucsim-sh68f90`, a name of its own: nixpkgs' sdcc already ships a stock `ucsim_51`.
        ucsim-sh68f90 = pkgs.stdenv.mkDerivation {
          pname = "ucsim-sh68f90";
          inherit (pkgs.sdcc) version;
          src = pkgs.sdcc.src;

          # the SDCC tarball does not ship the generated parser and lexer
          nativeBuildInputs = with pkgs; [ bison flex ];

          patches = [ ./tools/ucsim/sh68f90-register.patch ];

          postPatch = ''
            cp ${./tools/ucsim/sh68f90.cc}  sim/ucsim/src/sims/s51.src/sh68f90.cc
            cp ${./tools/ucsim/sh68f90cl.h} sim/ucsim/src/sims/s51.src/sh68f90cl.h
            flex -o sim/ucsim/src/core/cmd.src/cmdlex.cc sim/ucsim/src/core/cmd.src/cmdlex.l
          '';

          # only the mcs51 simulator: the others do not build and are not needed
          configurePhase = ''
            runHook preConfigure
            cd sim/ucsim
            ./configure \
              --enable-avr-sim=no   --enable-f8-sim=no     --enable-i8048-sim=no \
              --enable-i8085-sim=no --enable-m6800-sim=no  --enable-m6809-sim=no \
              --enable-m68hc08-sim=no --enable-m68hc11-sim=no --enable-m68hc12-sim=no \
              --enable-mos6502-sim=no --enable-oisc-sim=no --enable-p1516-sim=no \
              --enable-pblaze-sim=no --enable-pdk-sim=no   --enable-rxk-sim=no \
              --enable-st7-sim=no   --enable-stm8-sim=no   --enable-tlcs-sim=no \
              --enable-xa-sim=no    --enable-z80-sim=no
            runHook postConfigure
          '';

          buildPhase = ''
            runHook preBuild
            make
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin
            cp src/sims/s51.src/ucsim_51 $out/bin/ucsim-sh68f90
            runHook postInstall
          '';

          meta.description = "SDCC uCsim 8051 simulator with the SinoWealth SH68F90 as a CPU variant";
        };
      in
      {
        packages = { inherit ucsim-sh68f90; };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            sdcc          # 8051 compiler
            gnumake
            gcc           # host tests
            clang-tools   # clang-format
            python3
            nodejs        # the tests of the web updater (`make web-test`: node --test)
            ucsim-sh68f90 # the simulator for `make sim`
            binutils
            sinowisp.packages."${system}".default # flashing over the USB ISP
          ];

          # SDCC headers for the editor (IntelliSense / clangd): the store path changes with every sdcc update
          SDCC_INCLUDE = "${pkgs.sdcc}/share/sdcc/include";

          shellHook = ''
            ln -sfn "$SDCC_INCLUDE" .sdcc-include
          '';
        };
      });
}
