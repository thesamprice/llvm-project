add_llvm_component_library(LLVMMicroBlazeDisassembler
  MicroBlazeDisassembler.cpp

  LINK_COMPONENTS
  CodeGenTypes
  MicroBlazeDesc
  MicroBlazeInfo
  MC
  MCDisassembler
  Support

  ADD_TO_COMPONENT
  MicroBlaze
)
