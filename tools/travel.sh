#!/bin/bash
# travel.sh NAME : Somniel fast travel by taps: "X Somniel Map" prompt -> the location row whose text starts with NAME -> Travel.
D="$(dirname "$0")"; C="$D/cmd.sh"; N="$1"
tut(){ for i in 1 2 3 4 5 6; do "$D/where.sh" 25 | grep -q Tutorial || return 0; "$C" "tap 640 560" >/dev/null; sleep 2; done; }
if ! "$C" "menu" | head -2 | grep -q "^SolanelInfoMenu "; then   # map not open yet
x=$("$C" "keyhelp" | grep " X + a=1" | grep -o "[0-9]*,[0-9]* [0-9]*,[0-9]*" | tr ',' ' ')
set -- $x; [ -n "$1" ] && "$C" "tap $(( ($1+$3)/2 )) $(( ($2+$4)/2 ))" >/dev/null || "$C" "btn 4 4" >/dev/null; sleep 3; tut; fi
for k in $(seq 1 14); do b=$("$C" "menu" | grep "SolanelInfoMenuItem $N" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*"); [ -n "$b" ] && break
  "$C" "btn 32768 3" >/dev/null; sleep 0.5; done
[ -z "$b" ] && { echo "no row $N"; exit 1; }
set -- $(echo $b | tr ',' ' '); "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 0.8
"$C" "menu" | head -2 | grep -q SolanelInfoMenu && "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 2
"$C" "menu" | head -2 | grep -q ConfirmDialog && "$C" "tap 640 363" >/dev/null; sleep 6; tut; echo "travelled to $N"
