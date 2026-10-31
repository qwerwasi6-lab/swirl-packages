#!/bin/sh
# Build chess.exe (Windows x64 PE for SwirlRun) with Zig's bundled mingw-w64.  pip install ziglang
set -e
cd "$(dirname "$0")"
OUT=${1:-build}
mkdir -p "$OUT"
ZIG="python3 -m ziglang cc -target x86_64-windows-gnu"
CF="-O2 -ffreestanding -fno-builtin -fno-stack-protector -mgeneral-regs-only -Wall -Wextra -Wno-unused-parameter"
for f in engine ui assets selftest win crt; do
    $ZIG $CF -c $f.c -o "$OUT/$f.o"
done
$ZIG -nostdlib "$OUT"/engine.o "$OUT"/ui.o "$OUT"/assets.o "$OUT"/selftest.o "$OUT"/win.o "$OUT"/crt.o \
    -o "$OUT/chess.exe" \
    -Wl,--subsystem,windows -Wl,--entry,start -Wl,--dynamicbase -Wl,--stack,0x200000 \
    -lkernel32 -luser32 -lgdi32
rm -f "$OUT/chess.pdb"
ls -l "$OUT/chess.exe"
