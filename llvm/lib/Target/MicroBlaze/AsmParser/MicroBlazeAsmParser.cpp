//===-- MicroBlazeAsmParser.cpp - Parse MicroBlaze assembly ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "TargetInfo/MicroBlazeTargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {

// MicroBlazeOperand — a single parsed instruction operand.
struct MicroBlazeOperand : public MCParsedAsmOperand {
  enum KindTy { Token, Register, Immediate } Kind;

  SMLoc StartLoc, EndLoc;

  struct TokOp {
    const char *Data;
    unsigned Length;
  };
  struct RegOp {
    MCRegister RegNo;
  };
  struct ImmOp {
    const MCExpr *Val;
  };

  union {
    TokOp Tok;
    RegOp Reg;
    ImmOp Imm;
  };

  MicroBlazeOperand(KindTy K, SMLoc S, SMLoc E)
      : Kind(K), StartLoc(S), EndLoc(E) {}

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  bool isToken() const override { return Kind == Token; }
  bool isReg() const override { return Kind == Register; }
  bool isImm() const override { return Kind == Immediate; }
  bool isMem() const override { return false; }

  StringRef getToken() const {
    assert(Kind == Token);
    return StringRef(Tok.Data, Tok.Length);
  }

  MCRegister getReg() const override {
    assert(Kind == Register);
    return Reg.RegNo;
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && isReg());
    Inst.addOperand(MCOperand::createReg(Reg.RegNo));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && isImm());
    if (auto *CE = dyn_cast<MCConstantExpr>(Imm.Val))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Imm.Val));
  }

  void print(raw_ostream &OS, const MCAsmInfo &) const override {
    if (isToken())
      OS << "Tok:" << StringRef(Tok.Data, Tok.Length);
    else if (isReg())
      OS << "Reg:" << Reg.RegNo;
    else
      OS << "Imm";
  }

  static std::unique_ptr<MicroBlazeOperand> createToken(StringRef Str,
                                                        SMLoc S) {
    auto Op = std::make_unique<MicroBlazeOperand>(Token, S, S);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    return Op;
  }

  static std::unique_ptr<MicroBlazeOperand> createReg(MCRegister RegNo, SMLoc S,
                                                      SMLoc E) {
    auto Op = std::make_unique<MicroBlazeOperand>(Register, S, E);
    Op->Reg.RegNo = RegNo;
    return Op;
  }

  static std::unique_ptr<MicroBlazeOperand> createImm(const MCExpr *Val,
                                                      SMLoc S, SMLoc E) {
    auto Op = std::make_unique<MicroBlazeOperand>(Immediate, S, E);
    Op->Imm.Val = Val;
    return Op;
  }
};

class MicroBlazeAsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;
  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;
  ParseStatus parseDirective(AsmToken DirectiveID) override {
    StringRef IDVal = DirectiveID.getString();
    // GNU assembler emits .ent/.end as function-boundary markers and .set for
    // symbol attributes. They carry no machine code; accept and skip them so
    // assembly files written for GNU as (e.g. picolibc setjmp.S) assemble
    // cleanly with LLVM's integrated assembler.
    if (IDVal == ".ent" || IDVal == ".end" || IDVal == ".set") {
      Parser.eatToEndOfStatement();
      return ParseStatus::Success;
    }
    return ParseStatus::NoMatch;
  }
  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override {
    return Match_InvalidOperand;
  }

#define GET_ASSEMBLER_HEADER
#include "MicroBlazeGenAsmMatcher.inc"

  bool parseOperand(OperandVector &Operands);

public:
  MicroBlazeAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                      const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII), Parser(Parser) {
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

} // namespace

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "MicroBlazeGenAsmMatcher.inc"

bool MicroBlazeAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                        SMLoc &EndLoc) {
  if (!tryParseRegister(Reg, StartLoc, EndLoc).isSuccess())
    return Error(StartLoc, "expected register");
  return false;
}

ParseStatus MicroBlazeAsmParser::tryParseRegister(MCRegister &Reg,
                                                  SMLoc &StartLoc,
                                                  SMLoc &EndLoc) {
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  StringRef Name = Tok.getString();
  Reg = MatchRegisterName(Name);
  if (!Reg)
    return ParseStatus::NoMatch;

  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();
  Parser.Lex();
  return ParseStatus::Success;
}

// Map named SPR identifiers (used with mfs/mts) to their 14-bit addresses.
// GAS accepts these names; without this table, rmsr/rear/etc. would fall
// through to parseExpression as undefined symbols producing value 0.
static int64_t matchSprName(StringRef Name) {
  return StringSwitch<int64_t>(Name)
      .Case("rpc", 0x0000)
      .Case("rmsr", 0x0001)
      .Case("rear", 0x0003)
      .Case("resr", 0x0005)
      .Case("rfsr", 0x0007)
      .Case("rbtr", 0x000B)
      .Case("redr", 0x000D)
      .Case("rslr", 0x0800)
      .Case("rshr", 0x0802)
      .Case("rpid", 0x1000)
      .Case("rzpr", 0x1001)
      .Case("rtlbx", 0x1002)
      .Case("rtlblo", 0x1003)
      .Case("rtlbhi", 0x1004)
      .Case("rtlbsx", 0x1005)
      .Default(-1);
}

bool MicroBlazeAsmParser::parseOperand(OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  SMLoc E;

  // Register?
  MCRegister Reg;
  if (tryParseRegister(Reg, S, E).isSuccess()) {
    Operands.push_back(MicroBlazeOperand::createReg(Reg, S, E));
    return false;
  }

  // Named SPR (rmsr, rpc, rear, …)?
  if (Parser.getTok().is(AsmToken::Identifier)) {
    StringRef Name = Parser.getTok().getString();
    int64_t Addr = matchSprName(Name);
    if (Addr >= 0) {
      E = Parser.getTok().getEndLoc();
      Parser.Lex();
      const MCExpr *Val = MCConstantExpr::create(Addr, Parser.getContext());
      Operands.push_back(MicroBlazeOperand::createImm(Val, S, E));
      return false;
    }
  }

  // Immediate or expression.
  const MCExpr *Val;
  if (Parser.parseExpression(Val))
    return true;
  E = Parser.getTok().getLoc();
  Operands.push_back(MicroBlazeOperand::createImm(Val, S, E));
  return false;
}

bool MicroBlazeAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                           StringRef Name, SMLoc NameLoc,
                                           OperandVector &Operands) {
  Operands.push_back(MicroBlazeOperand::createToken(Name, NameLoc));

  if (Parser.getTok().is(AsmToken::EndOfStatement))
    return false;

  while (true) {
    if (parseOperand(Operands))
      return true;
    if (Parser.getTok().isNot(AsmToken::Comma))
      break;
    Parser.Lex(); // consume ','
  }

  return false;
}

bool MicroBlazeAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                                  OperandVector &Operands,
                                                  MCStreamer &Out,
                                                  uint64_t &ErrorInfo,
                                                  bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  switch (MatchResult) {
  case Match_Success:
    Out.emitInstruction(Inst, getSTI());
    return false;
  case Match_MnemonicFail:
    return Error(IDLoc, "unrecognized instruction mnemonic");
  case Match_InvalidOperand: {
    SMLoc ErrLoc = IDLoc;
    if (ErrorInfo != ~0ULL && ErrorInfo < Operands.size())
      ErrLoc = Operands[ErrorInfo]->getStartLoc();
    return Error(ErrLoc, "invalid operand for instruction");
  }
  case Match_MissingFeature:
    return Error(IDLoc, "instruction requires a target feature not enabled");
  default:
    return Error(IDLoc, "unknown match failure");
  }
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeMicroBlazeAsmParser() {
  RegisterMCAsmParser<MicroBlazeAsmParser> X(getTheMicroBlazeELTarget());
}
