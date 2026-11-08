#!/bin/bash
# prologue.sh : play the first map's opening turns by taps only (from the first player phase of newgame.sh):
# turn 1 move (5,4) -> (5,6) and Wait; then stay and Attack the enemy that comes adjacent; tap through everything else.
D="$(dirname "$0")"; C="$D/cmd.sh"
tut(){ for i in 1 2 3 4 5 6; do if "$D/where.sh" | grep -q Tutorial; then "$C" "tap 640 560" >/dev/null; sleep 2; else return; fi; done; }
menu_tap(){ # tap the item of the focused menu whose class matches $1
  b=$("$C" "menu" | grep "$1" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return 1
  set -- $(echo "$b" | tr ',' ' '); x=$(( ($2+$4)/2 )); y=$(( ($3+$5)/2 )); "$C" "tap $x $y" >/dev/null; echo "menu tap $x $y"; }
"$D/ttile.sh" 5 4; sleep 1.5; "$D/ttile.sh" 5 6; sleep 2; "$D/ttile.sh" 5 6; sleep 3; tut
menu_tap FixedMenuItem; sleep 1.5; "$C" "menu" | grep -q FixedMenuItem && menu_tap FixedMenuItem
"$D/advance.sh"; "$C" "units" | grep -E "force (0|1) "
