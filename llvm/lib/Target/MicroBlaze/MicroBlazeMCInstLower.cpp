//===-- MicroBlazeMCInstLower.cpp - Lower MachineInstr to MCInst ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeMCInstLower.h"
#include "MicroBlazeBaseInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"

#define GET_REGINFO_ENUM
#include "MicroBlazeGenRegisterInfo.inc"
#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

using namespace llvm;

MicroBlazeMCInstLower::MicroBlazeMCInstLower(MCContext &Ctx,
                                             AsmPrinter &Printer)
    : Ctx(Ctx), Printer(Printer) {}

void MicroBlazeMCInstLower::Lower(const MachineInstr *MI,
                                   MCInst &OutMI) const {
  // LI32: 32-bit constant materialisation pseudo.
  // Expand to ADDIK rD, R0, imm32 — the MCCodeEmitter prepends an IMM
  // prefix automatically when the value does not fit in simm16 (UG984 §5.2).
  if (MI->getOpcode() == MicroBlaze::LI32) {
    OutMI.setOpcode(MicroBlaze::ADDIK);
    OutMI.addOperand(MCOperand::createReg(MI->getOperand(0).getReg()));
    OutMI.addOperand(MCOperand::createReg(MicroBlaze::R0));
    OutMI.addOperand(MCOperand::createImm(MI->getOperand(1).getImm()));
    return;
  }

  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp = LowerOperand(MO);
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}

MCOperand
MicroBlazeMCInstLower::LowerOperand(const MachineOperand &MO,
                                     int64_t Offset) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      return MCOperand();
    return MCOperand::createReg(MO.getReg());

  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm() + Offset);

  case MachineOperand::MO_GlobalAddress:
  case MachineOperand::MO_ExternalSymbol:
    return LowerSymbolOperand(MO);

  case MachineOperand::MO_ConstantPoolIndex: {
    const MCSymbol *CPSym = Printer.GetCPISymbol(MO.getIndex());
    const MCExpr *Expr = MCSymbolRefExpr::create(CPSym, Ctx);
    if (MO.getOffset())
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
    return MCOperand::createExpr(Expr);
  }

  case MachineOperand::MO_MachineBasicBlock:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx));

  case MachineOperand::MO_BlockAddress: {
    const MCSymbol *BASym = Printer.GetBlockAddressSymbol(MO.getBlockAddress());
    const MCExpr *Expr = MCSymbolRefExpr::create(BASym, Ctx);
    if (MO.getOffset())
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);
    return MCOperand::createExpr(Expr);
  }

  case MachineOperand::MO_JumpTableIndex: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(Printer.GetJTISymbol(MO.getIndex()), Ctx);
    // PIC: the jump-table base is materialised as an offset from the GOT base
    // (addik rD, r20, .LJTI@GOTOFF), a link-time constant.
    if (MO.getTargetFlags() == MicroBlazeII::MO_GOTOFF)
      Expr = MCSpecifierExpr::create(Expr, ELF::R_MICROBLAZE_GOTOFF_64, Ctx);
    return MCOperand::createExpr(Expr);
  }

  case MachineOperand::MO_RegisterMask:
    return MCOperand(); // skip — register masks are not MC operands.

  default:
    llvm::errs() << "MicroBlazeMCInstLower: unhandled MachineOperand type "
                 << (unsigned)MO.getType() << "\n";
    return MCOperand();
  }
}

MCOperand
MicroBlazeMCInstLower::LowerSymbolOperand(const MachineOperand &MO) const {
  const MCSymbol *Symbol;
  if (MO.isGlobal())
    Symbol = Printer.getSymbol(MO.getGlobal());
  else
    Symbol = Ctx.getOrCreateSymbol(MO.getSymbolName());

  const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, Ctx);
  if (MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);

  // Wrap with a target specifier for PIC relocations.
  unsigned TF = MO.getTargetFlags();
  if (TF == MicroBlazeII::MO_GOT)
    Expr = MCSpecifierExpr::create(Expr, ELF::R_MICROBLAZE_GOT_64, Ctx);
  else if (TF == MicroBlazeII::MO_PLT)
    Expr = MCSpecifierExpr::create(Expr, ELF::R_MICROBLAZE_PLT_64, Ctx);

  return MCOperand::createExpr(Expr);
}
