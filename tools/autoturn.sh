#!/bin/bash
# autoturn.sh : play one player phase by taps. Each player unit goes for the nearest enemy: it tries the free tiles next to that
# enemy (nearest first) and attacks from the first one the game accepts; if none is reachable it moves as far towards the enemy
# as the game accepts and waits. Units that can't be picked (already acted) are skipped. Ends the turn from the map menu if
# the phase is still on afterwards, then taps through the enemy phase (explore.sh) to the next stop.
D="$(dirname "$0")"; C="$D/cmd.sh"
log(){ echo "[autoturn] $*"; }
tut(){ for i in 1 2 3 4 5; do "$D/where.sh" 20 | grep -q "Tutorial" && { "$C" "tap 640 560" >/dev/null; sleep 2; } || return 0; done; }
hlabel(){ p=$("$D/where.sh" 20 | grep "MapSequenceHuman App.MapSequenceHuman" | awk '{print $NF}'); [ -n "$p" ] && "$C" "label $p" | grep "^label" | awk '{print $2}'; }
human_free(){ "$D/where.sh" 20 | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0"; }
menu1(){ "$C" "menu" | sed -n 2p; }
mtap(){ l=$("$C" "menu" | grep "$1" | head -1); b=$(echo "$l" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return 1
  set -- $(echo "$b" | tr ',' ' '); "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 0.8
  "$C" "menu" | grep -q "^$(echo "$l" | awk '{print $1" "$2}')" && "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; return 0; }
# tap tile (x,z) until something happens: 0 = command menu up
tile_go(){ "$D/ttile.sh" $1 $2 >/dev/null; sleep 1.3; tut; menu1 | grep -q MapUnitCommandMenu && return 0
  "$D/ttile.sh" $1 $2 >/dev/null; sleep 2.2; tut; menu1 | grep -q MapUnitCommandMenu; }
pick(){ for k in 1 2; do "$D/ttile.sh" $1 $2 >/dev/null; sleep 1.3; tut; [ "$(hlabel)" = 4 ] && return 0
    menu1 | grep -q "MapSystemMenu" && { "$C" "btn 2 4" >/dev/null; sleep 1; return 1; }; done; return 1; }
# combat / events after an action: tap through until the free map cursor (or a menu / unknown screen)
settle(){ for i in $(seq 1 90); do sleep 1.5; w=$("$D/where.sh" 20)
    echo "$w" | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0" && [ "$(menu1 | grep -c Menu)" = 0 ] && return 0
    echo "$w" | head -1 | grep -q "Menu \|Dialog " && return 2
    echo "$w" | grep -q "MapSequenceEnemy\|MapSequenceAI" && return 3
    echo "$w" | grep -q "TalkKeyWait\|Tutorial\|EventSequence\|MovieSequence\|CombatSequence" && "$C" "tap 640 300" >/dev/null; done; return 1; }
act_menu(){   # command menu is up: attack if possible, else wait
  if "$C" "menu" | grep -q "EngageAttackMenuItem\|^[0-9] AttackMenuItem.*attr 0x1"; then
    mtap "^[0-9] AttackMenuItem.*attr 0x1" || mtap "EngageAttackMenuItem"; sleep 2.2
    "$C" "menu" | head -2 | grep -q "AttackMenu " && { "$C" "tap 1040 118" >/dev/null; sleep 2.2; }
    "$D/where.sh" 20 | grep -q TargetSelect && { "$C" "tap 640 527" >/dev/null; sleep 1; }
    log "attack"; else mtap "FixedMenuItem" && log "wait" || { "$C" "btn 2 4" >/dev/null; sleep 1; }; fi
  sleep 1; settle; }
dist(){ echo $(( ($1>$3?$1-$3:$3-$1) + ($2>$4?$2-$4:$4-$2) )); }
occupied(){ echo "$U" | awk -v x=$1 -v z=$2 '$2==x && $3==z {f=1} END{exit !f}'; }
done_ids=""
for round in 1 2 3 4 5 6 7 8 9 10 11 12; do
  human_free || break
  U=$("$C" "units" | grep "force [01] " | sed 's/ at \([-0-9]*\),\([-0-9]*\) force \([0-9]*\) id \([0-9]*\).*/ \1 \2 \3 \4/' | awk '$2>=0 {print $5, $2, $3, $4}')
  P=$(echo "$U" | awk '$4==0'); E=$(echo "$U" | awk '$4==1')
  [ -z "$E" ] && { log "no enemies"; break; }
  next=$(echo "$P" | while read id x z f; do echo " $done_ids " | grep -q " $id " || { echo "$id $x $z"; break; }; done)
  [ -z "$next" ] && break
  set -- $next; id=$1; ux=$2; uz=$3; done_ids="$done_ids $id"
  pick $ux $uz || { log "unit $id at $ux,$uz can't act"; continue; }
  # nearest enemy
  set -- $(echo "$E" | while read eid ex ez f; do echo "$(dist $ux $uz $ex $ez) $ex $ez"; done | sort -n | head -1); ex=$2; ez=$3
  log "unit $id $ux,$uz -> enemy $ex,$ez"
  ok=0
  for c in $(for t in "$((ex-1)) $ez" "$((ex+1)) $ez" "$ex $((ez-1))" "$ex $((ez+1))"; do set -- $t; [ $1 -ge 0 ] && [ $2 -ge 0 ] || continue
      { [ $1 = $ux ] && [ $2 = $uz ]; } || ! occupied $1 $2 || continue; echo "$(dist $ux $uz $1 $2) $1,$2"; done | sort -n | awk '{print $2}'); do
    tile_go ${c%,*} ${c#*,} && { ok=1; break; }
    [ "$(hlabel)" = 4 ] || pick $ux $uz || break
  done
  if [ $ok = 0 ]; then   # walk towards the enemy: the farthest accepted tile on a path that alternates x / z steps
    for d in 8 7 6 5 4 3 2 1; do x=$ux; z=$uz; for s in $(seq 1 $d); do
        if [ $(( s % 2 )) = 1 ] && [ $x != $ex ] || [ $z = $ez ]; then [ $x -lt $ex ] && x=$((x+1)) || { [ $x -gt $ex ] && x=$((x-1)); }
        else [ $z -lt $ez ] && z=$((z+1)) || { [ $z -gt $ez ] && z=$((z-1)); }; fi; done
      { [ $x = $ex ] && [ $z = $ez ]; } && continue; occupied $x $z && continue
      tile_go $x $z && { ok=1; break; }; [ "$(hlabel)" = 4 ] || pick $ux $uz || break; done
  fi
  if [ $ok = 0 ]; then tile_go $ux $uz && ok=1; fi
  if [ $ok = 1 ]; then act_menu; r=$?; [ $r = 2 ] && { log "menu/dialog after action"; "$D/where.sh" 20 | head -3; exit 2; }
  else "$C" "btn 2 4" >/dev/null; sleep 1; log "unit $id stuck"; fi
done
if human_free; then log "end turn"; "$C" "btn 1024 4" >/dev/null; sleep 1.5; mtap "TurnItem"; sleep 1.5
  "$C" "menu" | grep -q YesNoDialog && { mtap "YesDialogItem"; sleep 1; }; fi
timeout 400 "$D/explore.sh" 180
