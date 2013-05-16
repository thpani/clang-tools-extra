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

#include "Loop.h"
#include "LoopMatchers.h"
#include "LoopClassifier.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

static cl::extrahelp MoreHelp(
    "\n"
    "To run loop-classify on all files in a subtree of the source tree, use:\n"
    "\n"
    "  find path/in/subtree -name '*.c' -o -name '*.cc' -o -name '*.cpp' | \\\n"
    "    xargs loop-classify\n"
);

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();

  /* ----- Parse options ----- */

  // This causes options to be parsed.
  CommonOptionsParser OptionsParser(argc, argv);

  RefactoringTool Tool(OptionsParser.getCompilations(),
                       OptionsParser.getSourcePathList());
  MatchFinder Finder;

  /* ----- Match loops ----- */
  LoopClassifier AnyLoopCounter("ANY");
  LoopClassifier DoStmtCounter("DO");
  LoopClassifier ForStmtCounter("FOR");
  LoopClassifier ForRangeStmtCounter("FOR-RANGE");
  LoopClassifier WhileStmtCounter("WHILE");
  Finder.addMatcher(AnyLoopMatcher, &AnyLoopCounter);
  Finder.addMatcher(DoStmtMatcher, &DoStmtCounter);
  Finder.addMatcher(ForStmtMatcher, &ForStmtCounter);
  Finder.addMatcher(ForRangeStmtMatcher, &ForRangeStmtCounter);
  Finder.addMatcher(WhileStmtMatcher, &WhileStmtCounter);

  EmptyBodyClassifier emptyBodyClassifier;
  Finder.addMatcher(AnyLoopMatcher, &emptyBodyClassifier);

  BranchingClassifier branchingClassifier;
  Finder.addMatcher(AnyLoopMatcher, &branchingClassifier);

  ConditionClassifier ConditionClassifier;
  Finder.addMatcher(AnyLoopMatcher, &ConditionClassifier);

  AdaForLoopClassifier AdaForLoopClassifier;
  Finder.addMatcher(AnyLoopMatcher, &AdaForLoopClassifier);

  DataIterClassifier DataIterClassifier;
  Finder.addMatcher(AnyLoopMatcher, &DataIterClassifier);

  ArrayIterClassifier ArrayIterClassifier;
  Finder.addMatcher(AnyLoopMatcher, &ArrayIterClassifier);

  if (Tool.run(newFrontendActionFactory(&Finder)) != 0) {
    return 1;
  }

  /* ----- Print stats ----- */
  if (LoopStats) {
    std::cout.setf( std::ios::fixed, std:: ios::floatfield ); // floatfield set to fixed
    std::cout.precision(1);
    std::cout << "==============" << "\n";
    std::cout << "= STATISTICS =\n";
    std::cout << "==============" << "\n";
    std::ofstream myfile;
    myfile.open ("unclassified.txt", std::ofstream::app);
    std::map<std::string, int> Counter;
    for (auto Classification : Classifications) {
      const Stmt *Stmt = Classification.first;
      auto ClassList = Classification.second;

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
           Class.find("Branch-") != std::string::npos) {
          negClasses++;
        }
      }
      if (ClassList.size() < (unsigned long) 3+negClasses) { // ANY, TYPE
        Counter["UNCLASSIFIED"]++;
        myfile << Loops.find(Stmt)->second.Dump() << "\n";
        myfile << "----------------------------------------------------------------------------------------------------\n";
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

  if (PerLoopStats) {
    std::ofstream myfile;
    myfile.open ("loops.sql");
    myfile << LoopInfo::DumpSQLCreate();
    myfile << "INSERT INTO loops VALUES\n";
    std::stringstream sstm;
    for (auto Loop : Loops) {
      sstm << Loop.second.DumpSQL() << ",\n";
    }
    std::string values = sstm.str();
    values = values.substr(0, values.size()-2);
    myfile << values << ";";
    myfile.close();
    /* std::cout << "==============" << "\n"; */
    /* std::cout << "= STATISTICS =\n"; */
    /* std::cout << "==============" << "\n"; */
    /* std::map<llvm::FoldingSetNodeID, std::vector<std::string> >::iterator it; */
    /* for (it = Classifications.begin(); it != Classifications.end(); ++it) { */
    /*   std::cout << it->first.ComputeHash() << ": "; */
    /*   for (std::vector<std::string>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) { */
    /*     std::cout << *it2 << ", "; */
    /*   } */
    /*   std::cout << "\n"; */
    /* } */
  }

  return 0;
}
