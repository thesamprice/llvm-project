add_llvm_component_group(MicroBlaze)

set(LLVM_TARGET_DEFINITIONS MicroBlaze.td)

tablegen(LLVM MicroBlazeGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MicroBlazeCommonTableGen)

add_llvm_target(MicroBlazeCodeGen
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

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
