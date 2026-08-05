; NOTE: Do not autogenerate
; RUN: llc -mtriple=microblazeel -mattr=+barrel-shift < %s | FileCheck %s
; REQUIRES: microblaze-registered-target

; Contiguous bit-field extract / insert lowering.
;
; GAS / hardware encoding is ASYMMETRIC between BSEFI and BSIFI:
;   BSEFI: bsefi rD, rA, WIDTH, START  → bits[10:6]=WIDTH, bits[4:0]=START
;   BSIFI: bsifi rD, rA, WIDTH, START  → bits[10:6]=START+WIDTH-1, bits[4:0]=START
;
; tryBSEFI matches (and (lshr x, c), low_mask); $immw = WIDTH, $imms = start.
; tryBSIFI matches (or (and base, inv_placed_mask) (shl src shift)).
;   The tied-register constraint ($rD_src = $rD) forces the allocator to
;   copy the base into the destination before the instruction runs.
;   $immw = WIDTH; getBSIFIImmWValue encoder stores START+WIDTH-1 in bits[10:6].

; A field anchored at bit 0 is already a single andi (shift=0 → tryBSEFI skips).
; CHECK-LABEL: bfx_bit0:
; CHECK:       andi r3, r5, 31
; CHECK-NOT:   bsefi
define i32 @bfx_bit0(i32 %x) {
  %m = and i32 %x, 31
  ret i32 %m
}

; Field at non-zero offset: (x >> 8) & 0xFF → bsefi rD, rA, 8, 8
;   width=8, start=8 → bits[10:6]=8, bits[4:0]=8
; CHECK-LABEL: bfx_off8:
; CHECK:       bsefi r3, r5, 8, 8
; CHECK-NOT:   bsrli
; CHECK-NOT:   andi
define i32 @bfx_off8(i32 %x) {
  %s = lshr i32 %x, 8
  %m = and i32 %s, 255
  ret i32 %m
}

; (x >> 12) & 0x3FF → bsefi rD, rA, 10, 12
;   width=10, start=12 → bits[10:6]=10, bits[4:0]=12
; CHECK-LABEL: bfx_mid:
; CHECK:       bsefi r3, r5, 10, 12
; CHECK-NOT:   bsrli
; CHECK-NOT:   andi
define i32 @bfx_mid(i32 %x) {
  %s = lshr i32 %x, 12
  %m = and i32 %s, 1023
  ret i32 %m
}

; Insert low 8 bits of %y into %x[15:8]:
;   PlacedMask = ~(-65281) & 0xFFFFFFFF = 0xFF00, Shift=8, Width=8
;   → addk rbase, r5, r0 (tied copy) + andi r4, r6, 255 + bsifi r3, r4, 8, 8
; CHECK-LABEL: bfins:
; CHECK:       bsifi
; CHECK-NOT:   bslli
; CHECK-NOT:   {{^[[:space:]]+or[[:space:]]}}
define i32 @bfins(i32 %x, i32 %y) {
  %yc = and i32 %y, 255
  %ys = shl i32 %yc, 8
  %xc = and i32 %x, -65281        ; ~0xFF00
  %r  = or i32 %xc, %ys
  ret i32 %r
}
