#pragma once

llvm::cl::opt<std::string> BenchName("bench-name");
llvm::cl::opt<bool> LoopStats("loop-stats");
llvm::cl::opt<bool> AllowInfiniteLoops("allow-infinite-loops");
llvm::cl::opt<bool> ViewSliced("view-sliced");
llvm::cl::opt<bool> ViewSlicedOuter("view-sliced-outer");
llvm::cl::opt<bool> ViewUnsliced("view-unsliced");
llvm::cl::opt<bool> ViewCFG("view-cfg");
llvm::cl::opt<bool> DumpBlocks("dump-blocks");
llvm::cl::opt<bool> DumpIncrementVars("dump-increment-vars");
llvm::cl::opt<bool> DumpControlVars("dump-control-vars");
llvm::cl::opt<bool> DumpControlVarsDetail("dump-control-vars-detail");
llvm::cl::opt<std::string> HasClass("has-class");
llvm::cl::opt<bool> DumpClasses("dump-classes");
llvm::cl::opt<bool> DumpClassesAll("dump-classes-all");
llvm::cl::opt<bool> DumpAST("dump-ast");
llvm::cl::opt<bool> DumpStmt("dump-stmt");
llvm::cl::opt<std::string> Function("func");
llvm::cl::opt<std::string> File("file");
llvm::cl::opt<unsigned> Line("line");
llvm::cl::opt<bool> DumpZ3("dump-z3");
llvm::cl::opt<bool> EnableAmortized("enable-amortized");
