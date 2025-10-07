{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.gnumake
    pkgs.ninja
    pkgs.gcc
    pkgs.genimage
  ];

  buildInputs = [
    pkgs.unicorn
    pkgs.raylib
  ];
}
