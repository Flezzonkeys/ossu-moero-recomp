#!/bin/bash
# usage: tools/build.sh [rom.nds]   builds ./ossu-sdl from your own ROM (Linux, needs python3, gcc, make, SDL3)
# Without a path it uses the .nds file in the main folder.
set -e
cd "$(dirname "$0")/.."
ROM="$1"
if [ -z "$ROM" ]; then
  set -- ./*.nds
  [ -f "$1" ] || { echo "No .nds file found. Put your ROM in this folder: $PWD"; exit 1; }
  ROM="$1"
fi
python3 -I tools/prepare.py "$ROM" work
python3 -I tools/recomp.py work gen
make -j"$(nproc)" BUILD=build-sdl TARGET=ossu-sdl CFLAGS_SRC=-DUSE_SDL LIBS="-lSDL3 -lm" ossu-sdl
echo
echo "Done! Start the game with: ./ossu-sdl"
