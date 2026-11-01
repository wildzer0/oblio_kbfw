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
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            sdcc          # 8051 compiler
            gnumake
            gcc           # host tests
            clang-tools   # clang-format
            python3
            binutils
            sinowisp.packages."${system}".default # flashing over the USB ISP
          ];
        };
      });
}
