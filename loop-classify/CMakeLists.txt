set(CMAKE_CXX_FLAGS "-g")

add_clang_executable(loop-classify
    LoopClassify.cpp
  )
target_link_libraries(loop-classify
  clangEdit clangTooling clangBasic clangAST clangASTMatchers)
