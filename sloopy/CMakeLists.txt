set(LLVM_REQUIRES_EH 1)
add_clang_executable(sloopy
    Sloopy.cpp
    PseudoConstantAnalysis.cpp
  )
target_link_libraries(sloopy
  clangEdit clangTooling clangBasic clangAST clangASTMatchers)
