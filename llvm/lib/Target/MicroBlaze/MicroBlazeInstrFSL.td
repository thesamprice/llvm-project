//===-- MicroBlazeInstrFSL.td - MicroBlaze FSL/AXI-Stream Instructions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze FSL (Fast Simplex Link) / AXI-Stream instructions.
// UG984 §5.  Two instruction families:
//
//   Static  (opcode 0x1B): port number encoded in bits[7:4] as rfsl0..rfsl15.
//   Dynamic (opcode 0x13): port number supplied in a GPR (rB).
//
// Five modifier flags appear in both families:
//   T — test       (don't consume data from the FIFO)
//   N — non-blocking
//   E — exception  (raise exception on empty/full)
//   C — control channel
//   A — acknowledge
//
// Mnemonic letter order (when multiple prefixes apply): t, n, e, c, a.
// Example: T=1, N=1, E=1, C=1, A=1 → tnecaget / tnecagetd.
//
// GET  — 32 variants:  get rD, rfslN  /  getd rD, rB
// PUT  — 32 variants:  put rA, rfslN  /  putd rA, rB
// (each defm below generates one static + one dynamic def)
//
// The MBFSLG / MBFSLP / MBFSLDG / MBFSLDP format classes are defined in
// MicroBlazeInstrFormats.td.  The RFSL register class (rfsl0..rfsl15) is
// defined in MicroBlazeRegisterInfo.td.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Multiclass FSLGET — one defm generates a static + dynamic GET pair.
//
//   Static:  <name>   rD, rfslN   (MBFSLG, opcode 0x1B, P=0)
//   Dynamic: <name>d  rD, rB      (MBFSLDG, opcode 0x13, P=0)
//
// Parameters: name=base mnemonic (without 'd' suffix), N,C,T,A,E flag bits.
//===----------------------------------------------------------------------===//

multiclass FSLGET<string name, bit N, bit C, bit T, bit A, bit E> {
  // Static variant: encoded port in bits[7:4].
  def "" : MBFSLG<N, C, T, A, E> {
    let AsmString = name # "\t$rD, $port";
  }
  // Dynamic variant: port from GPR rB in bits[15:11].
  def "D" : MBFSLDG<N, C, T, A, E> {
    let AsmString = name # "d\t$rD, $rB";
  }
}

//===----------------------------------------------------------------------===//
// Multiclass FSLPUT — one defm generates a static + dynamic PUT pair.
//
//   Static:  <name>   rA, rfslN   (MBFSLP, opcode 0x1B, P=1)
//   Dynamic: <name>d  rA, rB      (MBFSLDP, opcode 0x13, P=1)
//===----------------------------------------------------------------------===//

multiclass FSLPUT<string name, bit N, bit C, bit T, bit A, bit E> {
  // Static variant: encoded port in bits[7:4].
  def "" : MBFSLP<N, C, T, A, E> {
    let AsmString = name # "\t$rA, $port";
  }
  // Dynamic variant: port from GPR rB in bits[15:11].
  def "D" : MBFSLDP<N, C, T, A, E> {
    let AsmString = name # "d\t$rA, $rB";
  }
}

//===----------------------------------------------------------------------===//
// FSL instruction instantiations — all 32 GET + 32 PUT flag combinations.
//
// Each defm expands to two defs (static + dynamic).  The outer let block sets
// hasSideEffects=1 for all: FSL ops read/write hardware FIFOs and therefore
// have observable side effects that must not be eliminated or reordered.
//
// Encoding summary for static GET (opcode 0x1B, P=0):
//   bits[14:10] = N,C,T,A,E  (MSB to LSB)
//   get=0x6C000000  eget=0x6C000400  ...  tnecaget=0x6C007C00
//
// Encoding summary for dynamic GET (opcode 0x13, P=0):
//   bits[9:5] = N,C,T,A,E  (MSB to LSB)
//   getd=0x4C000000  egetd=0x4C000020  ...  tnecagetd=0x4C0003E0
//===----------------------------------------------------------------------===//

let Predicates = [HasFSLPred], hasSideEffects = 1 in {

  //--------------------------------------------------------------------------
  // GET variants — 32 flag combinations (N=0..1, C=0..1, T=0..1, A=0..1, E=0..1)
  // Args to FSLGET: <mnemonic>  N  C  T  A  E
  //--------------------------------------------------------------------------

  defm GET      : FSLGET<"get",      0, 0, 0, 0, 0>;  // static 0x6C000000  dynamic 0x4C000000
  defm EGET     : FSLGET<"eget",     0, 0, 0, 0, 1>;  // static 0x6C000400  dynamic 0x4C000020
  defm AGET     : FSLGET<"aget",     0, 0, 0, 1, 0>;  // static 0x6C000800  dynamic 0x4C000040
  defm EAGET    : FSLGET<"eaget",    0, 0, 0, 1, 1>;  // static 0x6C000C00  dynamic 0x4C000060
  defm TGET     : FSLGET<"tget",     0, 0, 1, 0, 0>;  // static 0x6C001000  dynamic 0x4C000080
  defm TEGET    : FSLGET<"teget",    0, 0, 1, 0, 1>;  // static 0x6C001400  dynamic 0x4C0000A0
  defm TAGET    : FSLGET<"taget",    0, 0, 1, 1, 0>;  // static 0x6C001800  dynamic 0x4C0000C0
  defm TEAGET   : FSLGET<"teaget",   0, 0, 1, 1, 1>;  // static 0x6C001C00  dynamic 0x4C0000E0
  defm CGET     : FSLGET<"cget",     0, 1, 0, 0, 0>;  // static 0x6C002000  dynamic 0x4C000100
  defm ECGET    : FSLGET<"ecget",    0, 1, 0, 0, 1>;  // static 0x6C002400  dynamic 0x4C000120
  defm CAGET    : FSLGET<"caget",    0, 1, 0, 1, 0>;  // static 0x6C002800  dynamic 0x4C000140
  defm ECAGET   : FSLGET<"ecaget",   0, 1, 0, 1, 1>;  // static 0x6C002C00  dynamic 0x4C000160
  defm TCGET    : FSLGET<"tcget",    0, 1, 1, 0, 0>;  // static 0x6C003000  dynamic 0x4C000180
  defm TECGET   : FSLGET<"tecget",   0, 1, 1, 0, 1>;  // static 0x6C003400  dynamic 0x4C0001A0
  defm TCAGET   : FSLGET<"tcaget",   0, 1, 1, 1, 0>;  // static 0x6C003800  dynamic 0x4C0001C0
  defm TECAGET  : FSLGET<"tecaget",  0, 1, 1, 1, 1>;  // static 0x6C003C00  dynamic 0x4C0001E0
  defm NGET     : FSLGET<"nget",     1, 0, 0, 0, 0>;  // static 0x6C004000  dynamic 0x4C000200
  defm NEGET    : FSLGET<"neget",    1, 0, 0, 0, 1>;  // static 0x6C004400  dynamic 0x4C000220
  defm NAGET    : FSLGET<"naget",    1, 0, 0, 1, 0>;  // static 0x6C004800  dynamic 0x4C000240
  defm NEAGET   : FSLGET<"neaget",   1, 0, 0, 1, 1>;  // static 0x6C004C00  dynamic 0x4C000260
  defm TNGET    : FSLGET<"tnget",    1, 0, 1, 0, 0>;  // static 0x6C005000  dynamic 0x4C000280
  defm TNEGET   : FSLGET<"tneget",   1, 0, 1, 0, 1>;  // static 0x6C005400  dynamic 0x4C0002A0
  defm TNAGET   : FSLGET<"tnaget",   1, 0, 1, 1, 0>;  // static 0x6C005800  dynamic 0x4C0002C0
  defm TNEAGET  : FSLGET<"tneaget",  1, 0, 1, 1, 1>;  // static 0x6C005C00  dynamic 0x4C0002E0
  defm NCGET    : FSLGET<"ncget",    1, 1, 0, 0, 0>;  // static 0x6C006000  dynamic 0x4C000300
  defm NECGET   : FSLGET<"necget",   1, 1, 0, 0, 1>;  // static 0x6C006400  dynamic 0x4C000320
  defm NCAGET   : FSLGET<"ncaget",   1, 1, 0, 1, 0>;  // static 0x6C006800  dynamic 0x4C000340
  defm NECAGET  : FSLGET<"necaget",  1, 1, 0, 1, 1>;  // static 0x6C006C00  dynamic 0x4C000360
  defm TNCGET   : FSLGET<"tncget",   1, 1, 1, 0, 0>;  // static 0x6C007000  dynamic 0x4C000380
  defm TNECGET  : FSLGET<"tnecget",  1, 1, 1, 0, 1>;  // static 0x6C007400  dynamic 0x4C0003A0
  defm TNCAGET  : FSLGET<"tncaget",  1, 1, 1, 1, 0>;  // static 0x6C007800  dynamic 0x4C0003C0
  defm TNECAGET : FSLGET<"tnecaget", 1, 1, 1, 1, 1>;  // static 0x6C007C00  dynamic 0x4C0003E0

  //--------------------------------------------------------------------------
  // PUT variants — 32 flag combinations
  // Static base: 0x6C008000 (P=1, bit[15]=1); dynamic base: 0x4C000400 (P=1, bit[10]=1).
  //--------------------------------------------------------------------------

  defm PUT      : FSLPUT<"put",      0, 0, 0, 0, 0>;  // static 0x6C008000  dynamic 0x4C000400
  defm EPUT     : FSLPUT<"eput",     0, 0, 0, 0, 1>;  // static 0x6C008400  dynamic 0x4C000420
  defm APUT     : FSLPUT<"aput",     0, 0, 0, 1, 0>;  // static 0x6C008800  dynamic 0x4C000440
  defm EAPUT    : FSLPUT<"eaput",    0, 0, 0, 1, 1>;  // static 0x6C008C00  dynamic 0x4C000460
  defm TPUT     : FSLPUT<"tput",     0, 0, 1, 0, 0>;  // static 0x6C009000  dynamic 0x4C000480
  defm TEPUT    : FSLPUT<"teput",    0, 0, 1, 0, 1>;  // static 0x6C009400  dynamic 0x4C0004A0
  defm TAPUT    : FSLPUT<"taput",    0, 0, 1, 1, 0>;  // static 0x6C009800  dynamic 0x4C0004C0
  defm TEAPUT   : FSLPUT<"teaput",   0, 0, 1, 1, 1>;  // static 0x6C009C00  dynamic 0x4C0004E0
  defm CPUT     : FSLPUT<"cput",     0, 1, 0, 0, 0>;  // static 0x6C00A000  dynamic 0x4C000500
  defm ECPUT    : FSLPUT<"ecput",    0, 1, 0, 0, 1>;  // static 0x6C00A400  dynamic 0x4C000520
  defm CAPUT    : FSLPUT<"caput",    0, 1, 0, 1, 0>;  // static 0x6C00A800  dynamic 0x4C000540
  defm ECAPUT   : FSLPUT<"ecaput",   0, 1, 0, 1, 1>;  // static 0x6C00AC00  dynamic 0x4C000560
  defm TCPUT    : FSLPUT<"tcput",    0, 1, 1, 0, 0>;  // static 0x6C00B000  dynamic 0x4C000580
  defm TECPUT   : FSLPUT<"tecput",   0, 1, 1, 0, 1>;  // static 0x6C00B400  dynamic 0x4C0005A0
  defm TCAPUT   : FSLPUT<"tcaput",   0, 1, 1, 1, 0>;  // static 0x6C00B800  dynamic 0x4C0005C0
  defm TECAPUT  : FSLPUT<"tecaput",  0, 1, 1, 1, 1>;  // static 0x6C00BC00  dynamic 0x4C0005E0
  defm NPUT     : FSLPUT<"nput",     1, 0, 0, 0, 0>;  // static 0x6C00C000  dynamic 0x4C000600
  defm NEPUT    : FSLPUT<"neput",    1, 0, 0, 0, 1>;  // static 0x6C00C400  dynamic 0x4C000620
  defm NAPUT    : FSLPUT<"naput",    1, 0, 0, 1, 0>;  // static 0x6C00C800  dynamic 0x4C000640
  defm NEAPUT   : FSLPUT<"neaput",   1, 0, 0, 1, 1>;  // static 0x6C00CC00  dynamic 0x4C000660
  defm TNPUT    : FSLPUT<"tnput",    1, 0, 1, 0, 0>;  // static 0x6C00D000  dynamic 0x4C000680
  defm TNEPUT   : FSLPUT<"tneput",   1, 0, 1, 0, 1>;  // static 0x6C00D400  dynamic 0x4C0006A0
  defm TNAPUT   : FSLPUT<"tnaput",   1, 0, 1, 1, 0>;  // static 0x6C00D800  dynamic 0x4C0006C0
  defm TNEAPUT  : FSLPUT<"tneaput",  1, 0, 1, 1, 1>;  // static 0x6C00DC00  dynamic 0x4C0006E0
  defm NCPUT    : FSLPUT<"ncput",    1, 1, 0, 0, 0>;  // static 0x6C00E000  dynamic 0x4C000700
  defm NECPUT   : FSLPUT<"necput",   1, 1, 0, 0, 1>;  // static 0x6C00E400  dynamic 0x4C000720
  defm NCAPUT   : FSLPUT<"ncaput",   1, 1, 0, 1, 0>;  // static 0x6C00E800  dynamic 0x4C000740
  defm NECAPUT  : FSLPUT<"necaput",  1, 1, 0, 1, 1>;  // static 0x6C00EC00  dynamic 0x4C000760
  defm TNCPUT   : FSLPUT<"tncput",   1, 1, 1, 0, 0>;  // static 0x6C00F000  dynamic 0x4C000780
  defm TNECPUT  : FSLPUT<"tnecput",  1, 1, 1, 0, 1>;  // static 0x6C00F400  dynamic 0x4C0007A0
  defm TNCAPUT  : FSLPUT<"tncaput",  1, 1, 1, 1, 0>;  // static 0x6C00F800  dynamic 0x4C0007C0
  defm TNECAPUT : FSLPUT<"tnecaput", 1, 1, 1, 1, 1>;  // static 0x6C00FC00  dynamic 0x4C0007E0

} // let Predicates = [HasFSLPred], hasSideEffects = 1
