#!/bin/bash
# explore.sh [N] : tap through talk / tutorials / events / movies / combat; stop at a menu, the player phase, or a screen
# none of those (prints the active procs). Only taps (the same path as a touch).
D="$(dirname "$0")"; C="$D/cmd.sh"; idle=0
for i in $(seq 1 ${1:-120}); do sleep 2; w=$("$D/where.sh" 20)
  if echo "$w" | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0"; then echo "player phase"; exit 0; fi
  if echo "$w" | head -1 | grep -q "Menu \|Dialog "; then echo "$w" | head -1; exit 2; fi
  # log each new tutorial / message's text (work/seen_texts.txt) before tapping it away: reading new systems blind
  t=$(echo "$w" | grep -E "TutorialSequence App|GameMessage App" | head -1 | awk '{print $NF}')
  if [ -n "$t" ] && [ "$t" != "$lastt" ]; then lastt=$t; { echo "== $(date +%T) $(echo "$w" | grep -E "TutorialSequence App|GameMessage App" | head -1 | awk '{print $1}')"; "$C" "ptexts $t" | grep -v "^seq\|^+0x"; } >> "$D/../work/seen_texts.txt"; fi
  if echo "$w" | grep -qE "TalkKeyWait|Tutorial|EventSequence|MovieSequence|CombatSequence|TitleLoop|GameMessage|ProcBond|GodGrow|DemoTelop"; then "$C" "tap 640 300" >/dev/null; idle=0; continue; fi
  if [ $idle -ge 2 ]; then m=$(echo "$w" | grep " MapSequence App.MapSequence " | awk "{print \$NF}"); [ -n "$m" ] && "$C" "label $m" | grep -q "^label 17$" && { "$C" "tap 640 400" >/dev/null; idle=0; continue; }; fi
  idle=$((idle+1)); if [ $idle -ge 4 ]; then echo "unknown:"; echo "$w" | grep "sus 0" | grep -v "AutoLogout\|DamageInfo\|MiniMap\|MapKeyHelp\|ProcFade"; exit 3; fi
done; echo "timeout"; exit 1
