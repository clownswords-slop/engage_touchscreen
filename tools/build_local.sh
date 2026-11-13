#!/bin/bash
# Local Windows build with the pip "ziglang" package (bundles clang + lld).
# usage: tools/build_local.sh [debug]   -> build/subsdk8 (release, also packaged into release/ by package.sh) or build/subsdk8_dbg
set -e
cd "$(dirname "$0")/../src"
OUT=../build; mkdir -p $OUT/obj
EXTRA=""; NAME=subsdk8
if [ "$1" = debug ]; then EXTRA=-DTM_DEBUG; NAME=subsdk8_dbg; fi
Z="python -m ziglang"
CF="$EXTRA --target=aarch64-none-elf -march=armv8-a -O2 -fPIC -ffreestanding -fno-builtin -nostdlib -fno-stack-protector -fvisibility=default -mno-outline-atomics -fno-asynchronous-unwind-tables -ffunction-sections -fno-plt -ffixed-x18 -Wall -Wno-unused-function -Wno-unused-variable"
$Z clang $CF -c start.S -o $OUT/obj/start.o
$Z clang $CF -c main.c -o $OUT/obj/main.o
$Z ld.lld -shared -T link.ld --no-undefined-version -z now --hash-style=both $OUT/obj/start.o $OUT/obj/main.o -o $OUT/obj/touchmod.elf --unresolved-symbols=ignore-all -soname subsdk8
python elf2nso.py $OUT/obj/touchmod.elf $OUT/$NAME
md5sum $OUT/$NAME
# every release build also refreshes release/ (with and without main.npdm; never installed)
if [ "$1" != debug ]; then cd .. && tools/package.sh; fi
