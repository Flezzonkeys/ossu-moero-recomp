#!/bin/bash
# usage: LLVM_MINGW=/pfad/llvm-mingw SDL3_MINGW=/pfad/SDL3-x.y.z/x86_64-w64-mingw32 tools/build-win.sh <rom.nds>
set -e
cd "$(dirname "$0")/.."
: "${LLVM_MINGW:?LLVM_MINGW setzen}" "${SDL3_MINGW:?SDL3_MINGW setzen}"
python3 -I tools/prepare.py "$1" work
python3 -I tools/recomp.py work gen
make -j"$(nproc)" BUILD=build-win TARGET=ossu.exe CC="$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" CFLAGS_SRC="-DUSE_SDL -I$SDL3_MINGW/include" LIBS="-L$SDL3_MINGW/lib -lSDL3 -lm -mwindows" ossu.exe
mkdir -p dist; cp ossu.exe "$SDL3_MINGW/bin/SDL3.dll" dist/
