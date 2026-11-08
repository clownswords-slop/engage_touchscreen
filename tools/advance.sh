#!/bin/bash
# advance.sh : tap through talk / tutorials / events / combat (screen centre-bottom) until the player phase has the input
D="$(dirname "$0")"
for i in $(seq 1 ${1:-60}); do sleep 2; w=$("$D/where.sh" 14)
  if echo "$w" | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0"; then echo "player phase"; exit 0; fi
  if echo "$w" | grep -q "^[A-Za-z]*Menu \|Dialog "; then echo "$w" | head -3; exit 2; fi
  if echo "$w" | grep -q "TalkKeyWait\|Tutorial\|EventSequence\|CombatSequence\|MovieSequence"; then "$D/cmd.sh" "tap 640 600" >/dev/null; fi
done; echo "timeout"; exit 1
