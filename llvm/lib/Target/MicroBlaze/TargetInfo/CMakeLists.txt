add_llvm_component_library(LLVMMicroBlazeInfo
  MicroBlazeTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  MicroBlaze
  )
