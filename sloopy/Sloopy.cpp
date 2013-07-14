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

  /* ----- Parse options ----- */

  // This causes options to be parsed.
  CommonOptionsParser OptionsParser(argc, argv);

  RefactoringTool Tool(OptionsParser.getCompilations(),
                       OptionsParser.getSourcePathList());
  MatchFinder Finder;

  FunctionCallback FC;
  Finder.addMatcher(FunctionMatcher, &FC);

  if (Tool.run(newFrontendActionFactory(&Finder)) != 0) {
    return 1;
  }

  /* ----- Print stats ----- */
  if (LoopStats) {
    llvm::errs() << "Preparing statistics...\n";
    std::string Filename("classifications_"+BenchName+".txt");
    std::string ErrorInfo;
    raw_fd_ostream ostream(Filename.c_str(), ErrorInfo);
    dumpClasses(ostream, OutputFormat::JSON);
    ostream.close();
  }

  return 0;
}
