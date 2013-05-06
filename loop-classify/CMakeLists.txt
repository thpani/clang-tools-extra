add_clang_executable(loop-classify
    LoopClassify.cpp
    PseudoConstantAnalysis.cpp
  )
target_link_libraries(loop-classify
  clangEdit clangTooling clangBasic clangAST clangASTMatchers)
