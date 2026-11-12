#!/bin/bash
# tl.sh [n] : last n [engagetouch] lines of the newest Ryujinx log
L=$(ls -t /e/emulators/Ryujinx/Logs/*.log | head -1); grep -a "engagetouch" "$L" | tail -${1:-20} | sed 's/.*OutputDebugString: \[engagetouch\] //'
