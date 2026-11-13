#!/bin/bash
# package.sh : release/ from build/subsdk8 (tools/build_local.sh runs it after every release build). Two variants:
#   release/EngageTouchMod_WithCobalt/exefs/subsdk8              (+ EngageTouchMod_WithCobalt.zip): for Cobalt, whose main.npdm covers us
#   release/EngageTouchMod_Standalone/exefs/{subsdk8,main.npdm}  (+ EngageTouchMod_Standalone.zip): without Cobalt; main.npdm =
#     the game's own + SdCard + the code-patching SVCs (tools/mknpdm.py)
# Only packaged, never installed: restart.ps1 installs subsdk8 alone.
set -e; cd "$(dirname "$0")/.."
[ -f build/subsdk8 ] || tools/build_local.sh
python tools/mknpdm.py work/exefs/main.npdm build/main.npdm
rm -rf release; mkdir -p release/EngageTouchMod_WithCobalt/exefs release/EngageTouchMod_Standalone/exefs
cp build/subsdk8 release/EngageTouchMod_WithCobalt/exefs/
cp build/subsdk8 build/main.npdm release/EngageTouchMod_Standalone/exefs/
(cd release && python -c "import shutil
for n in ('EngageTouchMod_WithCobalt','EngageTouchMod_Standalone'): shutil.make_archive(n,'zip','.',n)")
ls -la release release/*/exefs
