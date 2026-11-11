#!/bin/bash
# autobattle.sh [mode-row 0..3] : TEST SAVES ONLY. Plays the map with the game's own Auto-Battle (map menu -> Auto-Battle -> row,
# default 1 = Charge) after weaken.sh; taps through talks / combat / tutorials / STAGE CLEAR; re-weakens and restarts Auto-Battle
# when the free player phase comes back; stops at a save list, a dialog, GAME OVER or the explore field.
D="$(dirname "$0")"; C="$D/cmd.sh"; R=${1:-1}
row(){ b=$("$C" "menu" | grep "$1" | head -1 | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return 1; set -- $(echo "$b" | tr ',' ' ')
  "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 0.8; "$C" "menu" | grep -q "$1" && "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 2; }
auto(){ "$D/weaken.sh" >/dev/null; "$C" "btn 2 3" >/dev/null; sleep 1; "$C" "btn 1024 4" >/dev/null; sleep 2; row OrderItem
  b=$("$C" "menu" | grep "^$R SubOrderMenuItem" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return; set -- $(echo "$b" | tr ',' ' ')
  "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 0.8; "$C" "menu" | grep -q SubOrderMenu && "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 2; echo "auto-battle"; }
for i in $(seq 1 400); do sleep 1.5; w=$("$D/where.sh" 25)
  m=$(echo "$w" | head -1); echo "$m" | grep -q "SaveDataMenu\|BasicDialog\|YesNoDialog" && { echo "stop: $m"; exit 2; }
  echo "$w" | grep -q "HubSequence App.HubSequence st 6 sus 0" && { echo "explore field"; exit 0; }
  ms=$(echo "$w" | grep " MapSequence App.MapSequence " | awk '{print $NF}'); lb=""; [ -n "$ms" ] && lb=$("$C" "label $ms" | grep ^label | awk '{print $2}')
  [ "$lb" = 18 ] && { echo "GAME OVER"; exit 3; }
  [ "$lb" = 17 ] && { "$C" "tap 640 400" >/dev/null; continue; }
  if echo "$w" | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0" && ! echo "$m" | grep -q "Menu "; then auto; continue; fi
  echo "$w" | grep -qE "TalkKeyWait|Tutorial|EventSequence|MovieSequence|CombatSequence|ProcBond|GodGrow|GameMessage" && "$C" "tap 640 300" >/dev/null
  [ $((i % 20)) = 0 ] && "$D/weaken.sh" >/dev/null
done; echo timeout
