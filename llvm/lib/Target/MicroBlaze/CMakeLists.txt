add_llvm_component_group(MicroBlaze)

set(LLVM_TARGET_DEFINITIONS MicroBlaze.td)

tablegen(LLVM MicroBlazeGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MicroBlazeGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM MicroBlazeGenCallingConv.inc -gen-callingconv)
tablegen(LLVM MicroBlazeGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MicroBlazeGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MicroBlazeGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM MicroBlazeGenMCCodeEmitter.inc -gen-emitter)

add_public_tablegen_target(MicroBlazeCommonTableGen)

add_llvm_target(MicroBlazeCodeGen
  MicroBlazeFrameLowering.cpp
  MicroBlazeISelLowering.cpp
  MicroBlazeInstrInfo.cpp
  MicroBlazeRegisterInfo.cpp
  MicroBlazeSubtarget.cpp
  MicroBlazeTargetMachine.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  MicroBlazeDesc
  MicroBlazeInfo
  SelectionDAG
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  MicroBlaze
  )

add_subdirectory(AsmParser)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
