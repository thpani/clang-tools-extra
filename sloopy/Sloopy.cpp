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
#include "Time.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

using namespace sloopy;

double percentage(unsigned Count, unsigned N) {
  if (N == 0) return 0;
  return 100. * Count / N;
}

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();

  // parse options
  CommonOptionsParser OptionsParser(argc, argv);


  if (MachineLearningFormat) {
    std::cout << "benchmark\tbounded\tterminating\tsimple\ttnont\thard\tfpcalls\tfpargs\tcfgblocks\tmaxindeg\tsloopytime\tsloopylooptime\tsloopycfgtime\tsloopyparsing\n";
    return 0;
  }

  // setup clang tool
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  MatchFinder Finder;

  FunctionCallback FC;
  CFGCallback CFGFC;
  Finder.addMatcher(FunctionMatcher, &FC);
  Finder.addMatcher(FunctionMatcher, &CFGFC);

  auto CallMatcher = callExpr().bind(FunctionName);
  FPCallback FPC;
  Finder.addMatcher(CallMatcher, &FPC);

  // run
  long Begin = now();
  unsigned ret = Tool.run(newFrontendActionFactory(&Finder));
  /* we continue even if sloopy failed on some file */

  // print statistics
  if (LoopStats) {
    DEBUG_WITH_TYPE("progress", llvm::dbgs() << "Preparing statistics...\n");
#ifndef NDEBUG
    if (!::llvm::DebugFlag) {
      llvm::errs() << ".";
    }
#endif
    std::string Filename(BenchName+".json");
    std::string ErrorInfo;
    raw_fd_ostream ostream(Filename.c_str(), ErrorInfo);
    dumpClasses(ostream, OutputFormat::JSON);


    ostream.close();
  }

  if (MachineLearning) {
    size_t NumLoops = Classifications.size();
    std::map<std::string,unsigned> ClassCounts = {
      { "FinitePaths", 0 },
      { "Proved", 0 },
      { "AnyExitWeakCfWellformed", 0 },
      { "TriviallyNonterminating", 0 },
      { "ANY", 0 },
      { "Time", 0 }
    };
    for (ClassificationMap::const_iterator I = Classifications.begin(),
                                           E = Classifications.end();
                                           I != E; I++) {
      auto PropertyMap = I->second;
      for (std::map<std::string,unsigned>::iterator it=ClassCounts.begin(); it!=ClassCounts.end(); ++it) {
        std::string CurrentProperty = it->first;
        if(int CurrentValue = boost::get<int>(PropertyMap[CurrentProperty])) {
          ClassCounts[CurrentProperty] += CurrentValue;
        }
      }
    }

    long End = now();
    std::cout <<
      BenchName                                                                                   << "\t" <<
      percentage(ClassCounts["FinitePaths"], NumLoops)                                            << "\t" <<
      percentage(ClassCounts["Proved"], NumLoops)                                                 << "\t" <<
      percentage(ClassCounts["AnyExitWeakCfWellformed"], NumLoops)                                << "\t" <<
      percentage(ClassCounts["TriviallyNonterminating"], NumLoops)                                << "\t" <<
      (NumLoops == 0 ? 0 : (100. - percentage(ClassCounts["AnyExitWeakCfWellformed"], NumLoops))) << "\t" <<
      percentage(FPC.fp_calls, FPC.calls)                                                         << "\t" <<
      percentage(FPC.fp_args, FPC.args)                                                           << "\t" <<
      CFGFC.size                                                                                  << "\t" <<
      CFGFC.max_fan_in                                                                            << "\t" <<
      (End-Begin)                                                                                 << "\t" <<
      FC.time                                                                                     << "\t" <<
      (FPC.time + CFGFC.time)                                                                     << "\t" <<
      (End-Begin-FC.time-FPC.time-CFGFC.time)                                                     << "\n";
  }

  return ret;
}
