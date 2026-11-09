#!/bin/bash
# skiptest.sh : tap through talk / events like advance.sh; during a combat, read CombatSkip.state
# (CombatWorld.get_Instance 0x29255d0 -> m_Skip +0x78 -> +0x18: 0 Prohibited, 1 Waiting, 2 FadeOut ... 7 End)
# and tap once while it is Waiting, then report whether the state moved on (= the tap skipped the animation).
D="$(dirname "$0")"; C="$D/cmd.sh"
st(){ w=$("$C" "call 0x29255d0 0" | grep -o "ret 0x[0-9a-f]*" | cut -d' ' -f2); [ -z "$w" ] || [ "$w" = "0x0" ] && { echo none; return; }
  # read m_Skip (+0x78) directly: get_Skip 0x2935350 would GetComponent on a destroyed world and abort the game
  [ "$("$D/rd64.sh" $(printf '0x%x' $((w+0x10))))" = 0x0 ] && { echo none; return; }
  s=$("$D/rd64.sh" $(printf '0x%x' $((w+0x78)))); [ "$s" = 0x0 ] && { echo none; return; }
  [ "$("$D/rd64.sh" $(printf '0x%x' $((s+0x10))))" = 0x0 ] && { echo none; return; }
  "$C" "dump $(printf '0x%x' $((s+0x18))) 4" | sed -n 2p | awk '{print $2}'; }
for i in $(seq 1 ${1:-80}); do sleep 1; w=$("$D/where.sh" 14)
  if echo "$w" | grep -q "MapSequenceHuman App.MapSequenceHuman st 6 sus 0"; then echo "player phase"; exit 0; fi
  if echo "$w" | grep -q "^[A-Za-z]*Menu \|Dialog "; then echo "$w" | head -3; exit 2; fi
  if echo "$w" | grep -q "TalkKeyWait"; then "$C" "tap 640 300" >/dev/null; continue; fi
  if echo "$w" | grep -q "CombatSequence"; then s=$(st); [ "$s" != "$last" ] && echo "combat skip state $s"; last=$s
    if echo "$s" | grep -q "^0x1$"; then "$C" "tap 640 600" >/dev/null; sleep 0.3; echo "after tap: $(st)"; sleep 0.5; echo "after tap: $(st)"; fi
    continue; fi
  if echo "$w" | grep -q "TalkKeyWait\|Tutorial\|EventSequence\|MovieSequence"; then "$C" "tap 640 600" >/dev/null; fi
done; echo "timeout"; exit 1
