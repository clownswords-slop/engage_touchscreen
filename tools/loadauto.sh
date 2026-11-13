#!/bin/bash
# loadauto.sh [row] : boot -> top menu -> Continue -> save row (0 = Autosave, 1 = Slot 1 (never: the user's), 2 = Slot 2 ...)
# -> tap through to the player phase / next stop.
# Only loads; never saves (Slot 1 is the user's own save).
D="$(dirname "$0")"; C="$D/cmd.sh"
[ "$NOBOOTWAIT" ] || sleep 8; "$C" "tlog 1" >/dev/null; "$D/totop.sh" || exit 1
# Continue: one tap opens the list once the row is selected (a second tap could land on the list's Slot 1 row)
"$C" "tap 1100 327" >/dev/null; sleep 1.5; "$C" "menu" | grep -q "SaveDataMenu" || { "$C" "tap 1100 327" >/dev/null; sleep 3; }
"$C" "menu" | grep -q "SaveDataMenu" || { echo "no save list"; exit 1; }
R=${1:-0}; [ "$R" = 1 ] && { echo "row 1 is the user's save"; exit 1; }
# tap row R by its on-screen box (the list may open scrolled); scroll with the d-pad until it is visible
for k in 1 2 3 4 5 6; do b=$("$C" "menu" | grep "^$R MenuItem" | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*")
  if [ -n "$b" ]; then set -- $(echo "$b" | tr ',' ' '); [ $3 -ge 60 ] && [ $5 -le 700 ] && break; fi
  s=$("$C" "menu" | head -2 | grep -o "sel [0-9]*" | awk '{print $2}'); [ "$s" -gt "$R" ] && "$C" "btn 8192 3" >/dev/null || "$C" "btn 32768 3" >/dev/null; sleep 0.6; done
[ -n "$b" ] || { echo "row $R not found"; exit 1; }
Y=$(( ($3+$5)/2 )); "$C" "tap 640 $Y" >/dev/null; sleep 1
# the cursor may already have been on the row: then that tap loaded it
if "$C" "menu" | head -2 | grep -q "SaveDataMenu"; then "$C" "menu" | head -2 | grep -q "sel $R " || { echo "row $R not selected"; exit 1; }
  "$C" "tap 640 $Y" >/dev/null; fi; sleep 8
"$D/explore.sh" ${EXPLN:-120}
