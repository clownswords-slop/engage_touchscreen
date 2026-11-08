#!/bin/bash
# totop.sh : from boot, press A until the title's top menu (Start / Continue / Options / DLC) has the input
D="$(dirname "$0")"
for i in $(seq 1 15); do sleep 3; if "$D/cmd.sh" "menu" | grep -q "StartMenuItem"; then echo top; exit 0; fi; "$D/cmd.sh" "btn 1 8" >/dev/null; done; echo "no top menu"; exit 1
