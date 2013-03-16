#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_


#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

const char LoopName[] = "forLoop";
const char IncrementVarName[] = "incrementVar";
const char InitVarName[] = "initVar";
const char EndCallName[] = "endCall";
const char ConditionEndVarName[] = "conditionEndVar";
const char EndVarName[] = "endVar";
const char ConditionOperatorName[] = "conditionOp";
const char IncrementOperatorName[] = "incrementOp";
const char InitDeclStmt[] = "initDeclStmt";

std::map<llvm::FoldingSetNodeID, std::vector<std::string> > Classifications;

// Returns the text that makes up 'node' in the source.
// Returns an empty string if the text cannot be found.
template <typename T>
static std::string getText(const SourceManager &SourceManager, const T &Node) {
  clang::SourceLocation StartSpellingLocatino =
      SourceManager.getSpellingLoc(Node.getLocStart());
  clang::SourceLocation EndSpellingLocation =
      SourceManager.getSpellingLoc(Node.getLocEnd());
  if (!StartSpellingLocatino.isValid() || !EndSpellingLocation.isValid()) {
    return std::string();
  }
  bool Invalid = true;
  const char *Text =
      SourceManager.getCharacterData(StartSpellingLocatino, &Invalid);
  if (Invalid) {
    return std::string();
  }
  std::pair<clang::FileID, unsigned> Start =
      SourceManager.getDecomposedLoc(StartSpellingLocatino);
  std::pair<clang::FileID, unsigned> End =
      SourceManager.getDecomposedLoc(clang::Lexer::getLocForEndOfToken(
          EndSpellingLocation, 0, SourceManager, clang::LangOptions()));
  if (Start.first != End.first) {
    // Start and end are in different files.
    return std::string();
  }
  if (End.second < Start.second) {
    // Shuffling text with macros may cause this.
    return std::string();
  }
  return std::string(Text, End.second - Start.second);
}

static bool areSameVariable(const ValueDecl *First, const ValueDecl *Second) {
  return First && Second &&
         First->getCanonicalDecl() == Second->getCanonicalDecl();
}

class LoopCounter : public MatchFinder::MatchCallback {
  public:
    LoopCounter(const std::string &Marker) : Marker(Marker) {}

    virtual void run(const MatchFinder::MatchResult &Result, const std::string Marker) {
      const Stmt *Stmt = Result.Nodes.getNodeAs<clang::Stmt>(LoopName);

      /* Classification */
      llvm::FoldingSetNodeID Id;
      Stmt->Profile(Id, *Result.Context, true);
      Classifications[Id].push_back(Marker);
    }

    virtual void run(const MatchFinder::MatchResult &Result) {
      run(Result, Marker);
    }

  protected:
    const std::string Marker;
};

class LoopClassifier : public LoopCounter {
 public:
  LoopClassifier(const std::string &Marker, Replacements *Replace) : LoopCounter(Marker), Replace(Replace) {}

  virtual void run(const MatchFinder::MatchResult &Result, const std::string Marker) {
    LoopCounter::run(Result, Marker);

    const Stmt *Stmt = Result.Nodes.getNodeAs<clang::Stmt>(LoopName);

    /* Replacement */
    const std::string ClassComment = "/* LOOPCLASS: "+Marker+"*/ ";
    const std::string Comment = ClassComment + getText(*Result.SourceManager, *Stmt);
    if (ClassComment.size() == Comment.size()) {
      // TODO
      llvm::errs() << Marker << " Skipped some replacements (empty statment text from getText).\n";

      Stmt->dumpPretty(*Result.Context);
      return;
    }
    Replace->insert(Replacement(*Result.SourceManager, Stmt, Comment));
  }

  virtual void run(const MatchFinder::MatchResult &Result) {
    run(Result, Marker);
  }

 private:
  Replacements *Replace;
};

class VariableAssignmentASTVisitor : public RecursiveASTVisitor<VariableAssignmentASTVisitor> {
  public:
    VariableAssignmentASTVisitor(const VarDecl *LoopVar) : AssignedTo(false), LoopVar(LoopVar) {};

    bool findUsages(const Stmt *Body) {
      this->Body = Body;
      TraverseStmt(const_cast<Stmt *>(Body));
      return AssignedTo;
    }

    bool VisitDeclRefExpr(DeclRefExpr *DRE) {
      const ParentMap Map(const_cast<Stmt *>(Body));
      const Stmt *Parent = Map.getParent(DRE);

      /* Visiting uninteresting variable reference. */
      if (!areSameVariable(LoopVar, DRE->getDecl())) {
        return true;
      }

      /* LoopVar++ or LoopVar-- */
      if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Parent)) {
        if (UOP->isIncrementDecrementOp()) {
          AssignedTo = true;
        }
      }

      /* (Compound) assignment.
       * Ensure we came here for the LHS, i.e. LoopVar = <EXPR>;
       */
      if (const BinaryOperator *BOP = dyn_cast<BinaryOperator>(Parent))
        if (BOP->isAssignmentOp())
          if (const DeclRefExpr *LHS = dyn_cast<DeclRefExpr>(BOP->getLHS()))
            if (areSameVariable(LoopVar, LHS->getDecl()))
            {
              AssignedTo = true;
            }

      return true;
    }

  private:
    bool AssignedTo;
    const VarDecl *LoopVar;
    const Stmt *Body;
};

class EmptyBodyClassifier : public LoopClassifier {
  public:
    EmptyBodyClassifier(Replacements *Replace) : LoopClassifier("EMPTY-BODY", Replace) {}

    void run(const MatchFinder::MatchResult &Result) {
      const Stmt *LSS = Result.Nodes.getNodeAs<Stmt>(LoopName);

      const Stmt *Body;
      if (const DoStmt *LS = dyn_cast<DoStmt>(LSS)) {
        Body = LS->getBody();
      } else if (const ForStmt *LS = dyn_cast<ForStmt>(LSS)) {
        Body = LS->getBody();
      } else if (const CXXForRangeStmt *LS = dyn_cast<CXXForRangeStmt>(LSS)) {
        Body = LS->getBody();
      } else if (const WhileStmt *LS = dyn_cast<WhileStmt>(LSS)) {
        Body = LS->getBody();
      }
      assert(Body!=NULL);

      if (isa<NullStmt>(Body) ||
          (isa<CompoundStmt>(Body) && cast<CompoundStmt>(Body)->size() == 0)) {
          LoopClassifier::run(Result);
      }
    }
};

/* TODO: not sound wrt impure function calls, aliasses */
class ForLoopClassifier : public LoopClassifier {
 public:
  ForLoopClassifier(Replacements *Replace) : LoopClassifier("FOR-ADA", Replace) {}

  void run(const MatchFinder::MatchResult &Result) {
    const ForStmt *FS = Result.Nodes.getNodeAs<clang::ForStmt>(LoopName);
    const DeclStmt *InitDecl = Result.Nodes.getNodeAs<DeclStmt>(InitDeclStmt);
    const VarDecl *LoopVar = Result.Nodes.getNodeAs<VarDecl>(IncrementVarName);
    const Stmt *Body = FS->getBody();
    const BinaryOperator *ConditionOp = Result.Nodes.getNodeAs<BinaryOperator>(ConditionOperatorName);
    const UnaryOperator *IncrementOp = Result.Nodes.getNodeAs<UnaryOperator>(IncrementOperatorName);

    /* loop init
     * =========
     *
     * loop init is a single integer declaration
     */
    if (!InitDecl->isSingleDecl()) {
      LoopClassifier::run(Result, "FOR-MultiInitDecl)");
      return;
    }
    const VarDecl *InitVar;
    if (!(InitVar = dyn_cast<VarDecl>(InitDecl->getSingleDecl()))) {
      return;
    }
    if (!InitVar->getType()->isIntegerType()) {
      return;
    }

    /* loop condition & increment
     * ==========================
     * 
     * loop condition is one of the following combinations:
     * (i * N; i+) and (N * i; i+) where (*; +) are
     *   < ; ++
     *   <=; ++
     *   > ; --
     *   >=; --
     */

    const BinaryOperatorKind ConditionOpCode = ConditionOp->getOpcode();
    if ((ConditionOpCode == BO_LT || ConditionOpCode == BO_LE) && !IncrementOp->isIncrementOp())
      return;
    if ((ConditionOpCode == BO_GT || ConditionOpCode == BO_GE) && !IncrementOp->isDecrementOp())
      return;

    // LHS is an integer variable
    const VarDecl *CondVar;
    const Expr *ConditionLHS = ConditionOp->getLHS()->IgnoreParenImpCasts();
    if (const DeclRefExpr *CondVarDRE = dyn_cast<DeclRefExpr>(ConditionLHS)) {
      if (!CondVarDRE->getType()->isIntegerType()) {
        return;
      }
      if (!(CondVar = dyn_cast<VarDecl>(CondVarDRE->getDecl()))) {
        // TODO could be something more complex
        return;
      }
    }
    else {
      return;
    }

    // RHS is either an ...
    const Expr *ConditionRHS = ConditionOp->getRHS()->IgnoreParenImpCasts();
    if (ConditionRHS->isIntegerConstantExpr(*Result.Context)) {
      // ... integer literal
      ; // we're done
    }
    else if (const DeclRefExpr *ConditionRHSDeclRefExpr = dyn_cast<DeclRefExpr>(ConditionRHS)) {
      // ... integer variable
      if (!ConditionRHSDeclRefExpr->getType()->isIntegerType()) {
        return;
      }
      // check the variable isn't assigned in the loop body
      if (const VarDecl *ConditionRHSVar = dyn_cast<VarDecl>(ConditionRHSDeclRefExpr->getDecl())) {
        VariableAssignmentASTVisitor Finder(ConditionRHSVar);
        if (Finder.findUsages(FS->getBody())) {
          LoopClassifier::run(Result, "FOR-(!ADA > N-ASSIGNED)");
          return;
        }
      }
      else {
        assert(false); // this is not a variable reference
        return;
      }
    }
    else {
      // TODO could be something more complex (member expression, array subscript, ...)
    }

    /* Ensure InitVar = CondVar = IncVar */
    if (!areSameVariable(InitVar, CondVar) || !areSameVariable(InitVar, LoopVar))
      return;

    /* Ensure the loop variable is never assigned in the loop body */
    VariableAssignmentASTVisitor Finder(LoopVar);
    if (Finder.findUsages(FS->getBody())) {
      LoopClassifier::run(Result, "FOR-(!ADA > i-ASSIGNED)");
      return;
    }

    LoopClassifier::run(Result);
  }
};

/* FS->viewAST(); // dotty */
/* FS->dumpColor(); // AST */
/* FS->dumpPretty(*Result.Context); // C source */

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_
