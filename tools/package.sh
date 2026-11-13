#!/bin/bash
# package.sh : release build -> release/EngageTouchMod/exefs/subsdk8 + release/EngageTouchMod.zip
# No main.npdm in the package: Cobalt's grants the permissions the mod needs. A standalone npdm (the game's own + SdCard +
# the code-patching SVCs) can still be made with:  python tools/mknpdm.py work/exefs/main.npdm build/main.npdm
set -e; cd "$(dirname "$0")/.."
tools/build_local.sh
R=release/EngageTouchMod/exefs; rm -rf release; mkdir -p $R
cp build/subsdk8 $R/
(cd release && python -c "import shutil; shutil.make_archive('EngageTouchMod','zip','.','EngageTouchMod')")
ls -la $R release
