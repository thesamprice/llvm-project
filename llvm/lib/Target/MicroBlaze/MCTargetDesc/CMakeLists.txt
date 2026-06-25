set(LLVM_TARGET_DEFINITIONS ../MicroBlaze.td)

tablegen(LLVM MicroBlazeGenAsmWriter.inc -gen-asm-writer)
add_public_tablegen_target(MicroBlazeDescTableGen)

add_llvm_component_library(LLVMMicroBlazeDesc
  MicroBlazeInstPrinter.cpp
  MicroBlazeMCAsmInfo.cpp
  MicroBlazeMCCodeEmitter.cpp
  MicroBlazeMCTargetDesc.cpp

  LINK_COMPONENTS
  MicroBlazeInfo
  MC
  Support
  TargetParser

  ADD_TO_COMPONENT
  MicroBlaze
)
