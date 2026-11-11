#!/bin/bash
# skipch.sh : TEST SAVES ONLY. From the Somniel / an explore field / the world map: into the next chapter's battle (tobattle.sh),
# win it at once (debug `win`: MapSituation.SetComplete), End Turn, tap through the clear scenes, save to Slot 2 (save2.sh only),
# and tap on to the next free-roam stop. Logs each stage.
D="$(dirname "$0")"; C="$D/cmd.sh"
w=$("$D/where.sh" 30)
if echo "$w" | grep -q "GmapSequence"; then FROMGMAP=1 "$D/tobattle.sh" | tail -1
else echo "$w" | grep -q "HubSequence App.HubSequence st 6 sus 0" || "$D/explore.sh" 150 >/dev/null
  if "$D/where.sh" 30 | grep -q GmapSequence; then FROMGMAP=1 "$D/tobattle.sh" | tail -1; else "$D/tobattle.sh" | tail -1; fi; fi
"$D/where.sh" 30 | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0" || { echo "not in a player phase"; "$D/where.sh" 30 | head -6; exit 1; }
"$C" "win" | grep ^win
"$C" "btn 2 3" >/dev/null; sleep 1; "$C" "btn 1024 4" >/dev/null; sleep 2
b=$("$C" "menu" | grep TurnItem | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*" | tr ',' ' ') || { echo "no End Turn"; exit 1; }
set -- $b; "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 5
for i in $(seq 1 30); do ms=$("$D/where.sh" 30 | grep " MapSequence App.MapSequence " | awk '{print $NF}'); [ -z "$ms" ] && break
  lb=$("$C" "label $ms" | grep ^label | awk '{print $2}'); [ "$lb" = 17 ] && "$C" "tap 640 400" >/dev/null; sleep 2
  "$C" "menu" | head -1 | grep -q SaveDataMenu && break; "$D/explore.sh" 20 >/dev/null; done
for i in 1 2 3; do "$C" "menu" | head -1 | grep -q SaveDataMenu && break; "$D/explore.sh" 150 >/dev/null; done
"$C" "menu" | head -1 | grep -q SaveDataMenu && "$D/save2.sh"
"$D/explore.sh" 150 | tail -2
