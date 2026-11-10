#!/bin/bash
# goto.sh NAME [flip] : walk Alear (hub / explore field) to the first active interactable whose AID contains NAME (debug `access`),
# with the debug `walkto` steering; waits until she stops (arrived or stuck). Prints the remaining distance.
D="$(dirname "$0")"; C="$D/cmd.sh"
l=$("$C" "access" | grep " $1" | head -1); [ -z "$l" ] && { echo "no $1"; exit 1; }
set -- $l $2; x=$(awk -v v=$3 "BEGIN{printf \"%d\", v*100}"); z=$(awk -v v=$4 "BEGIN{printf \"%d\", v*100}")
"$C" "walkto $x $z ${7:-0}" >/dev/null; last=""
for i in $(seq 1 40); do sleep 1; d=$("$C" "access" | grep "^$1 " | awk '{print $NF}'); [ "$d" = "$last" ] && break; last=$d; done
echo "$2 at $3,$4 dist $d"
