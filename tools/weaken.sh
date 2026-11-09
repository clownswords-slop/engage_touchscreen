#!/bin/bash
# weaken.sh : TEST SAVES ONLY. Sets every enemy's (force 1) HP to 1: Unit +0x8b m_Hp (top byte of the word at +0x88) and the
# byte at +0x8c (both hold the current HP), keeping the other bytes.
D="$(dirname "$0")"; C="$D/cmd.sh"
"$C" "units" | grep "force 1 " | while read i at xz f fv id iv u hp hv; do
  a=$(printf '0x%x' $((u+0x88))); w=$("$C" "dump $a 8" | sed -n 2p); w1=$(echo "$w" | awk '{print $2}'); w2=$(echo "$w" | awk '{print $3}')
  [ -z "$w1" ] && continue
  "$C" "w32 $a $(printf '0x%x' $(( (w1 & 0x00ffffff) | 0x01000000 )));w32 $(printf '0x%x' $((u+0x8c))) $(printf '0x%x' $(( (w2 & 0xffffff00) | 1 )))" >/dev/null; done
"$C" "units" | grep "force 1 " | awk '{print $NF}' | sort | uniq -c
