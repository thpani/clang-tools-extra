#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_

#include <sstream>
#include <algorithm>
#include <vector>
#include "clang/AST/ASTContext.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;
using namespace clang::tooling;

std::map<llvm::FoldingSetNodeID, std::vector<std::string> > Classifications;

enum CountingDirection { PLUS, MINUS };
struct IncrementInfo {
  CountingDirection Direction;
  unsigned long long Step;
  const Stmt *Statement;
};

class checkerror {
  public:
    checkerror(const std::string &reason) : reason(reason) {}
    const std::string what() const throw() {
      return reason;
    }
  private:
    const std::string reason;
};

static bool areSameStmt(const ASTContext &Context, const Stmt *First, const Stmt *Second) {
  if (!First || !Second)
    return false;
  llvm::FoldingSetNodeID FirstID, SecondID;
  First->Profile(FirstID, Context, true);
  Second->Profile(SecondID, Context, true);
  return FirstID == SecondID;
}

static const IntegerLiteral *getIntegerLiteral(const Expr *Expression, const ASTContext &Context) {
  return dyn_cast<IntegerLiteral>(Expression->IgnoreParenImpCasts());
}

static bool isIntegerLiteral(const Expr *Expression, const ASTContext &Context) {
  return getIntegerLiteral(Expression, Context) != NULL;
}

static const EnumConstantDecl *getEnum(const Expr *Expression) {
  const Expr *AdjustedExpr = Expression->IgnoreParenImpCasts();
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(AdjustedExpr)) {
    if (const EnumConstantDecl *Var = dyn_cast<EnumConstantDecl>(DRE->getDecl())) {
      return Var;
    }
  }
  return NULL;
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

static const ValueDecl *getVariableOrEnum(const Expr *Expression) {
  if (const VarDecl *Var = getVariable(Expression))
    return Var;
  return getEnum(Expression);
}

static const VarDecl *getIntegerVariable(const Expr *Expression) {
  const VarDecl *Var = getVariable(Expression);
  if (Var != NULL && Var->getType()->isIntegerType()) {
    return Var;
  }
  return NULL;
}

static const ValueDecl *getIntegerVariableOrEnum(const Expr *Expression) {
  const ValueDecl *Val = getVariableOrEnum(Expression);
  if (Val != NULL && Val->getType()->isIntegerType()) {
    return Val;
  }
  return NULL;
}

static bool areSameVariable(const ValueDecl *First, const ValueDecl *Second) {
  return First != NULL && Second != NULL &&
         First->getCanonicalDecl() == Second->getCanonicalDecl();
}

static bool areSameIntegerVariable(const Expr *First, const VarDecl *Second) {
  const VarDecl *FirstVar = getIntegerVariable(First);
  return areSameVariable(FirstVar, Second);
}

static std::pair<const VarDecl*, const IncrementInfo> getAdaIncrementInfo(Stmt *Stmt, const ASTContext &Context) {
  if (Expr *Expression = dyn_cast<Expr>(Stmt)) {
    Expression = Expression->IgnoreParenImpCasts();
    if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Expression)) {
      // i{++,--}
      if (UOP->isIncrementDecrementOp()) {
        if (const VarDecl *LoopVarCandidate = getIntegerVariable(UOP->getSubExpr())) {
          const IncrementInfo I = {
            UOP->isIncrementOp() ? PLUS : MINUS,
            1,
            UOP
          };
          return std::pair<const VarDecl*, const IncrementInfo>(LoopVarCandidate, I);
        }
      }
    }
    else if (const BinaryOperator *BOP = dyn_cast<BinaryOperator>(Expression)) {
      // i = i {+-} d
      if (BOP->getOpcode() == BO_Assign) {
        if (const VarDecl *LoopVarCandidate = getIntegerVariable(BOP->getLHS())) {
          if (const BinaryOperator *RHS = dyn_cast<BinaryOperator>(BOP->getRHS()->IgnoreParenImpCasts())) {
            if (RHS->isAdditiveOp()) {
              const Expr *RRHS = RHS->getRHS();
              const Expr *RLHS = RHS->getLHS();
              // TODO other side could also be a integer var / -> check if not assigned
              const IntegerLiteral *Int;
              if ((areSameIntegerVariable(RRHS, LoopVarCandidate) && (Int = getIntegerLiteral(RLHS, Context)) != NULL) ||
                  (areSameIntegerVariable(RLHS, LoopVarCandidate) && (Int = getIntegerLiteral(RRHS, Context)) != NULL)) {
                const IncrementInfo I = {
                  RHS->getOpcode() == BO_Add ? PLUS : MINUS,
                  *Int->getValue().getRawData(),
                  BOP
                };
                return std::pair<const VarDecl*, const IncrementInfo>(LoopVarCandidate, I);
              }
            }
          }
        }
      }
      // i {+-}= d
      else if (BOP->getOpcode() == BO_AddAssign ||
                BOP->getOpcode() == BO_SubAssign ) {
        if (const VarDecl *LoopVarCandidate = getIntegerVariable(BOP->getLHS())) {
          if (const IntegerLiteral *Int = getIntegerLiteral(BOP->getRHS(), Context)) {
              const IncrementInfo I = {
                BOP->getOpcode() == BO_AddAssign ? PLUS : MINUS,
                *Int->getValue().getRawData(),
                BOP
              };
              return std::pair<const VarDecl*, const IncrementInfo>(LoopVarCandidate, I);
          }
        }
      }
    }
  }
  throw checkerror("No valid ADA increment");
}

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
class AdaChecker {
  public:
    AdaChecker(const ASTContext &Context) : Context(Context) {}
    void check(const Expr *Cond, const IncrementInfo &Increment, const Stmt* Body, const VarDecl *LoopVar, const VarDecl *InitVar, const VarDecl *IncVar) {
      checkCond(Cond, LoopVar);
      checkVars(InitVar, IncVar);
      checkBody(Body, Cond, Increment, LoopVar);
    }
    std::string getSuffix() {
      return suffix;
    }
  private:
    void checkCond(const Expr *Cond, const VarDecl *LoopVar) {
      if (!Cond) {
        throw checkerror("!ADA_Cond_None");
      }
      /* check the condition is a binary operator
       * where either LHS or RHS are integer lieterals
       * or one is an integer literal
       */
      const Expr *CondInner = Cond->IgnoreParenImpCasts();
      if (!(ConditionOp = dyn_cast<BinaryOperator>(CondInner))) {
        throw checkerror("!ADA_Cond_No_BinaryOperator");
      }
      if (!ConditionOp->isRelationalOp()) {
        throw checkerror("!ADA_Cond_Binary_NotRel");
      }
      if (!(ConditionOp->getLHS()->getType()->isIntegerType()))
        throw checkerror("!ADA_Cond_LHS_NoInteger");
      if (!(ConditionOp->getRHS()->getType()->isIntegerType()))
        throw checkerror("!ADA_Cond_RHS_NoInteger");

      // see if LHS/RHS is integer var
      const ValueDecl *LHS = getIntegerVariableOrEnum(ConditionOp->getLHS());
      const ValueDecl *RHS = getIntegerVariableOrEnum(ConditionOp->getRHS());
      // see if LHS/RHS is (integer var)++
      bool UOPLHS = false,
           UOPRHS = false;
      if (LHS == NULL) {
        if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(ConditionOp->getLHS()->IgnoreParenImpCasts())) {
          if (UOP->isIncrementDecrementOp()) {
            LHS = getIntegerVariable(UOP->getSubExpr());
            UOPLHS = true;
          }
        }
      }
      if (RHS == NULL) {
        if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(ConditionOp->getRHS()->IgnoreParenImpCasts())) {
          if (UOP->isIncrementDecrementOp()) {
            RHS = getIntegerVariable(UOP->getSubExpr());
            UOPRHS = true;
          }
        }
      }
      if (LHS != NULL && RHS != NULL) {
        // both lhs and rhs are vars
        // check which one is the loop var (i.e. equal to the initialization variable)
        if (areSameVariable(LoopVar, LHS)) {
          LoopVarLHS = true;
        }
        else if (areSameVariable(LoopVar, RHS)) {
          LoopVarLHS = false;
        }
        else {
          throw checkerror("!ADA_Cond_LoopVar_NotIn_BinaryOperator");
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
        /* std::string r = std::string(ConditionOp->getRHS()->IgnoreParenImpCasts()->getStmtClassName()); */
        /* std::string l = std::string(ConditionOp->getLHS()->IgnoreParenImpCasts()->getStmtClassName()); */
        /* llvm::outs() << "! " << l << " " << r << "\n"; */
        /* if (r=="DeclRefExpr" && l=="DeclRefExpr") { */
        /*   ConditionOp->getRHS()->dumpColor(); */
        /*   ConditionOp->getLHS()->dumpColor(); */
        /* } */
        throw checkerror("!ADA_Cond_Bound_TooComplex");
      }
      if (LoopVarLHS && UOPRHS) throw checkerror("!ADA_Cond_Bound_TooComplex_InUOP");
      if (!LoopVarLHS && UOPLHS) throw checkerror("!ADA_Cond_Bound_TooComplex_InUOP");
      CondVar = LoopVarLHS ? LHS : RHS;
      BoundVar = LoopVarLHS ? RHS : LHS;
    }
    void checkVars(const VarDecl *InitVar, const VarDecl *IncVar) {
      // check InitVar == CondVar == IncVar
      if (InitVar != NULL && !areSameVariable(InitVar, CondVar)) {
        throw checkerror("!ADA_InitVar_NEQ_CondVar");
      }
      if (!areSameVariable(CondVar, IncVar)) {
        throw checkerror("!ADA_CondVar_NEQ_IncVar");
      }
    }
    void checkBody(const Stmt *Body, const Expr *Cond, const IncrementInfo &Increment, const VarDecl *LoopVar) {
      // check we're counting towards a bound
      const BinaryOperatorKind ConditionOpCode = ConditionOp->getOpcode();
      if (ConditionOpCode == BO_LT || ConditionOpCode == BO_LE) {
        if ((LoopVarLHS && Increment.Direction == MINUS) ||
            (!LoopVarLHS && Increment.Direction == PLUS)) {
          throw checkerror("!ADA_NotTowardsBound");
        }
      }
      else if (ConditionOpCode == BO_GT || ConditionOpCode == BO_GE) {
        if ((LoopVarLHS && Increment.Direction == PLUS) ||
            (!LoopVarLHS && Increment.Direction == MINUS)) {
          throw checkerror("!ADA_NotTowardsBound");
        }
      }

      // check the condition bound var (if any) isn't assigned in the loop body
      // TODO fix this if incr becomes more flexible (step is symbolic)
      if (BoundVar != NULL) {
        VariableAssignmentASTVisitor Finder(BoundVar, Context);
        if (Finder.findUsages(Body)) {
          throw checkerror("!ADA_N-ASSIGNED");
        }
      }

      // ensure the loop variable isn't assigned in the loop body
      {
        VariableAssignmentASTVisitor Finder(LoopVar, Context);
        if (Finder.findUsages(Body, Increment.Statement)) {
          throw checkerror("!ADA_i-ASSIGNED-Body");
        }
        if (Finder.findUsages(Cond, Increment.Statement)) {
          throw checkerror("!ADA_i-ASSIGNED-Cond");
        }
      }

      // see if there are nested loops
      {
        LoopASTVisitor Finder;
        if (Finder.findUsages(Body, const_cast<ASTContext&>(Context))) {
          suffix += "-NESTED";
        }
      }
    }
  private:
    const ASTContext &Context;
    const BinaryOperator *ConditionOp;
    const ValueDecl *CondVar;
    const ValueDecl *BoundVar;
    bool LoopVarLHS;
    std::string suffix;

  class LoopASTVisitor : public MatchFinder::MatchCallback {
    public:
      bool findUsages(const Stmt *Body, ASTContext &Context) {
        MatchFinder Finder;
        Finder.addMatcher(AnyLoopMatcher, this);
        Finder.match(*Body, Context);
        return LoopFound;
      }

      virtual void run(const MatchFinder::MatchResult &Result) {
        LoopFound = true;
      }
    private:
      bool LoopFound = false;
  };

  class VariableAssignmentASTVisitor : public RecursiveASTVisitor<VariableAssignmentASTVisitor> {
    public:
      VariableAssignmentASTVisitor(const ValueDecl *LoopVar, const ASTContext &Context) : LoopVar(LoopVar), Context(Context) {};

      bool findUsages(const Stmt *Body, const Stmt *AllowedAssignment=NULL) {
        AssignedTo = false;
        this->Body = Body;
        this->AllowedAssignment = AllowedAssignment;
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
          if (areSameStmt(Context, UOP, AllowedAssignment)) {
            return true;
          }
          if (UOP->isIncrementDecrementOp()) {
            AssignedTo = true;
            return false;
          }
        }

        /* (Compound) assignment.
        * Ensure we came here for the LHS, i.e. LoopVar = <EXPR>;
        */
        if (const BinaryOperator *BOP = dyn_cast<BinaryOperator>(Parent)) {
          if (BOP->isAssignmentOp()) {
            if (areSameStmt(Context, BOP, AllowedAssignment)) {
              return true;
            }
            if (const DeclRefExpr *LHS = dyn_cast<DeclRefExpr>(BOP->getLHS())) {
              if (areSameVariable(LoopVar, LHS->getDecl()))
              {
                AssignedTo = true;
                return false;
              }
            }
          }
        }
        return true;
      }

    private:
      bool AssignedTo;
      const ValueDecl *LoopVar;
      const Stmt *Body;
      const Stmt *AllowedAssignment;
      const ASTContext &Context;
  };
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
  LoopClassifier(const std::string &Marker) : LoopCounter(Marker) {}

  virtual void run(const MatchFinder::MatchResult &Result, const std::string Marker) {
    LoopCounter::run(Result, Marker);
  }

  virtual void run(const MatchFinder::MatchResult &Result) {
    run(Result, Marker);
  }
};

class EmptyBodyClassifier : public LoopClassifier {
  public:
    EmptyBodyClassifier() : LoopClassifier("ANY-EMPTYBODY") {}

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

class ConditionClassifier : public LoopClassifier {
  public:
    ConditionClassifier(const std::string Marker) : LoopClassifier(Marker) {}
  protected:
  void classifyCond(const MatchFinder::MatchResult &Result, const Expr *Cond) {
    std::string suffix;

    /* for(<init>;;<incr>) */
    if (Cond == NULL) {
      LoopClassifier::run(Result, Marker+"-Cond-NULL");
      return;
    }

    /* while(<INTEGER LITERAL>) */
    if (const IntegerLiteral *Lit = getIntegerLiteral(Cond, *Result.Context)) {
      if (Lit->getValue().getBoolValue()) {
        LoopClassifier::run(Result, Marker+"-Cond-TRUE");
      }
      else {
        LoopClassifier::run(Result, Marker+"-Cond-FALSE");
      }
      return;
    }

    /* while (<UNARY>) */
    if (const UnaryOperator *ConditionOp = dyn_cast<UnaryOperator>(Cond)) {
      if (ConditionOp->isIncrementDecrementOp()) {
        LoopClassifier::run(Result, Marker+"-Cond-Unary-IncDec");
        return;
      }
      LoopClassifier::run(Result, Marker+"-Cond-Unary-Other");
      return;
    }

    /* while(<BINARY>) */
    if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(Cond)) {
      if (ConditionOp->isEqualityOp()) {
        LoopClassifier::run(Result, Marker+"-Cond-Binary-Equality");
        return;
      }
      if (ConditionOp->isRelationalOp()) {
        LoopClassifier::run(Result, Marker+"-Cond-Binary-Rel");
        return;
      }
      if (ConditionOp->isAssignmentOp()) {
        LoopClassifier::run(Result, Marker+"-Cond-Binary-Assign");
        return;
      }
      if (ConditionOp->isLogicalOp()) {
        LoopClassifier::run(Result, Marker+"-Cond-Binary-Logic");
        return;
      }
      std::stringstream sstm;
      sstm << ConditionOp->getOpcode();
      LoopClassifier::run(Result, Marker+"-Cond-Binary-Other-"+sstm.str());
      return;
    }

    /* while ((cast)...) {} */
    if (/*const CastExpr *Cast =*/ dyn_cast<CastExpr>(Cond)) {
      LoopClassifier::run(Result, Marker+"-Cond-Cast");
      return;
    }

    const std::string Cls(Cond->getStmtClassName());
    LoopClassifier::run(Result, Marker+"-Cond-Other-"+Cls);
    return;
  }
};

class WhileLoopClassifier : public ConditionClassifier {
 public:
   WhileLoopClassifier() : ConditionClassifier("WHILE") {}
  void run(const MatchFinder::MatchResult &Result) {
    const WhileStmt *WS = Result.Nodes.getNodeAs<clang::WhileStmt>(LoopName);
    const Expr *Cond = WS->getCond()->IgnoreParenImpCasts();

    classifyCond(Result, Cond);
    classifyAda(Result, WS, Cond);
  }
  void classifyAda(const MatchFinder::MatchResult &Result, const WhileStmt *WS, const Expr *Cond) {
      // find unary increment/decrement -> loop variable candidates
      LoopVariableFinder Finder(*Result.Context);
      auto LoopVarCandidates = Finder.findLoopVarCandidates(WS->getCond(), WS->getBody());

      // check loop variable conadidates for ada condition
      AdaChecker Checker(*Result.Context);
      std::vector<std::string> reasons;
      for (auto Pair : LoopVarCandidates) {
        const VarDecl *LoopVarCandidate = Pair.first;
        const IncrementInfo Increment = Pair.second;
        try {
          Checker.check(WS->getCond(), Increment, WS->getBody(), LoopVarCandidate, NULL, LoopVarCandidate);
          LoopClassifier::run(Result, "WHILE-ADA"+Checker.getSuffix());
          return;
        } catch(checkerror &e) {
          reasons.push_back(e.what());
        }
      }
      std::vector<std::string>::iterator it;
      std::sort(reasons.begin(), reasons.end());
      it = std::unique (reasons.begin(), reasons.end());
      reasons.resize( std::distance(reasons.begin(),it) );

      std::stringstream ss;
      for(auto reason : reasons) {
        ss << reason << "-";
      }
      std::string suffix = ss.str();
      suffix = suffix.substr(0, suffix.size()-1);
      if (suffix.size()==0) {
        suffix = "!ADA-NoLoopVarCandidate";
      }

      LoopClassifier::run(Result, "WHILE-"+suffix);
      return;
  }

 private:
  class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
    public:
      LoopVariableFinder(const ASTContext &Context) : Context(Context) {}
      const std::vector<const std::pair<const VarDecl*, const IncrementInfo>> findLoopVarCandidates(const Stmt *Cond, const Stmt *Body) {
        TraverseStmt(const_cast<Stmt *>(Cond));
        TraverseStmt(const_cast<Stmt *>(Body));
        return LoopVarCandidates;
      }

      bool VisitStmt(Stmt *Stmt) {
        try {
          auto Pair = getAdaIncrementInfo(Stmt, Context);
          LoopVarCandidates.push_back(Pair);
        } catch(checkerror) {}
        return true;
      }

    private:
      std::vector<const std::pair<const VarDecl*, const IncrementInfo>> LoopVarCandidates;
      const ASTContext &Context;
  };
};

/* TODO: not sound wrt impure function calls, aliasses, nested loops, control flow
 * within the loop */
class ForLoopClassifier : public ConditionClassifier {
 public:
   ForLoopClassifier() : ConditionClassifier("FOR") {}
  void run(const MatchFinder::MatchResult &Result) {
    const ForStmt *FS = Result.Nodes.getNodeAs<clang::ForStmt>(LoopName);
    const Expr *Cond = FS->getCond();

    classifyCond(Result, Cond);
    classifyAda(Result, FS, Cond);
  }
  void classifyAda(const MatchFinder::MatchResult &Result, const ForStmt *FS, const Expr *Cond) {
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
        if (InitDecl->isSingleDecl()) {
          InitVar = dyn_cast<VarDecl>(InitDecl->getSingleDecl());
        }
        else {
          // TODO search through MultiDecl
          suffix += "-Init_Multi_Decl";
        }
      }
      else if (const BinaryOperator *InitAssignment = dyn_cast<BinaryOperator>(FS->getInit())) {
        if (InitAssignment->isAssignmentOp()) {
          if (!(InitVar = getVariable(InitAssignment->getLHS()))) {
            suffix += "-Init_BinOp_LHS_NoVar";
          }
        }
        else {
          suffix += "-Init_BinOp_NoAssign";
        }
      }
      else if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(FS->getInit())) {
        if (UOP->isIncrementDecrementOp()) {
          if ((InitVar = getVariable(UOP->getSubExpr())) != NULL) {
            suffix += "-Init_Inc/Dec";
          }
          else {
            suffix += "-Init_Unary_SubExprNoVar";
          }
        }
        else {
          suffix += "-Init_Unary_No_Inc/Dec";
          return; // TODO this could be something more complex
        }
      }
      else {
        suffix += "-Init_TooComplex";
        return; // TODO this could be something more complex
      }
      /* check InitVar type is Integer */
      if (InitVar != NULL && !InitVar->getType()->isIntegerType()) {
        suffix += "-Init_NoInteger";
        /* llvm::outs() << InitVar->getType().getAsString() << "\n"; */
        InitVar = NULL;
      }
    }
    else {
      suffix += "-Init_None";
    }

    /* loop increment
     * ==============
     */
    // check increment is a unary operator, ++ or -- 
    if (!FS->getInc()) {
      // TODO increment could happen elsewhere
      LoopClassifier::run(Result, "FOR-!ADA-Inc_None");
      return;
    }
    const UnaryOperator *IncrementOp;
    if (!(IncrementOp = dyn_cast<UnaryOperator>(FS->getInc()))) {
      LoopClassifier::run(Result, "FOR-!ADA-Inc_NotUnary");
      return;
    }
    const VarDecl *IncVar;
    if (!(IncVar = getIntegerVariable(IncrementOp->getSubExpr()))) {
      LoopClassifier::run(Result, "FOR-!ADA-Inc_UnaryOperator_SubExprIntNoVar");
      return;
    }
    const VarDecl *LoopVar = IncVar;

    AdaChecker Checker(*Result.Context);
    try {
      const IncrementInfo Incr = {
        IncrementOp->isIncrementOp() ? PLUS : MINUS,
        1,
        IncrementOp
      };

      Checker.check(FS->getCond(), Incr, FS->getBody(), LoopVar, InitVar, IncVar);
      LoopClassifier::run(Result, "FOR-ADA"+suffix+Checker.getSuffix());
    } catch(checkerror &e) {
      LoopClassifier::run(Result, "FOR-"+e.what());
    }
  }
};

/* FS->viewAST(); // dotty */
/* FS->dumpColor(); // AST */
/* FS->dumpPretty(*Result.Context); // C source */

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFY_H_
