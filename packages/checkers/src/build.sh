#!/bin/sh
# Build checkers.elf, the native SwirlOS version (static-PIE SwirlWM client).
#   SWIRLOS_SRC=/path/to/SwirlOS-source  ./build.sh [out-dir]
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
for f in checkers assets; do gcc $CF -c "$f.c" -o "$OBJ/$f.o"; done
gcc $CF -I"$SRC/user" -I"$SRC/user/lib" -c native.c -o "$OBJ/native.o"
mkdir -p "$OUT"
ld -pie -T "$SRC/user/app_pie.ld" --gc-sections --build-id=none \
   -o "$OUT/checkers.elf" "$OBJ"/native.o "$OBJ"/checkers.o "$OBJ"/assets.o \
   $(for o in $OSOBJS; do echo "$SRC/$o"; done) "$SRC/build/libswirl.a" 2>&1 | grep -v "GNU-stack\|deprecated" || true
ls -l "$OUT/checkers.elf"
