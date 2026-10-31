#!/bin/sh
# Build chess.elf, the native SwirlOS version (static-PIE SwirlWM client).
#
#   SWIRLOS_SRC=/path/to/SwirlOS-source  ./build.sh [out-dir]
#
# Needs a SwirlOS source tree (r58 or newer: wmclient / app_pie.ld) and its
# normal host toolchain (gcc + ld).  The output lands in ../ by default, i.e.
# packages/chess/chess.elf, ready for tools/mkindex.py.
set -e
cd "$(dirname "$0")"
HERE=$(pwd)
OUT=${1:-"$HERE/.."}
SRC=${SWIRLOS_SRC:?set SWIRLOS_SRC to the SwirlOS source tree}
[ -f "$SRC/user/app_pie.ld" ] || { echo "build.sh: $SRC is not a SwirlOS tree" >&2; exit 1; }
OBJ=$(mktemp -d)
trap 'rm -rf "$OBJ"' EXIT

# the OS-side objects every GUI client links (libswirl.a = syscalls + libc bits)
OSOBJS="build/deskgfx.u.o build/settings.u.o build/theme.u.o build/theme_parse.u.o build/appbridge.u.o"
make -C "$SRC" build/libswirl.a $OSOBJS >/dev/null

# same code generation flags as the OS's own user programs (make UFLAGS)
CF="-ffreestanding -fPIE -fno-stack-protector -fcf-protection=none -fno-builtin \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387 -O3 -Wall -Wextra \
 -Wno-unused-parameter -ffunction-sections -fdata-sections \
 -fno-asynchronous-unwind-tables -fno-unwind-tables"

# portable game modules: no OS headers on purpose (engine.h has its own types)
for f in engine ui assets selftest; do
    gcc $CF -DDBG_DEFAULT=0 -c "$f.c" -o "$OBJ/$f.o"
done
# the SwirlOS glue
gcc $CF -I"$SRC/user" -I"$SRC/user/lib" -c native.c -o "$OBJ/native.o"

mkdir -p "$OUT"
ld -pie -T "$SRC/user/app_pie.ld" --gc-sections --build-id=none \
   -o "$OUT/chess.elf" "$OBJ"/native.o "$OBJ"/engine.o "$OBJ"/ui.o "$OBJ"/assets.o "$OBJ"/selftest.o \
   $(for o in $OSOBJS; do echo "$SRC/$o"; done) "$SRC/build/libswirl.a" 2>&1 | grep -v "GNU-stack\|deprecated" || true
ls -l "$OUT/chess.elf"
