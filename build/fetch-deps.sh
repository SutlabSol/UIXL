#!/usr/bin/env bash
# Fetch and build the pinned bgfx / bx / bimg revisions used by XUI Launcher.
# Run from the build/ directory inside an MSYS2 MINGW64 shell (Windows) or a
# normal shell on Linux / macOS.
set -euo pipefail
mkdir -p "$(dirname "$0")/../third-party"
cd "$(dirname "$0")/../third-party"

fetch() { # name sha
  if [ ! -d "$1/.git" ]; then
    git init -q "$1"
    git -C "$1" remote add origin "https://github.com/bkaradzic/$1.git"
  fi
  git -C "$1" fetch -q --depth 1 origin "$2"
  git -C "$1" checkout -q FETCH_HEAD
}
fetch bx   217823bc0a05a7dcf29e5166708b82910cda0623
fetch bimg 5c7eabb1d73cb0c1a3c23c0268dd70e8dca972f7
fetch bgfx 9849fe96cba81355352e5355ee2d333ea70857ac

cd bgfx
case "$(uname -s)" in
  MINGW*|MSYS*)
    export MINGW=/mingw64
    ../bx/tools/bin/windows/genie.exe --gcc=mingw-gcc gmake
    make -C .build/projects/gmake-mingw-gcc -j"$(nproc)" bx bimg bgfx config=release64 CC=gcc CXX=g++ AR=ar ;;
  Linux)
    ../bx/tools/bin/linux/genie --gcc=linux-gcc gmake
    make -C .build/projects/gmake-linux-gcc -j"$(nproc)" bx bimg bgfx config=release64 ;;
  Darwin)
    ../bx/tools/bin/darwin/genie --gcc=osx-arm64 gmake
    make -C .build/projects/gmake-osx-arm64 -j"$(sysctl -n hw.ncpu)" bx bimg bgfx config=release ;;
esac
echo "bgfx libraries ready."
