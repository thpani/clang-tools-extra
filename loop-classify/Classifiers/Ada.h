#include "IncrementClassifier.h"

class AdaForLoopClassifier : public IncrementClassifier {
  public:
    AdaForLoopClassifier() : IncrementClassifier("ADA") {}

  protected:
    const IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      if (Expr == NULL) throw checkerror("!ADA_Inc_None");
      const class Expr *Expression = Expr->IgnoreParenImpCasts();
      if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Expression)) {
        // i{++,--}
        if (UOP->isIncrementDecrementOp()) {
          if (const VarDecl *VD = getIntegerVariable(UOP->getSubExpr())) {
            return { VD, UOP, NULL };
          }
        }
      }
      else if (const BinaryOperator *BOP = dyn_cast<BinaryOperator>(Expression)) {
        // i = i {+-} <int>
        // i = <int> {+-} i
        if (BOP->getOpcode() == BO_Assign) {
          if (const VarDecl *VD = getIntegerVariable(BOP->getLHS())) {
            if (const BinaryOperator *RHS = dyn_cast<BinaryOperator>(BOP->getRHS()->IgnoreParenImpCasts())) {
              if (RHS->isAdditiveOp()) {
                const VarDecl *RRHS = getIntegerVariable(RHS->getRHS());
                const VarDecl *RLHS = getIntegerVariable(RHS->getLHS());
                // TODO this could also be a integer var / -> check if not assigned
                if ((RRHS == VD && isIntegerConstant(RHS->getLHS(), this->Context)) ||
                    (RLHS == VD && isIntegerConstant(RHS->getRHS(), this->Context))) {
                  return { VD, BOP, NULL };
                }
              }
            }
          }
        }
        // i {+-}= <int>
        else if (BOP->getOpcode() == BO_AddAssign ||
                  BOP->getOpcode() == BO_SubAssign ) {
          if (const VarDecl *VD = getIntegerVariable(BOP->getLHS())) {
            if (isIntegerConstant(BOP->getRHS(), this->Context)) {
              return { VD, BOP, NULL };
            }
            else if (const VarDecl *Delta = getIntegerVariable(BOP->getRHS())) {
              return { VD, BOP, Delta };
            }
          }
        }
      }
      throw checkerror("!ADA_Inc_NotAda");
    }

    const std::string check(const Stmt *LS, const IncrementInfo Increment) const throw (checkerror) {
      /* COND */
      const Expr *Cond = getLoopCond(LS);
      if (!Cond) {
        throw checkerror("?ADA_Cond_None");
      }
      /* check the condition is a binary operator
       * where either LHS or RHS are integer lieterals
       * or one is an integer literal
       */
      const Expr *CondInner = Cond->IgnoreParenImpCasts();
      const VarDecl *CondVar;
      const ValueDecl *BoundVar;
      const Stmt *BoundVarAllowedAssign = NULL;
      if (const VarDecl *VD = getIntegerVariable(CondInner)) {
        CondVar = VD;
        BoundVar = VD;
      }
      else if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(CondInner)) {
        if (UO-> isIncrementDecrementOp()) {
          if (const VarDecl *VD = getIntegerVariable(UO->getSubExpr())) {
            CondVar = VD;
            BoundVar = VD;
            BoundVarAllowedAssign = UO;
          }
          else {
            throw checkerror("?ADA_Cond_UnaryNoIntegerVar");
          }
        }
        else {
          throw checkerror("?ADA_Cond_UnaryNotIncDec");
        }
      }
      else if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(CondInner)) {
        if (!ConditionOp->isRelationalOp()) {
          throw checkerror("?ADA_Cond_Binary_NotRel");
        }
        if (!(ConditionOp->getLHS()->getType()->isIntegerType()))
          throw checkerror("?ADA_Cond_LHS_NoInteger");
        if (!(ConditionOp->getRHS()->getType()->isIntegerType()))
          throw checkerror("?ADA_Cond_RHS_NoInteger");

        // see if LHS/RHS is integer var
        bool LoopVarLHS;
        const ValueDecl *LHS = getIntegerField(ConditionOp->getLHS());
        const ValueDecl *RHS = getIntegerField(ConditionOp->getRHS());
        // see if LHS/RHS is (integer var)++
        if (LHS == NULL) {
          if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(ConditionOp->getLHS()->IgnoreParenImpCasts())) {
            if (UOP->isIncrementDecrementOp()) {
              LHS = getIntegerVariable(UOP->getSubExpr());
            }
          }
        }
        if (RHS == NULL) {
          if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(ConditionOp->getRHS()->IgnoreParenImpCasts())) {
            if (UOP->isIncrementDecrementOp()) {
              RHS = getIntegerVariable(UOP->getSubExpr());
            }
          }
        }

        /* determine which is loop var, which is bound */
        if (LHS != NULL && RHS != NULL) {
          // both lhs and rhs are vars
          // check which one is the loop var (i.e. equal to the initialization variable)
          if (Increment.VD == LHS) {
            LoopVarLHS = true;
          }
          else if (Increment.VD == RHS) {
            LoopVarLHS = false;
          }
          else {
            throw checkerror("?ADA_Cond_LoopVar_NotIn_BinaryOperator");
          }
        }
        else if (LHS != NULL && isIntegerConstant(ConditionOp->getRHS(), this->Context)) {
          // LHS is loop var, RHS is integer bound
          LoopVarLHS = true;
        }
        else if (RHS != NULL && isIntegerConstant(ConditionOp->getLHS(), this->Context)) {
          // RHS is loop var, LHS is integer bound
          LoopVarLHS = false;
        }
        else if (LHS != NULL && dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenImpCasts())) {
          // LHS is loop var, RHS is sizeof() bound
          if (const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenImpCasts())) {
            if (B->getKind() == UETT_SizeOf) {
              LoopVarLHS = true;
            }
          }
        }
        else if (RHS != NULL && dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenImpCasts())) {
          // RHS is loop var, LHS is sizeof() bound
          if (const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenImpCasts())) {
            if (B->getKind() == UETT_SizeOf) {
              LoopVarLHS = false;
            }
          }
        }
        else {
          throw checkerror("?ADA_Cond_Bound_TooComplex");
        }
        CondVar = dyn_cast<VarDecl>(LoopVarLHS ? LHS : RHS);
        BoundVar = LoopVarLHS ? RHS : LHS; // null if integer-literal, sizeof, ...
      }
      else {
        throw checkerror("?ADA_Cond_NotSupported");
      }

      /* VARS */

      // check InitVar == CondVar == IncVar
      /* if (InitVar != NULL && InitVar != CondVar) { */
      /*   throw checkerror("!ADA_InitVar_NEQ_CondVar"); */
      /* } */
      if (CondVar != Increment.VD) {
        throw checkerror("?ADA_CondVar_NEQ_IncVar");
      }
      
      /* BODY */
      // TODO check we're counting towards a bound

      if (BoundVar != NULL) {
        addPseudoConstantVar("N", BoundVar, BoundVarAllowedAssign);
      }
      if (Increment.Delta != NULL) {
        addPseudoConstantVar("D", Increment.Delta);
      }
      addPseudoConstantVar("i", Increment.VD, Increment.Statement);

      try {
        checkPseudoConstantSet(LS);
      } catch (checkerror &e) {
        throw checkerror("!ADA_"+e.what());
      }

      // TODO
/*       // see if there are nested loops */
/*       { */
/*         LoopASTVisitor Finder; */
/*         if (Finder.findUsages(getLoopBody(LS), const_cast<ASTContext&>(Context))) { */
/*           suffix += "-NESTED"; */
/*         } */
/*       } */
      return "";
    }

/*     class LoopASTVisitor : public MatchFinder::MatchCallback { */
/*       public: */
/*         bool findUsages(const Stmt *Body, ASTContext &Context) { */
/*           MatchFinder Finder; */
/*           Finder.addMatcher(AnyLoopMatcher, this); */
/*           Finder.match(*Body, Context); */
/*           return LoopFound; */
/*         } */

/*         virtual void run(const MatchFinder::MatchResult &Result) { */
/*           LoopFound = true; */
/*         } */
/*       private: */
/*         bool LoopFound = false; */
/*     }; */
};

