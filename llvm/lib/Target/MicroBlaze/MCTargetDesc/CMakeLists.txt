add_llvm_component_library(LLVMMicroBlazeDesc
  MicroBlazeAsmBackend.cpp
  MicroBlazeELFObjectWriter.cpp
  MicroBlazeInstPrinter.cpp
  MicroBlazeMCAsmInfo.cpp
  MicroBlazeMCCodeEmitter.cpp
  MicroBlazeMCTargetDesc.cpp

  LINK_COMPONENTS
  MicroBlazeInfo
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  MicroBlaze
)
