#!/bin/bash
# goto.sh NAME|=INDEX [flip] : walk Alear (hub / explore field) to an interactable (debug `access`): the first active one whose
# AID contains NAME, or the one listed at INDEX (=9). Steers with the debug `walkto`; waits until she stops (arrived or stuck).
# Stuck short of it (a wall, a hedge): sidesteps 5 units to one side of the line (alternating sides, then wider), and retries.
D="$(dirname "$0")"; C="$D/cmd.sh"
if [ "${1:0:1}" = "=" ]; then l=$("$C" "access" | grep "^${1:1} " | head -1); else l=$("$C" "access" | grep " $1" | head -1); fi
[ -z "$l" ] && { echo "no $1"; exit 1; }
F=${2:-0}; set -- $l; IDX=$1; NAME=$2; TX=$3; TZ=$4
x=$(awk -v v=$TX 'BEGIN{printf "%d", v*100}'); z=$(awk -v v=$TZ 'BEGIN{printf "%d", v*100}')
dist(){ "$C" "access" | grep "^$IDX " | awk '{print $NF}'; }
walk(){ "$C" "walkto $1 $2 $F" >/dev/null; last=""; for i in $(seq 1 40); do sleep 1; d=$(dist); [ "$d" = "$last" ] && break; last=$d; done; }
walk $x $z
for t in 1 2 3 4; do
  awk -v d="$d" 'BEGIN{exit !(d>1.6)}' || break
  me=$("$C" "access" | sed -n 2p); set -- $me; MX=$2; MZ=$3
  side=$(( t%2 ? 1 : -1 )); w=$(( 5 + 3*((t-1)/2) ))
  # a point w units to the side of the line from Alear to the target, 2 units along it
  s=$(awk -v mx=$MX -v mz=$MZ -v tx=$TX -v tz=$TZ -v s=$side -v w=$w 'BEGIN{dx=tx-mx; dz=tz-mz; n=sqrt(dx*dx+dz*dz); if(n<0.01)n=0.01; ux=dx/n; uz=dz/n;
    printf "%d %d", (mx+ux*2-uz*w*s)*100, (mz+uz*2+ux*w*s)*100}')
  walk $s; walk $x $z
done
echo "$NAME at $TX,$TZ dist $d"
