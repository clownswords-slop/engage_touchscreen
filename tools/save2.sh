#!/bin/bash
# save2.sh : TEST SAVES ONLY. On a SaveDataMenu with the cursor on Slot 2 (row 2): confirm it and Yes on the overwrite dialog.
# Refuses unless row 2 is the selected row (Slot 1 is the user's).
D="$(dirname "$0")"; C="$D/cmd.sh"
"$C" "menu" | head -2 | grep -q "^SaveDataMenu .* sel 2 " || { echo "not on Slot 2"; exit 1; }
b=$("$C" "menu" | grep "^2 MenuItem" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*" | tr ',' ' '); set -- $b; Y=$(( ($3+$5)/2 ))
[ "$Y" -gt 230 ] || { echo "slot 2 box odd: $b"; exit 1; }
for i in 1 2 3; do "$C" "tap 640 $Y" >/dev/null; sleep 1.5; "$C" "menu" | head -1 | grep -q "YesNoDialog\|ConfirmDialog" && break; done
y=$("$C" "menu" | grep "^0 YesDialogItem\|^0 ConfirmYesDialogItem" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*" | tr ',' ' ') || { echo "no dialog"; exit 1; }
set -- $y; "$C" "tap 640 $(( ($3+$5)/2 ))" >/dev/null; sleep 4; echo "saved to slot 2"
