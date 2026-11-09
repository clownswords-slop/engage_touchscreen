#!/bin/bash
# ttile.sh X Z : tap the centre of map tile (X, Z) through the normal tap path (screen point from the map camera).
# Waits until the camera holds still (two projections 0.25 s apart within 3 px) so the tap isn't made against a panning view.
D="$(dirname "$0")"; p=""
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do s=$("$D/cmd.sh" "tscr $1 $2" | grep "^scr") || { echo "no screen point"; exit 1; }
  set -- $1 $2 $s; if [ -n "$p" ]; then set -- $1 $2 $3 $4 $5 $p; d=$(( ($4-$6)*($4-$6) + ($5-$7)*($5-$7) )); [ $d -le 9 ] && break; p="$4 $5"; else p="$4 $5"; fi
  sleep 0.25; done
s=$("$D/cmd.sh" "tscr $1 $2" | grep "^scr"); set -- $s; "$D/cmd.sh" "tap $2 $3" >/dev/null; echo "tap $2 $3"
