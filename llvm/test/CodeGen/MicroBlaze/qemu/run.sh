#!/bin/sh
# Bare-metal QEMU smoke test for the MicroBlaze backend.
#
# Requires:
#   - clang with microblazeel target registered
#   - llvm-mc, ld.lld on PATH (or override LLVM_BIN)
#   - qemu-system-microblazeel (override QEMU_MICROBLAZE)
#
# Exit 0 = 'P' received on UART (all tests passed)
# Exit 1 = 'F' received or timeout

set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="${LLVM_BIN:-$(dirname "$(which llvm-mc)")}"
QEMU="${QEMU_MICROBLAZE:-qemu-system-microblazeel}"
TMP="${TMPDIR:-/tmp}"

OBJ_HARNESS="$TMP/mb_harness.o"
OBJ_CRT0="$TMP/mb_crt0.o"
ELF="$TMP/mb_test.elf"
UART="$TMP/mb_uart.txt"

"$BUILD/clang" --target=microblazeel-unknown-elf \
    -ffreestanding -nostdlib -O1 -c "$DIR/harness.c" -o "$OBJ_HARNESS"

"$BUILD/llvm-mc" -filetype=obj -triple=microblazeel \
    "$DIR/crt0.s" -o "$OBJ_CRT0"

"$BUILD/ld.lld" -T "$DIR/linker.ld" --entry=_start \
    "$OBJ_CRT0" "$OBJ_HARNESS" -o "$ELF"

timeout 4 "$QEMU" -M petalogix-s3adsp1800 -kernel "$ELF" \
    -nographic -serial "file:$UART" 2>/dev/null || true

RESULT="$(cat "$UART" | tr -d '\n' | head -c 1)"
if [ "$RESULT" = "P" ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL (got: $(xxd "$UART" | head -1))"
    exit 1
fi
