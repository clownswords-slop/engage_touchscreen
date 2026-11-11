#!/bin/bash
# tobattle.sh : TEST SAVES ONLY. From the Somniel (after loadauto.sh 2): + Menu -> next (World Map) -> chapter node ->
# Begin Chapter -> Yes -> through the events -> Start -> Yes -> to the first player phase. Only taps / debug buttons, never saves.
D="$(dirname "$0")"; C="$D/cmd.sh"
row(){ "$C" "menu" | grep "^[0-9]* $1 " | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*" | head -1 | tr ',' ' ' | awk '{print int(($2+$4)/2), int(($3+$5)/2)}'; }
tapr(){ p=$(row "$1"); [ -n "$p" ] || { echo "no $1"; return 1; }; "$C" "tap $p" >/dev/null; }
p=$("$C" "keyhelp" | grep " Plus + a=1" | grep -o "[0-9]*,[0-9]* [0-9]*,[0-9]*" | tr ',' ' ' | awk '{print int(($1+$3)/2), int(($2+$4)/2)}')
"$C" "tap ${p:-1014 26}" >/dev/null; sleep 3; tapr NextItem || exit 1; sleep 3; tapr DialogItemGmap || exit 1; sleep 10
for i in $(seq 1 20); do w=$("$D/where.sh" 30); echo "$w" | grep -q "GmapSequence" && ! echo "$w" | grep -q "Movie\|Dispose" && break
  echo "$w" | grep -q Movie && "$C" "btn 1024 4" >/dev/null; sleep 3; done
for i in $(seq 1 8); do "$C" "menu" | grep -q "GmapMenu " && break
  if "$D/where.sh" 25 | grep -q Tutorial; then "$C" "tap 60 680" >/dev/null; else "$C" "tap 640 360" >/dev/null; fi; sleep 3; done
if ! "$C" "menu" | grep -q EnterChapterItem; then "$C" "btn 2 4" >/dev/null; sleep 2; "$C" "btn 64 4" >/dev/null; sleep 6; "$C" "tap 640 360" >/dev/null; sleep 3; fi
tapr EnterChapterItem || exit 1; sleep 3; tapr EnterChapterDialogMainItemYes || exit 1; sleep 6
"$D/explore.sh" 150 >/dev/null; "$C" "menu" | grep -q StartMenuItem || { echo "no prep"; exit 1; }
tapr StartMenuItem; sleep 4; tapr ConfirmYesDialogItem; sleep 6; "$D/advance.sh" | tail -1
