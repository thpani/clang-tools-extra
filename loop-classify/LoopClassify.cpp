#include "iostream"
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
static cl::opt<bool> LoopStats("loop-stats");
static cl::opt<bool> PerLoopStats("per-loop-stats");
static cl::opt<bool> Annotate("annotate");

static int output(RefactoringTool &Tool) {
  // Syntax check passed, write results to file.
  LangOptions DefaultLangOptions;
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter DiagnosticPrinter(llvm::errs(), &*DiagOpts);
  DiagnosticsEngine Diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()),
      &*DiagOpts, &DiagnosticPrinter, false);
  SourceManager Sources(Diagnostics, Tool.getFiles());
  Rewriter Rewrite(Sources, DefaultLangOptions);

  if (!tooling::applyAllReplacements(Tool.getReplacements(), Rewrite)) {
    llvm::errs() << "Skipped some replacements.\n";
  }

  for (Rewriter::buffer_iterator I = Rewrite.buffer_begin(),
                                 E = Rewrite.buffer_end();
       I != E; ++I) {
    // FIXME: This code is copied from the FixItRewriter.cpp - I think it should
    // go into directly into Rewriter (there we also have the Diagnostics to
    // handle the error cases better).
    const FileEntry *Entry =
        Rewrite.getSourceMgr().getFileEntryForID(I->first);
    std::string ErrorInfo;
    std::string FileName = std::string(Entry->getName())+".out.c";
    llvm::raw_fd_ostream FileStream(
        FileName.c_str(), ErrorInfo, llvm::raw_fd_ostream::F_Binary);
    if (!ErrorInfo.empty())
      return 1;
    I->second.write(FileStream);
    FileStream.flush();
  }

  return 0;
}

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();

  /* ----- Parse options ----- */

  // This causes options to be parsed.
  CommonOptionsParser OptionsParser(argc, argv);

  RefactoringTool Tool(OptionsParser.getCompilations(),
                       OptionsParser.getSourcePathList());
  MatchFinder Finder;

  /* ----- Match loops ----- */
  LoopCounter AnyLoopCounter("ANY");
  LoopCounter DoStmtCounter("DO");
  LoopCounter ForStmtCounter("FOR");
  LoopCounter ForRangeStmtCounter("FOR-RANGE");
  LoopCounter WhileStmtCounter("WHILE");
  Finder.addMatcher(AnyLoopMatcher, &AnyLoopCounter);
  Finder.addMatcher(DoStmtMatcher, &DoStmtCounter);
  Finder.addMatcher(ForStmtMatcher, &ForStmtCounter);
  Finder.addMatcher(ForRangeStmtMatcher, &ForRangeStmtCounter);
  Finder.addMatcher(WhileStmtMatcher, &WhileStmtCounter);

  EmptyBodyClassifier emptyBodyClassifier;
  Finder.addMatcher(AnyLoopMatcher, &emptyBodyClassifier);

  ForLoopClassifier AdaForLoopClassifier;
  Finder.addMatcher(ForStmtMatcher, &AdaForLoopClassifier);

  WhileLoopClassifier WhileLoopClassifier;
  Finder.addMatcher(WhileStmtMatcher, &WhileLoopClassifier);

  if (Tool.run(newFrontendActionFactory(&Finder)) != 0) {
    return 1;
  }

  /* ----- Write annotated output ----- */
  if (Annotate) {
    if (output(Tool) != 0) {
      return 1;
    }
  }

  /* ----- Print stats ----- */
  if (LoopStats) {
    std::cout.setf( std::ios::fixed, std:: ios::floatfield ); // floatfield set to fixed
    std::cout.precision(1);
    std::cout << "==============" << "\n";
    std::cout << "= STATISTICS =\n";
    std::cout << "==============" << "\n";
    std::map<std::string, int> Counter;
    std::map<llvm::FoldingSetNodeID, std::vector<std::string> >::iterator it;
    for (it = Classifications.begin(); it != Classifications.end(); ++it) {
      if (it->second.size() < 2) { // ANY, TYPE
        Counter["UNCLASSIFIED"]++;
      }
      for (std::vector<std::string>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
        Counter[*it2]++;
      }
    }
    for (std::map<std::string, int>::iterator it = Counter.begin();
         it != Counter.end();
         ++it) {
      std::cout << it->first << "\t" << it->second;
      std::cout << "\t" << (100.*it->second/Counter["ANY"]) << "%";
      size_t pos = it->first.find("-");
      if (pos != std::string::npos) {
        const std::string family = it->first.substr(0, pos);
        std::cout << "\t" << (100.*it->second/Counter[family]) << "%";
      }
      std::cout << std::endl;
    }
  }

  if (PerLoopStats) {
    std::cout << "==============" << "\n";
    std::cout << "= STATISTICS =\n";
    std::cout << "==============" << "\n";
    std::map<llvm::FoldingSetNodeID, std::vector<std::string> >::iterator it;
    for (it = Classifications.begin(); it != Classifications.end(); ++it) {
      std::cout << it->first.ComputeHash() << ": ";
      for (std::vector<std::string>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
        std::cout << *it2 << ", ";
      }
      std::cout << "\n";
    }
  }

  return 0;
}
