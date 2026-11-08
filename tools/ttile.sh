#!/bin/bash
# ttile.sh X Z : tap the centre of map tile (X, Z) through the normal tap path (screen point from the map camera)
D="$(dirname "$0")"
s=$("$D/cmd.sh" "tscr $1 $2" | grep "^scr") || { echo "no screen point"; exit 1; }
set -- $s; "$D/cmd.sh" "tap $2 $3" >/dev/null; echo "tap $2 $3"
