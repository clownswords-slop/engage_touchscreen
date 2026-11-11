#!/bin/bash
# totower.sh : Somniel -> warp to the Tower of Trials (CapeTower, debug warpa) -> its menu (tutorials closed by outside taps)
D="$(dirname "$0")"; C="$D/cmd.sh"
k=$("$C" access | grep " CapeTower " | awk '{print $1}'); [ -n "$k" ] || { echo "no CapeTower"; exit 1; }
"$C" "warpa $k" >/dev/null; sleep 1; x=$("$C" access | grep CapeTower | awk '{printf "%d %d", $3*100, $4*100}'); "$C" "walkto $x" >/dev/null; sleep 3
"$D/where.sh" 30 | grep -q HubPlayCamera && { "$D/explore.sh" 60 >/dev/null; "$C" "walkto $x" >/dev/null; sleep 2; }
"$D/hubtap.sh" >/dev/null; sleep 4
for i in 1 2 3; do "$D/where.sh" 30 | grep -q "TutorialSequence" || break; "$C" "tap 60 680" >/dev/null; sleep 2.5; done
"$D/where.sh" 30 | head -1
