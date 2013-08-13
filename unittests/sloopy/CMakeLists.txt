set(LLVM_LINK_COMPONENTS support)

get_filename_component(SLOOPY_SOURCE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sloopy REALPATH)
include_directories(${SLOOPY_SOURCE_DIR})

SET(LLVM_REQUIRES_EH 1)

add_extra_unittest(SloopyTests LinearHelperTest.cpp)

target_link_libraries(SloopyTests clangTooling z3)
