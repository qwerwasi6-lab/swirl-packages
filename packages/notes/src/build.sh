#!/bin/sh
# Build notes.elf, the native SwirlOS notes app (static-PIE SwirlWM client).
#
#   SWIRLOS_SRC=/path/to/SwirlOS-source  ./build.sh [out-dir]
#
# Needs a SwirlOS source tree (r58 or newer). Output lands in ../ by default,
# i.e. packages/notes/notes.elf, ready for tools/mkindex.py.
set -e
cd "$(dirname "$0")"
HERE=$(pwd)
OUT=${1:-"$HERE/.."}
SRC=${SWIRLOS_SRC:?set SWIRLOS_SRC to the SwirlOS source tree}
[ -f "$SRC/user/app_pie.ld" ] || { echo "build.sh: $SRC is not a SwirlOS tree" >&2; exit 1; }
OBJ=$(mktemp -d)
trap 'rm -rf "$OBJ"' EXIT

OSOBJS="build/deskgfx.u.o build/settings.u.o build/theme.u.o build/theme_parse.u.o build/appbridge.u.o"
make -C "$SRC" build/libswirl.a $OSOBJS >/dev/null

CF="-ffreestanding -fPIE -fno-stack-protector -fcf-protection=none -fno-builtin \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387 -O3 -Wall -Wextra \
 -Wno-unused-parameter -ffunction-sections -fdata-sections \
 -fno-asynchronous-unwind-tables -fno-unwind-tables"

gcc $CF -c core.c -o "$OBJ/core.o"
gcc $CF -I"$SRC/user" -I"$SRC/user/lib" -c native.c -o "$OBJ/native.o"

mkdir -p "$OUT"
ld -pie -T "$SRC/user/app_pie.ld" --gc-sections --build-id=none \
   -o "$OUT/notes.elf" "$OBJ"/native.o "$OBJ"/core.o \
   $(for o in $OSOBJS; do echo "$SRC/$o"; done) "$SRC/build/libswirl.a" 2>&1 | grep -v "GNU-stack\|deprecated" || true
ls -l "$OUT/notes.elf"
