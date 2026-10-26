#!/usr/bin/env bash
# One-time asset step: split Configs/xbox_boot.mp4 into the frame sequence +
# OGG soundtrack that the dependency-free boot player (src/desktop/boot_anim.cpp)
# reads. Requires the ffmpeg CLI (MSYS2: pacman -S mingw-w64-x86_64-ffmpeg).
# The output is committed under Data/Boot, so normal builds never need ffmpeg.
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=Configs/xbox_boot.mp4
DST=Data/Boot
FPS=30
rm -rf "$DST" && mkdir -p "$DST/frames"
ffmpeg -v error -y -i "$SRC" -vf "fps=$FPS,scale=1280:720:flags=lanczos" -q:v 4 "$DST/frames/%04d.jpg"
ffmpeg -v error -y -i "$SRC" -vn -c:a libvorbis -q:a 5 "$DST/boot.ogg"
echo "$FPS" > "$DST/fps.txt"
echo "Boot animation: $(ls "$DST/frames" | wc -l) frames, $(du -sh "$DST" | cut -f1)"
