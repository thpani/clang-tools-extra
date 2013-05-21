#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFIER_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFIER_H_

#include <sstream>
#include <algorithm>
#include <vector>
#include "clang/AST/ASTContext.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/AnalysisContext.h"
#include "PseudoConstantAnalysis.h"
#include "Loop.h"

using namespace clang;
using namespace clang::tooling;

llvm::cl::opt<bool> LoopStats("loop-stats");
llvm::cl::opt<bool> PerLoopStats("per-loop-stats");
llvm::cl::opt<bool> CondClass("cond");
llvm::cl::opt<bool> BranchClass("branch");

struct IncrementInfo {
  const VarDecl *VD;
  const Stmt *Statement;
  const VarDecl *Delta;
};

typedef struct {
  const CFGBlock* Header;
  const std::set<const CFGBlock*> Blocks;
} NaturalLoop;

class checkerror {
  public:
    checkerror(const std::string &reason) : reason(reason) {}
    const std::string what() const throw() {
      return reason;
    }
  private:
    const std::string reason;
};

static bool isIntegerConstant(const Expr *Expression, const ASTContext *Context) {
  llvm::APSInt Result;
  if (Expression->IgnoreParenImpCasts()->isIntegerConstantExpr(Result, const_cast<ASTContext&>(*Context))) {
    return true;
  }
  return false;
}

static llvm::APInt getIntegerConstant(const Expr *Expression, const ASTContext *Context) {
  llvm::APSInt Result;
  if (Expression->IgnoreParenImpCasts()->isIntegerConstantExpr(Result, const_cast<ASTContext&>(*Context))) {
    return Result;
  }
  llvm_unreachable("no integer constant");
}

static const VarDecl *getVariable(const Expr *Expression) {
  if (Expression==NULL) return NULL;
  const Expr *AdjustedExpr = Expression->IgnoreParenImpCasts();
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(AdjustedExpr)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(DRE->getDecl())) {
      return Var;
    }
  }
  return NULL;
}

static const VarDecl *getIntegerVariable(const Expr *Expression) {
  const VarDecl *Var = getVariable(Expression);
  if (Var != NULL && Var->getType()->isIntegerType()) {
    return Var;
  }
  return NULL;
}

static const ValueDecl *getIntegerField(const Expr *Expression) {
  if (Expression==NULL) return NULL;
  if (const VarDecl *VD = getIntegerVariable(Expression)) {
    return VD;
  }
  if (const MemberExpr *ME = dyn_cast<MemberExpr>(Expression->IgnoreParenImpCasts())) {
    if (const FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      if (FD->getType()->isIntegerType()) {
        return FD;
      }
    }
  }
  return NULL;
}

static const Stmt *getLoopBody(const Stmt *LSS) {
  const Stmt *Body;
  if (const DoStmt *LS = dyn_cast<DoStmt>(LSS)) {
    Body = LS->getBody();
  } else if (const ForStmt *LS = dyn_cast<ForStmt>(LSS)) {
    Body = LS->getBody();
  } else if (const CXXForRangeStmt *LS = dyn_cast<CXXForRangeStmt>(LSS)) {
    Body = LS->getBody();
  } else if (const WhileStmt *LS = dyn_cast<WhileStmt>(LSS)) {
    Body = LS->getBody();
  } else {
    Body = NULL;
  }
  return Body;
}
static const Expr *getLoopCond(const Stmt *LSS) {
  const Expr *Cond;
  if (const DoStmt *LS = dyn_cast<DoStmt>(LSS)) {
    Cond = LS->getCond();
  } else if (const ForStmt *LS = dyn_cast<ForStmt>(LSS)) {
    Cond = LS->getCond();
  } else if (const CXXForRangeStmt *LS = dyn_cast<CXXForRangeStmt>(LSS)) {
    Cond = LS->getCond();
  } else if (const WhileStmt *LS = dyn_cast<WhileStmt>(LSS)) {
    Cond = LS->getCond();
  } else {
    Cond = NULL;
  }
  if (Cond==NULL) return Cond;
  return Cond->IgnoreParenImpCasts();
}

// TODO use CFG
static const std::vector<const Stmt*> getLoopBlocks(const Stmt *LSS) {
  std::vector<const Stmt*> result;
  if (const Stmt *Body = getLoopBody(LSS))
    result.push_back(Body);
  if (const Stmt *Cond = getLoopCond(LSS))
    result.push_back(Cond);
  if (const ForStmt *LS = dyn_cast<ForStmt>(LSS)) {
    if (LS->getInc()) {
      result.push_back(LS->getInc());
    }
  }
  return result;
}

static const std::string getStmtMarker(const Stmt *LS) {
  switch(LS->getStmtClass()) {
    case Stmt::WhileStmtClass:
      return "WHILE@";
    case Stmt::DoStmtClass:
      return "DO@";
    case Stmt::ForStmtClass:
      return "FOR@";
    case Stmt::CXXForRangeStmtClass:
      return "RANGE@";
    default:
      llvm_unreachable("Unknown loop statement class");
      return "UNKNOWN@";
  }
}
class LoopClassifier : public MatchFinder::MatchCallback {
  private:
    void dfscb(std::set<const CFGBlock*> &visited, const CFGBlock *block) const {
      if (!visited.insert(block).second)
        return;
      for (CFGBlock::const_pred_iterator it = block->pred_begin(); it != block->pred_end(); it++) {
        dfscb(visited, *it);
      }
    }
    AnalysisDeclContext *getADC(Stmt *LS, ASTContext *Context) const {
      Stmt *Parent;
      for (Stmt *S = LS;
          S;
          S = const_cast<Stmt*>(Context->getParents(*Parent).front().get<Stmt>())) {
        Parent = S;
      }
      const FunctionDecl *D = Context->getParents(*Parent).front().get<FunctionDecl>();
      AnalysisDeclContextManager mgr;
      AnalysisDeclContext *AC = mgr.getContext(D);
      return AC;
    }

    CFG *getCFG(Stmt *LS, ASTContext *Context) const {
      CFG *C = getADC(LS, Context)->getCFG();
      C->viewCFG(LangOptions());
      return C;
    }

    DominatorTree getDom(Stmt *LS, ASTContext *Context) const {
      AnalysisDeclContext *AC = getADC(LS, Context);
      DominatorTree dom;
      dom.buildDominatorTree(*AC);
      return dom;
    }

  protected:
    const NaturalLoop getLoop(const Stmt *LS, const ASTContext *Context) const {
      CFG *CFG = getCFG(const_cast<Stmt*>(LS), const_cast<ASTContext*>(Context));
      DominatorTree Dom = getDom(const_cast<Stmt*>(LS), const_cast<ASTContext*>(Context));
      for (CFG::const_iterator it = CFG->begin(); it != CFG->end(); it++) {
        const CFGBlock *block = *it;
        for (CFGBlock::const_succ_iterator it2 = block->succ_begin(); it2 != block->succ_end(); it2++) {
          const CFGBlock *Header = *it2;
          if (Dom.dominates(Header, block) && Header->getTerminator() == LS) {
            std::set<const CFGBlock*> visited = { Header };
            dfscb(visited, block); // DFS on reverse CFG

            /* std::cout << "natural loop for back edge (" */
            /*           << block->getBlockID() << ", " << Header->getBlockID() */
            /*           << "): {"; */
            /* for (auto v : visited) { */
            /*   std::cout << v->getBlockID() << ", "; */
            /* } */
            /* std::cout << "}" << std::endl; */

            /* CFG->dump(LangOptions(), true); */
            /* CFG->viewCFG(LangOptions()); */
            return { Header, visited };
          }
        }
      }
      const std::set<const CFGBlock*> Empty;
      return { NULL, Empty };
    }

    void classify(const MatchFinder::MatchResult &Result, const std::string Marker) {
      const Stmt *LS = Result.Nodes.getNodeAs<clang::Stmt>(LoopName);

      Classifications[LS].push_back(Marker);
      Loops.insert(std::pair<const Stmt*, LoopInfo>(LS, LoopInfo(LS, Result.Context, Result.SourceManager)));
    }

  public:
    LoopClassifier() : Marker("") {}
    LoopClassifier(const std::string &Marker) : Marker(Marker) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
      classify(Result, Marker);
    }

  protected:
    std::string Marker;
};

#include "Classifiers/Ada.h"
#include "Classifiers/Branch.h"
#include "Classifiers/EmptyBody.h"
#include "Classifiers/Cond.h"
#include "Classifiers/DataIter.h"
#include "Classifiers/ArrayIter.h"

/* FS->viewAST(); // dotty */
/* FS->dumpColor(); // AST */
/* FS->dumpPretty(*Result.Context); // C source */

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFIER_H_
