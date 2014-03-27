#include "iostream"
#include "fstream"
#include "algorithm"

#include "boost/variant.hpp"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Driver/Options.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_os_ostream.h"

#include "CmdLine.h"
#include "CFGBuilder.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

using namespace sloopy;


int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();

  // parse options
  CommonOptionsParser OptionsParser(argc, argv);

  // setup clang tool
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  MatchFinder Finder;
  FunctionCallback FC;
  Finder.addMatcher(FunctionMatcher, &FC);

  // run
  unsigned ret = Tool.run(newFrontendActionFactory(&Finder));
  /* we continue even if sloopy failed on some file */

  // print statistics
  if (LoopStats) {
    DEBUG(llvm::dbgs() << "Preparing statistics...\n");
    if (!::llvm::DebugFlag) {
      llvm::errs() << ".";
    }
    std::string Filename("classifications_"+BenchName+".txt");
    std::string ErrorInfo;
    raw_fd_ostream ostream(Filename.c_str(), ErrorInfo);
    dumpClasses(ostream, OutputFormat::JSON);
    ostream.close();
  }

  return ret;
}
