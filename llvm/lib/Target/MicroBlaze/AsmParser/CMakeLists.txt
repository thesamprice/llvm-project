add_llvm_component_library(LLVMMicroBlazeAsmParser
  MicroBlazeAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  MicroBlazeDesc
  MicroBlazeInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  MicroBlaze
)
