#!/bin/bash
# where.sh : focused BasicMenu + the running procs (without the always-there helpers)
"$(dirname "$0")/cmd.sh" "menu;procs ${1:-10}" ${2:-5} | grep -v -E "^seq|^RingInfo|^ *(RingInfo|UnitStatus|UnitInfo|SoundBankManager|ProcSceneManager) |^root|ProcInst (Hi|Def|Low)"
