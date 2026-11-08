#!/bin/bash
# newgame.sh : from boot to the first player phase of a new game (debug build: the name keyboard is skipped).
# Normal / Casual / no network. Never touches existing saves (no save is loaded or written by these steps).
D="$(dirname "$0")"; C="$D/cmd.sh"
w(){ "$D/where.sh" 14 2>/dev/null; }
"$D/totop.sh" || exit 1
"$C" "tap 1100 260" >/dev/null; sleep 1; "$C" "tap 1100 260" >/dev/null
for i in $(seq 1 120); do sleep 1.5; s=$(w)
  case "$s" in
    *MapSequenceHuman*) echo "player phase"; exit 0;;
    *PlayerGenderSelectMenuSequence*) "$C" "btn 0x1000 6" >/dev/null; sleep 1; "$C" "btn 1 6" >/dev/null;;
    *NameCheckDialogItemYes*) "$C" "btn 0x2000 6" >/dev/null; sleep 0.5; "$C" "btn 1 6" >/dev/null;;
    *MonthMenuItem*) "$C" "btn 1 6" >/dev/null;;
    *NormalMenuItem*) "$C" "btn 1 6" >/dev/null;;
    *CasualMenuItem*) "$C" "btn 1 6" >/dev/null;;
    *NetworkServiceSelect*) "$C" "btn 0x8000 6" >/dev/null; sleep 0.5; "$C" "btn 1 6" >/dev/null;;
    *YesDialogItem*) "$C" "btn 0x2000 6" >/dev/null; sleep 0.5; "$C" "btn 1 6" >/dev/null;;
    *MovieSequence*) "$C" "btn 0x400 6" >/dev/null;;
    *EventSequence*) "$C" "btn 1 6" >/dev/null;;
  esac
done; echo "timeout"; w; exit 1
