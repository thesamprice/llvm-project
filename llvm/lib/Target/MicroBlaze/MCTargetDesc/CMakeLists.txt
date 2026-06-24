add_llvm_component_library(LLVMMicroBlazeDesc
  MicroBlazeMCAsmInfo.cpp
  MicroBlazeMCTargetDesc.cpp

  LINK_COMPONENTS
  MicroBlazeInfo
  MC
  Support
  TargetParser

  ADD_TO_COMPONENT
  MicroBlaze
)
