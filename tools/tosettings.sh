#!/bin/bash
# tosettings.sh : from the hub / explore field (free roam): + -> System -> Settings (taps on the rows by their boxes)
D="$(dirname "$0")"; C="$D/cmd.sh"
row(){ b=$("$C" "menu" | grep "$1" | head -1 | grep -o "box [0-9]*,[0-9]* [0-9]*,[0-9]*") || return 1; set -- $(echo "$b" | tr ',' ' ')
  "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 0.8; "$C" "menu" | grep -q "$1" && "$C" "tap $(( ($2+$4)/2 )) $(( ($3+$5)/2 ))" >/dev/null; sleep 2; }
"$C" "btn 1024 4" >/dev/null; sleep 2; row SystemItem; row ConfigItem; "$C" "menu" | head -2
