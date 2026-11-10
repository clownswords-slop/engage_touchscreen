#!/bin/bash
# goto.sh NAME|=INDEX [flip] : walk Alear (hub / explore field) to an interactable (debug `access`): the first active one whose
# AID contains NAME, or the one listed at INDEX (=9). Steers with the debug `walkto`; waits until she stops (arrived or stuck).
D="$(dirname "$0")"; C="$D/cmd.sh"
if [ "${1:0:1}" = "=" ]; then l=$("$C" "access" | grep "^${1:1} " | head -1); else l=$("$C" "access" | grep " $1" | head -1); fi
[ -z "$l" ] && { echo "no $1"; exit 1; }
F=${2:-0}; set -- $l
x=$(awk -v v=$3 'BEGIN{printf "%d", v*100}'); z=$(awk -v v=$4 'BEGIN{printf "%d", v*100}')
"$C" "walkto $x $z $F" >/dev/null; last=""
for i in $(seq 1 40); do sleep 1; d=$("$C" "access" | grep "^$1 " | awk '{print $NF}'); [ "$d" = "$last" ] && break; last=$d; done
echo "$2 at $3,$4 dist $d"
