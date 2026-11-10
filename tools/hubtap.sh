#!/bin/bash
# hubtap.sh : tap the "(A) Talk / Examine" label of the interactable in reach (hub / explore field) without a screenshot:
# a probe tap off the label makes the mod log the label's box ("hub access box x/y"), then the box centre is tapped.
D="$(dirname "$0")"; C="$D/cmd.sh"; L="$D/../../Logs/$(ls -t "$D/../../Logs" | head -1)"
"$C" "tap 640 690" >/dev/null; sleep 1
bx=$(grep -a "hub access box x" "$L" | tail -1 | awk '{print $(NF-1), $NF}'); by=$(grep -a "hub access box y" "$L" | tail -1 | awk '{print $(NF-1), $NF}')
[ -z "$bx" ] && { echo "no label"; exit 1; }
set -- $bx $by; x=$(( ($1+$2)/2 )); y=$(( ($3+$4)/2 )); "$C" "tap $x $y" >/dev/null; echo "label tap $x,$y"
