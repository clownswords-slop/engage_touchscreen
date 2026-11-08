#!/bin/bash
# toforecast.sh : from the prologue's first player phase (loadauto.sh): turn 1 Alear (5,4)->(5,6) Wait, turn 2 (5,6)->(5,9)
# Attack -> the forecast window. All by taps.
D="$(dirname "$0")"; C="$D/cmd.sh"
tut(){ for i in 1 2 3 4; do "$D/where.sh" | grep -q Tutorial && { "$C" "tap 640 560" >/dev/null; sleep 2; }; done; }
mtap(){ b=$("$C" "menu" | grep "$1" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return 1
  set -- $(echo "$b" | tr ',' ' '); "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; }
"$D/ttile.sh" 5 4 >/dev/null; sleep 1.5; "$D/ttile.sh" 5 6 >/dev/null; sleep 2; "$D/ttile.sh" 5 6 >/dev/null; sleep 3; tut
mtap FixedMenuItem; sleep 2; "$D/advance.sh" 90 || exit 1
"$D/ttile.sh" 5 6 >/dev/null; sleep 1.5; "$D/ttile.sh" 5 9 >/dev/null; sleep 2; "$D/ttile.sh" 5 9 >/dev/null; sleep 3; tut
mtap AttackMenuItem; sleep 2.5; "$D/where.sh" 20 | grep -q MapSequenceTargetSelect && echo forecast
