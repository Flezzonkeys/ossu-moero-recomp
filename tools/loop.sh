#!/bin/bash
# usage: tools/loop.sh <rom.nds> <frames> [input-script]   runs the headless test build and adds missing code blocks until it runs through
ROM=$1; N=${2:-60}; mkdir -p out
for i in $(seq 1 60); do
  ./ossu --headless "$ROM" "$N" out "$N" $3 > out/run.log 2>&1; rc=$?
  tail -1 out/run.log | cut -c1-160
  [ $rc = 42 ] || { echo "exit rc=$rc after $i runs"; break; }
  python3 -I tools/recomp.py work gen >/dev/null && make -j"$(nproc)" ossu 2>&1 | grep -E 'error|Error' | head -5
done
