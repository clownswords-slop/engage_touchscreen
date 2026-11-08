#!/bin/bash
# loadauto.sh : boot -> top menu -> Continue -> the Autosave slot (row 0) -> tap through to the player phase.
# Only loads; never saves (Slot 1 is the user's own save).
D="$(dirname "$0")"; C="$D/cmd.sh"
sleep 8; "$C" "tlog 1" >/dev/null; "$D/totop.sh" || exit 1
"$C" "tap 1100 327" >/dev/null; sleep 1; "$C" "tap 1100 327" >/dev/null; sleep 3
"$C" "menu" | grep -q "SaveDataMenu" || { echo "no save list"; exit 1; }
"$C" "tap 640 160" >/dev/null; sleep 1; "$C" "tap 640 160" >/dev/null; sleep 8
"$D/advance.sh" 90
