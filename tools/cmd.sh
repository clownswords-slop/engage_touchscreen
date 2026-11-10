#!/bin/bash
# usage: cmd.sh "<cmds separated by ;>" [timeout_s]   (debug build: sd:/engagetouch/cmd.txt -> out.txt)
# One request at a time across scripts running in parallel (autobattle + a weaken loop): a mkdir lock, taken over after 15 s.
D=/c/Users/zanek/AppData/Roaming/Ryujinx/sdcard/engagetouch; K=$D/cmd.lock
for i in $(seq 1 150); do mkdir "$K" 2>/dev/null && break
  [ $i -eq 150 ] && { rm -rf "$K"; mkdir "$K" 2>/dev/null; }; sleep 0.1; done
trap 'rmdir "$K" 2>/dev/null' EXIT
SEQ=1$(date +%s%N | cut -c9-15)
printf '%s\n%s\n' "$SEQ" "$(echo "$1" | tr ';' '\n')" > $D/cmd.txt
T=${2:-5}; for i in $(seq 1 $((T*10))); do sleep 0.1; if head -1 $D/out.txt 2>/dev/null | grep -q "seq $SEQ"; then sleep 0.2; cat $D/out.txt; exit 0; fi; done
echo "TIMEOUT"; exit 1
