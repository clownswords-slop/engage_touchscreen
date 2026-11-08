#!/bin/bash
# act.sh UX UZ DX DZ [attack|wait] : by taps: pick the unit on (UX,UZ), move to (DX,DZ), then Attack (forecast's button) or Wait;
# taps through tutorials on the way; then skiptest.sh until the next player phase / menu if the turn ended.
D="$(dirname "$0")"; C="$D/cmd.sh"
tut(){ for i in 1 2 3 4 5; do "$D/where.sh" | grep -q Tutorial && { "$C" "tap 640 560" >/dev/null; sleep 2; }; done; }
mtap(){ l=$("$C" "menu" | grep "$1" | head -1); b=$(echo "$l" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return 1
  set -- $(echo "$b" | tr ',' ' '); "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 0.8
  "$C" "menu" | grep -q "^$(echo "$l" | awk '{print $1" "$2}')" && "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; return 0; }
pick(){ for k in 1 2 3; do "$D/ttile.sh" $1 $2 >/dev/null; sleep 1.5; tut; [ "$("$C" "label $("$D/where.sh" 20 | grep MapSequenceHuman | awk '{print $NF}')" | grep "^label" | awk '{print $2}')" = 4 ] && return 0; done; return 1; }
[ "$1" = - ] || [ "$1" = + ] || pick $1 $2 || { echo "no pick"; exit 1; }
[ "$1" = + ] || { "$D/ttile.sh" $3 $4 >/dev/null; sleep 2.5; tut; }; "$C" "menu" | grep -q "MapUnitCommandMenu" || { "$D/ttile.sh" $3 $4 >/dev/null; sleep 3; tut; }
if [ "$5" = attack ]; then mtap "^1 AttackMenuItem\|^0 AttackMenuItem" || { echo "no attack"; "$C" "menu" | head -6; exit 1; }; sleep 2.5
  "$C" "menu" | grep -q "AttackMenu " && { "$C" "tap 1040 118" >/dev/null; sleep 2.5; }
  "$D/where.sh" 20 | grep -q TargetSelect && { "$C" "tap 640 527" >/dev/null; sleep 1; }
else mtap FixedMenuItem; sleep 2; fi
timeout 300 "$D/skiptest.sh" 150 | grep -v "skip state\|after tap"
"$C" "units" | grep "force [01] "
