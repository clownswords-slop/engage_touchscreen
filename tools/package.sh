#!/bin/bash
# package.sh : release build -> release/EngageTouchMod/exefs/{subsdk9,main.npdm} + release/EngageTouchMod.zip
set -e; cd "$(dirname "$0")/.."
tools/build_local.sh
python tools/mknpdm.py work/exefs/main.npdm build/main.npdm
R=release/EngageTouchMod/exefs; rm -rf release; mkdir -p $R
cp build/subsdk9 build/main.npdm $R/
(cd release && python -c "import shutil; shutil.make_archive('EngageTouchMod','zip','.','EngageTouchMod')")
ls -la $R release
