{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.gnumake
    pkgs.gcc
    pkgs.genimage
  ];

  buildInputs = [
    pkgs.unicorn
    pkgs.raylib
  ];
}
