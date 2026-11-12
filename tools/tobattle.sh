#!/bin/bash
# tobattle.sh : TEST SAVES ONLY. (FROMGMAP=1 tobattle.sh: start on the world map.) From the Somniel (after loadauto.sh 2): + Menu -> next (World Map) -> chapter node ->
# Begin Chapter -> Yes -> through the events -> Start -> Yes -> to the first player phase. Only taps / debug buttons, never saves.
D="$(dirname "$0")"; C="$D/cmd.sh"
row(){ "$C" "menu" | grep "^[0-9]* $1 " | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*" | head -1 | tr ',' ' ' | awk '{print int(($2+$4)/2), int(($3+$5)/2)}'; }
tapr(){ p=$(row "$1"); [ -n "$p" ] || { echo "no $1"; return 1; }; "$C" "tap $p" >/dev/null; }
if [ -z "$FROMGMAP" ]; then   # FROMGMAP=1: already on the world map
p=$("$C" "keyhelp" | grep " Plus + a=1" | grep -o "[0-9]*,[0-9]* [0-9]*,[0-9]*" | tr ',' ' ' | awk '{print int(($1+$3)/2), int(($2+$4)/2)}')
"$C" "tap ${p:-1014 26}" >/dev/null; sleep 3; if ! tapr NextItem; then "$C" "btn 2 3" >/dev/null; sleep 2   # no "next" row (Somniel after chapter 11): the World Map gate
  k=$("$C" access | grep " Move_Gmap " | awk '{print $1}'); [ -n "$k" ] || { echo "no Move_Gmap"; exit 1; }
  "$C" "warpa $k" >/dev/null; sleep 1; x=$("$C" access | grep " Move_Gmap " | awk '{printf "%d %d", $3*100, $4*100}'); "$C" "walkto $x" >/dev/null; sleep 3
  "$D/hubtap.sh" >/dev/null; fi; sleep 3; "$C" "menu" | grep -q DialogItemGmap && tapr DialogItemGmap; sleep 8
for k in 1 2 3 4; do "$D/where.sh" 30 | grep -q GmapSequence && break; "$D/explore.sh" 150 >/dev/null; "$C" "menu" | grep -q DialogItemGmap && { tapr DialogItemGmap; sleep 8; }; done
for i in $(seq 1 20); do w=$("$D/where.sh" 30); echo "$w" | grep -q "GmapSequence" && ! echo "$w" | grep -q "Movie\|Dispose" && break
  echo "$w" | grep -q Movie && "$C" "btn 1024 4" >/dev/null; sleep 3; done
fi
for i in $(seq 1 8); do "$C" "menu" | grep -q "GmapMenu " && break
  if "$D/where.sh" 25 | grep -q Tutorial; then "$C" "tap 60 680" >/dev/null; else "$C" "tap 640 360" >/dev/null; fi; sleep 3; done
if ! "$C" "menu" | grep -q EnterChapterItem; then "$C" "btn 2 4" >/dev/null; sleep 2; "$C" "btn 64 4" >/dev/null; sleep 6; "$C" "tap 640 360" >/dev/null; sleep 3; fi
tapr EnterChapterItem || exit 1; sleep 3; "$C" "menu" | grep -q EnterChapterDialogMainItemYes && tapr EnterChapterDialogMainItemYes; sleep 6
"$D/explore.sh" 150 >/dev/null
[ -n "$PREP" ] && { echo "at prep"; exit 0; }
if "$C" "menu" | grep -q StartMenuItem; then tapr StartMenuItem; sleep 4; tapr ConfirmYesDialogItem; sleep 6; fi; "$D/advance.sh" | tail -1
