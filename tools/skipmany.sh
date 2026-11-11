#!/bin/bash
# skipmany.sh N : TEST SAVES ONLY. skipch.sh N times; after each, log the free-roam stop (procs, + menu rows) to work/skip_log.txt.
D="$(dirname "$0")"; C="$D/cmd.sh"; L="$D/../work/skip_log.txt"
for n in $(seq 1 ${1:-1}); do echo "=== $(date +%T) chapter skip $n" >> $L
  "$D/skipch.sh" >> $L 2>&1; "$D/where.sh" 30 | head -6 >> $L
  tasklist //FI "IMAGENAME eq Ryujinx.exe" | grep -q Ryujinx || { echo "RYUJINX DIED" >> $L; exit 1; }
  grep -q "not in a player phase\|no End Turn" <(tail -12 $L) && { echo "stopped"; exit 1; }
done; echo done
