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
