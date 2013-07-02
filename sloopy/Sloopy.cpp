#include "iostream"
#include "fstream"
#include "algorithm"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Driver/OptTable.h"
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

#include "CFGBuilder.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

using namespace sloopy;

llvm::cl::opt<std::string> BenchName("bench-name");

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
    std::cout.setf( std::ios::fixed, std:: ios::floatfield ); // floatfield set to fixed
    std::cout.precision(1);
    std::cout << "==============" << "\n";
    std::cout << "= STATISTICS =\n";
    std::cout << "==============" << "\n";
    std::ofstream myfile;
    myfile.open ("classifications_"+BenchName+".txt");
    std::map<std::string, int> Counter;
    for (auto Classification : Classifications) {
      auto Loop = Classification.first;
      auto ClassList = Classification.second;

      myfile << LoopLocationMap[Loop] << " ";

      int negClasses = 0;
      for (auto Class : ClassList) {
        Counter[Class]++;
        size_t pos = Class.find("@");
        if (pos != std::string::npos) {
          Counter[Class.substr(pos+1, std::string::npos)]++;
        }
        if(Class.find("!") != std::string::npos ||
           Class.find("?") != std::string::npos ||
           Class.find("Cond-") != std::string::npos ||
           Class.find("Branch-") != std::string::npos ||
           Class.find("ControlVars-") != std::string::npos ||
           Class.find("SingleExit") != std::string::npos) {
          negClasses++;
        }
        myfile << Class << " ";
      }

      myfile << "\n";

      if (ClassList.size() < (unsigned long) 3+negClasses) { // ANY, TYPE
        Counter["UNCLASSIFIED"]++;
      }
    }
    myfile.close();
    for (auto CounterItem : Counter) {
      auto Class = CounterItem.first;
      auto Count = CounterItem.second;
      std::cout << Class << "\t" << Count;
      std::cout << "\t" << (100.*Count/Counter["ANY"]) << "%";
      size_t pos = Class.find("@");
      const std::string family = pos != std::string::npos ? Class.substr(0, pos) : "ANY";
      std::cout << "\t" << (100.*Count/Counter[family]) << "%";
      std::cout << std::endl;
    }
  }

  return 0;
}
