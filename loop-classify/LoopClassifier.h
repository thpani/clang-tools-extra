#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_

#include <sstream>
#include "clang/AST/ASTContext.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

const char LoopName[] = "forLoop";

std::map<llvm::FoldingSetNodeID, std::vector<std::string> > Classifications;

static bool areSameVariable(const ValueDecl *First, const ValueDecl *Second) {
  return First != NULL && Second != NULL &&
         First->getCanonicalDecl() == Second->getCanonicalDecl();
}

static const IntegerLiteral *getIntegerLiteral(const Expr *Expression, const ASTContext &Context) {
  return dyn_cast<IntegerLiteral>(Expression->IgnoreParenImpCasts());
}

static bool isIntegerLiteral(const Expr *Expression, const ASTContext &Context) {
  return getIntegerLiteral(Expression, Context) != NULL;
}

static const VarDecl *getVariable(const Expr *Expression) {
  const Expr *AdjustedExpr = Expression->IgnoreParenImpCasts();
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(AdjustedExpr)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(DRE->getDecl())) {
      return Var;
    }
  }
  return NULL;
}

static const VarDecl *getIntegerVariable(Expr *Expression) {
  const VarDecl *Var = getVariable(Expression);
  if (Var != NULL && Var->getType()->isIntegerType()) {
    return Var;
  }
  return NULL;
}

class AdaChecker {
  public:
    bool checkAdaCond(const Expr *Expression, const VarDecl *LoopVar, const ASTContext &Context) {
      /* check the condition is a binary operator
       * where either LHS or RHS are integer lieterals
       * or one is an integer literal
       */
      if (!(ConditionOp = dyn_cast<BinaryOperator>(Expression))) {
        Reason = "(!ADA_Cond_No_BinaryOperator)";
        return false;
      }
      const VarDecl *LHS = getIntegerVariable(ConditionOp->getLHS());
      const VarDecl *RHS = getIntegerVariable(ConditionOp->getRHS());
      if (LHS != NULL && RHS != NULL) {
        // LoopVar and BoundVar
        // check which one is the loop var (i.e. equal to the initialization variable)
        if (areSameVariable(LoopVar, LHS)) {
          LoopVarLHS = true;
        }
        else if (areSameVariable(LoopVar, RHS)) {
          LoopVarLHS = false;
        }
        else {
          Reason = "(!ADA_Cond_LoopVar_NotIn_BinaryOperator)";
          return false;
        }
      }
      else if (LHS != NULL && isIntegerLiteral(ConditionOp->getRHS(), Context)) {
        // LHS is loop var, RHS is integer bound
        LoopVarLHS = true;
      }
      else if (RHS != NULL && isIntegerLiteral(ConditionOp->getLHS(), Context)) {
        // RHS is loop var, LHS is integer bound
        LoopVarLHS = false;
      }
      else {
        // TODO could be something more complex, i.e. array subscript, member expr
        /* ConditionOp->getLHS()->dumpPretty(*Result.Context); */
        /* ConditionOp->getRHS()->dumpPretty(*Result.Context); */
        Reason = "(!ADA_Cond_Bound_TooComplex)";
        return false;
      }
      CondVar = LoopVarLHS ? LHS : RHS;
      BoundVar = LoopVarLHS ? RHS : LHS;

      return true;
    }
    const std::string getReason() {
      return Reason;
    }
    const VarDecl *getCondVar() {
      return CondVar;
    }
    const VarDecl *getBoundVar() {
      return BoundVar;
    }
    const BinaryOperator *getConditionOp() {
      return ConditionOp;
    }
    bool getLoopVarLHS() {
      return LoopVarLHS;
    }
  private:
    const BinaryOperator *ConditionOp;
    const VarDecl *CondVar;
    const VarDecl *BoundVar;
    bool LoopVarLHS;
    std::string Reason;
};

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
    
#if 0
    Rewrite.setSourceMgr(Result.Context->getSourceManager(),
                         Result.Context->getLangOpts());

    const Stmt *Stmt = Result.Nodes.getNodeAs<clang::Stmt>(LoopName);

    /* Replacement */
    const std::string ClassComment = "/* LOOPCLASS: "+Marker+"*/ ";
    const std::string Comment = ClassComment + Rewrite.ConvertToString(const_cast<class Stmt*>(Stmt));
    if (ClassComment.size() == Comment.size()) {
      // TODO
      llvm::errs() << Marker << " Skipped some replacements (empty statment text from getText).\n";

      Stmt->dumpPretty(*Result.Context);
      return;
    }
    Replace->insert(Replacement(*Result.SourceManager, Stmt, Comment));
#endif
  }

  virtual void run(const MatchFinder::MatchResult &Result) {
    run(Result, Marker);
  }

 private:
  Replacements *Replace;
  Rewriter Rewrite;
};


class EmptyBodyClassifier : public LoopClassifier {
  public:
    EmptyBodyClassifier(Replacements *Replace) : LoopClassifier("ANY-EMPTYBODY", Replace) {}

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

class WhileLoopClassifier : public LoopClassifier {
 public:
  WhileLoopClassifier(Replacements *Replace) : LoopClassifier("WHILE-TRUE", Replace) {}

  void run(const MatchFinder::MatchResult &Result) {
    const WhileStmt *WS = Result.Nodes.getNodeAs<clang::WhileStmt>(LoopName);
    const Expr *Cond = WS->getCond()->IgnoreParenImpCasts();
    std::string suffix;

    /* while(1) */
    if (const IntegerLiteral *Lit = getIntegerLiteral(Cond, *Result.Context)) {
      if (Lit->getValue().getBoolValue()) {;}
      else { suffix+="-FALSE"; }
      LoopClassifier::run(Result, Marker+suffix);
      return;
    }

    /* while (++i) */
    if (const UnaryOperator *ConditionOp = dyn_cast<UnaryOperator>(Cond)) {
      if (ConditionOp->isIncrementDecrementOp()) {
        LoopClassifier::run(Result, "WHILE-Unary-IncDec");
        return;
      }
      LoopClassifier::run(Result, "WHILE-Unary-Other");
      return;
    }

    /* while(i<N) */
    if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(Cond)) {
      if (ConditionOp->isEqualityOp()) {
        LoopClassifier::run(Result, "WHILE-Binary-Equality");
        return;
      }
      if (ConditionOp->isRelationalOp()) {
        LoopClassifier::run(Result, "WHILE-Binary-Rel");
        return;
      }
      if (ConditionOp->isAssignmentOp()) {
        LoopClassifier::run(Result, "WHILE-Binary-Assign");
        return;
      }
      if (ConditionOp->isLogicalOp()) {
        LoopClassifier::run(Result, "WHILE-Binary-Logic");
        return;
      }
      std::stringstream sstm;
      sstm << ConditionOp->getOpcode();
      LoopClassifier::run(Result, "WHILE-Binary-Other-"+sstm.str());
      return;
    }

    /* while ((cast)...) {} */
    if (const CastExpr *Cast = dyn_cast<CastExpr>(Cond)) {
      LoopClassifier::run(Result, "WHILE-Cast");
      return;
    }

    /* LoopVariableFinder Finder; */
    /* const std::vector<const VarDecl*> LoopVarCandidates = Finder.findLoopVarCandidates(WS->getBody()); */

    /* AdaChecker Checker = AdaChecker(); */
    /* for (const VarDecl *LoopVarCandidate : LoopVarCandidates) { */
    /*   if (Checker.checkAdaCond(WS->getCond(), LoopVarCandidate, *Result.Context)) { */
    /*     LoopClassifier::run(Result, "WHILE-ADACandidate"); */
    /*     return; */
    /*   } */
    /* } */

    const std::string Cls(Cond->getStmtClassName());
    LoopClassifier::run(Result, "WHILE-Other-"+Cls);
    return;
  }
 private:
  class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
    public:
      const std::vector<const VarDecl*> findLoopVarCandidates(const Stmt *Body) {
        this->Body = Body;
        TraverseStmt(const_cast<Stmt *>(Body));
        return LoopVarCandidates;
      }

      bool VisitStmt(Stmt *Stmt) {
        if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Stmt)) {
          if (UOP->isIncrementDecrementOp()) {
            if (const VarDecl *LoopVarCandidate = getIntegerVariable(UOP->getSubExpr())) {
              LoopVarCandidates.push_back(LoopVarCandidate);
            }
          }
        }
        return true;
      }

    private:
      const Stmt *Body;
      std::vector<const VarDecl*> LoopVarCandidates;
  };
};

/* TODO: not sound wrt impure function calls, aliasses, nested loops, control flow
 * within the loop */
class ForLoopClassifier : public LoopClassifier {
 public:
  ForLoopClassifier(Replacements *Replace) : LoopClassifier("FOR-ADA", Replace) {}

  void run(const MatchFinder::MatchResult &Result) {
    const ForStmt *FS = Result.Nodes.getNodeAs<clang::ForStmt>(LoopName);
    std::string suffix;

    /* loop init
     * =========
     *
     * loop init is a single integer declaration
     */
    const VarDecl *InitVar = NULL;
    if (FS->getInit()) {
      /* either this is 
      * - a DeclStmt
      * - a BinaryOperator =
      * - a UnaryOperator {++,--}
      */
      if (const DeclStmt *InitDecl = dyn_cast<DeclStmt>(FS->getInit())) {
        if (!InitDecl->isSingleDecl()) {
          LoopClassifier::run(Result, "FOR-(!ADA_Init_Multi_Decl)");
          return;
        }
        InitVar = dyn_cast<VarDecl>(InitDecl->getSingleDecl());
      }
      else if (const BinaryOperator *InitAssignment = dyn_cast<BinaryOperator>(FS->getInit())) {
        if (!InitAssignment->isAssignmentOp()) {
          LoopClassifier::run(Result, "FOR-(!ADA_Init_BinOp_NoAssign)");
          return;
        }
        if (!(InitVar = getVariable(InitAssignment->getLHS()))) {
          LoopClassifier::run(Result, "FOR-(!ADA_Init_BinOp_LHS_NoVar)");
          return;
        }
      }
      else if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(FS->getInit())) {
        if (UOP->isIncrementDecrementOp()) {
          if (!(InitVar = getVariable(UOP->getSubExpr()))) {
            LoopClassifier::run(Result, "FOR-(!ADA_Init_Unary_SubExprNoVar)");
            return;
          }
          suffix += "-INIT_INC/DEC";
        }
        else {
          LoopClassifier::run(Result, "FOR-(!ADA_Init_Unary_TooComplex)");
          return; // TODO this could be something more complex
        }
      }
      else {
        LoopClassifier::run(Result, "FOR-(!ADA_Init_TooComplex)");
        return; // TODO this could be something more complex
      }
      /* check InitVar type is Integer */
      if (!InitVar->getType()->isIntegerType()) {
        LoopClassifier::run(Result, "FOR-(!ADA_Init_NoInteger)");
        /* llvm::outs() << InitVar->getType().getAsString() << "\n"; */
        return;
      }
    }
    else {
      suffix += "-NO_INIT";
    }

    /* loop increment
     * ==============
     */
    // check increment is a unary operator, ++ or -- 
    if (!FS->getInc()) {
      // TODO increment could happen elsewhere
      LoopClassifier::run(Result, "FOR-(!ADA_Inc_None)");
      return;
    }
    const UnaryOperator *IncrementOp;
    if (!(IncrementOp = dyn_cast<UnaryOperator>(FS->getInc()))) {
      LoopClassifier::run(Result, "FOR-(!ADA_Inc_NotUnary)");
      return;
    }
    const VarDecl *IncVar;
    if (!(IncVar = getIntegerVariable(IncrementOp->getSubExpr()))) {
      LoopClassifier::run(Result, "FOR-(!ADA_Inc_UnaryOperator_SubExprIntNoVar");
      return;
    }
    const VarDecl *LoopVar = IncVar;

    /* loop condition & increment
     * ==========================
     * 
     * loop condition is one of the following combinations:
     *   i <  N ; ++i
     *   i <= N ; ++i
     *   i >  N ; --i
     *   i >= N ; --i
     * and similar cases where i and N are swapped
     */
    if (!FS->getCond()) {
      LoopClassifier::run(Result, "FOR-(!ADA_Cond_None)");
      return;
    }
    AdaChecker Checker;
    if (!Checker.checkAdaCond(FS->getCond(), LoopVar, *Result.Context)) {
      LoopClassifier::run(Result, "FOR-"+Checker.getReason());
      return;
    }
    const BinaryOperator *ConditionOp = Checker.getConditionOp();
    const VarDecl *CondVar = Checker.getCondVar();
    const VarDecl *BoundVar = Checker.getBoundVar();
    const bool LoopVarLHS = Checker.getLoopVarLHS();

    // check InitVar == CondVar == IncVar
    if (InitVar != NULL && !areSameVariable(InitVar, CondVar)) {
      LoopClassifier::run(Result, "FOR-(!ADA_InitVar_NEQ_CondVar)");
      return;
    }
    if (!areSameVariable(CondVar, IncVar)) {
      LoopClassifier::run(Result, "FOR-(!ADA_CondVar_NEQ_IncVar)");
      return;
    }

    // check we're counting towards a bound
    const BinaryOperatorKind ConditionOpCode = ConditionOp->getOpcode();
    if (ConditionOpCode == BO_LT || ConditionOpCode == BO_LE) {
      if ((LoopVarLHS && !IncrementOp->isIncrementOp()) ||
          (!LoopVarLHS && IncrementOp->isIncrementOp())) {
        LoopClassifier::run(Result, "FOR-(!ADA_NotTowardsBound)");
        return;
      }
    }
    else if (ConditionOpCode == BO_GT || ConditionOpCode == BO_GE) {
      if ((LoopVarLHS && !IncrementOp->isDecrementOp()) ||
          (!LoopVarLHS && IncrementOp->isDecrementOp())) {
        LoopClassifier::run(Result, "FOR-(!ADA_NotTowardsBound)");
        return;
      }
    }
    else {
      LoopClassifier::run(Result, "FOR-(!ADA_Cond_NoComparison)");
      return;
    }

    // check the condition bound (if any) isn't assigned in the loop body
    if (BoundVar != NULL) {
      VariableAssignmentASTVisitor Finder(BoundVar);
      if (Finder.findUsages(FS->getBody())) {
        LoopClassifier::run(Result, "FOR-(!ADA_N-ASSIGNED)");
        return;
      }
    }

    // ensure the loop variable isn't assigned in the loop body
    VariableAssignmentASTVisitor Finder(LoopVar);
    if (Finder.findUsages(FS->getBody())) {
      LoopClassifier::run(Result, "FOR-(!ADA_i-ASSIGNED)");
      return;
    }

    LoopClassifier::run(Result, Marker+suffix);
  }
 private:
  class VariableAssignmentASTVisitor : public RecursiveASTVisitor<VariableAssignmentASTVisitor> {
    public:
      VariableAssignmentASTVisitor(const VarDecl *LoopVar) : AssignedTo(false), LoopVar(LoopVar) {};

      bool findUsages(const Stmt *Body) {
        this->Body = Body;
        TraverseStmt(const_cast<Stmt *>(Body));
        return AssignedTo;
      }

      // TODO visit parents directly
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
            return false;
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
                return false;
              }
        return true;
      }

    private:
      bool AssignedTo;
      const VarDecl *LoopVar;
      const Stmt *Body;
  };
};

/* FS->viewAST(); // dotty */
/* FS->dumpColor(); // AST */
/* FS->dumpPretty(*Result.Context); // C source */

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_
