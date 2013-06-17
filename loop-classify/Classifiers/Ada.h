#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CLASSIFIERS_ADA_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CLASSIFIERS_ADA_
#include "IncrementClassifier.h"

class AdaForLoopClassifier : public IncrementClassifier {
  private:
    const ASTContext *Context;

  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isIntegerType);
    }

    std::pair<std::string, const ValueDecl*> checkCond(const NaturalLoop *L, const IncrementInfo Increment) const throw (checkerror) {
      std::string Suffix;

      /* COND */
      const Expr *Cond = (*L->getExit().pred_begin())->getCond();
      if (Cond == NULL) {
        throw checkerror(Unknown, Marker, "Cond_None");
      }
      /* check the condition is a binary operator
       * where either LHS or RHS are integer lieterals
       * or one is an integer literal
       */
      const Expr *CondInner = Cond->IgnoreParenCasts();
      const ValueDecl *CondVar;
      const ValueDecl *BoundVar;
      if (const VarDecl *VD = getIntegerVariable(CondInner)) {
        CondVar = VD;
        BoundVar = VD;
      }
      else if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(CondInner)) {
        if (UO->isIncrementDecrementOp()) {
          if (const VarDecl *VD = getIntegerVariable(UO->getSubExpr())) {
            CondVar = VD;
            BoundVar = VD;
          }
          else {
            throw checkerror(Unknown, Marker, "Cond_UnaryNoIntegerVar");
          }
        }
        else {
          throw checkerror(Unknown, Marker, "Cond_UnaryNotIncDec");
        }
      }
      else if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(CondInner)) {
        BinaryOperatorKind Opc = ConditionOp->getOpcode();
        if ((Opc >= BO_Mul && Opc <= BO_Shr) || ConditionOp->isComparisonOp()) {
          if (!(ConditionOp->getLHS()->getType()->isIntegerType()))
            throw checkerror(Unknown, Marker, "Cond_LHS_NoInteger");
          if (!(ConditionOp->getRHS()->getType()->isIntegerType()))
            throw checkerror(Unknown, Marker, "Cond_RHS_NoInteger");

          // see if LHS/RHS is integer var
          bool LoopVarLHS;
          const ValueDecl *LHS = getIntegerField(ConditionOp->getLHS());
          const ValueDecl *RHS = getIntegerField(ConditionOp->getRHS());
          // see if LHS/RHS is (integer var)++
          if (LHS == NULL) {
            try {
              IncrementInfo I = getIncrementInfo(ConditionOp->getLHS());
              LHS = I.VD;
            } catch(checkerror) {}
          }
          if (RHS == NULL) {
            try {
              IncrementInfo I = getIncrementInfo(ConditionOp->getRHS());
              RHS = I.VD;
            } catch(checkerror) {}
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
              throw checkerror(Unknown, Marker, "Cond_LoopVar_NotIn_BinaryOperator");
            }
          }
          else if (LHS != NULL && isIntegerConstant(ConditionOp->getRHS(), Context)) {
            // LHS is loop var, RHS is integer bound
            LoopVarLHS = true;
          }
          else if (RHS != NULL && isIntegerConstant(ConditionOp->getLHS(), Context)) {
            // RHS is loop var, LHS is integer bound
            LoopVarLHS = false;
          }
          else if (LHS != NULL && dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenCasts())) {
            // LHS is loop var, RHS is sizeof() bound
            if (const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenCasts())) {
              if (B->getKind() == UETT_SizeOf) {
                LoopVarLHS = true;
              }
            }
          }
          else if (RHS != NULL && dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenCasts())) {
            // RHS is loop var, LHS is sizeof() bound
            if (const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenCasts())) {
              if (B->getKind() == UETT_SizeOf) {
                LoopVarLHS = false;
              }
            }
          }
          else if (LHS != NULL && LHS == Increment.VD) {
            LoopVarLHS = true;
            Suffix = "ComplexBound";
          }
          else if (RHS != NULL && RHS == Increment.VD) {
            LoopVarLHS = false;
            Suffix = "ComplexBound";
          }
          else {
            throw checkerror(Fail, Marker, "Cond_BinOp_TooComplex");
          }
          CondVar = LoopVarLHS ? LHS : RHS;
          BoundVar = LoopVarLHS ? RHS : LHS; // null if integer-literal, sizeof, ...
        }
        else {
          throw checkerror(Unknown, Marker, "Cond_BinOp_OpNotSupported");
        }
      }
      else {
        throw checkerror(Unknown, Marker, "Cond_OpNotSupported");
      }

      /* VARS */

      if (CondVar->getType()->isIntegerType() && CondVar != Increment.VD) {
        throw checkerror(Unknown, Marker, "CondVar_NEQ_IncVar");
      }
      
      /* BODY */

      // TODO addPseudoConstantVar("i", Increment.VD, Increment.Statement);
      // (i) denotes a line number in Aho 9.3.3, Fig. 9.23
      /* NaturalLoop L = getLoop(LS, Context); */
      /* if (L.Header) { */
      /*   std::map<const CFGBlock*, unsigned> Out, In, OldOut; */

      /*   for (auto B : L.Blocks) {   // (2) */
      /*     Out[B] = 0; */
      /*   } */
      /*   unsigned n = 0; */
      /*   for (auto Element : *L.Header) { */
      /*     const Stmt* S = Element.castAs<CFGStmt>().getStmt(); */
      /*     cloopy::PseudoConstantAnalysis A(S); */
      /*     if (!A.isPseudoConstant(Increment.VD)) { */
      /*       n++; */
      /*     } */
      /*   } */
      /*   Out[L.Header] = n;          // (1) */
      /*   while (Out != OldOut) {     // (3) */
      /*     OldOut = Out; */

      /*     for (auto B : L.Blocks) { // (4) */
      /*       unsigned min = std::numeric_limits<unsigned int>::max(); */
      /*       for (auto Pred = B->pred_begin(); Pred != B->pred_end(); Pred++) {  // (5) */
      /*         if (L.Blocks.count(*Pred) == 0) continue;   // ignore preds outside the loop */
      /*         if (Out[*Pred] < min) { */
      /*           min = Out[*Pred]; */
      /*         } */
      /*       } */
      /*       In[B] = min; */

      /*       if (B != L.Header) { */
      /*         // f_B(x) */
      /*         unsigned n = 0; */
      /*         for (auto Element : *B) { */
      /*           const Stmt* S = Element.castAs<CFGStmt>().getStmt(); */
      /*           cloopy::PseudoConstantAnalysis A(S); */
      /*           if (!A.isPseudoConstant(Increment.VD)) { */
      /*             n++; */
      /*           } */
      /*         } */
      /*         Out[B] = std::min(2U, In[B] + n); // (6) */
      /*       } */
      /*     } */
      /*   } */
      /*   if (In[L.Header] < 1) */
      /*       throw checkerror("!ADA_i-ASSIGNED-NotAllPaths"); */
      /*   if (In[L.Header] > 1) */
      /*       throw checkerror("!ADA_i-ASSIGNED-Twice"); */
      /* } */
      /* else { */
      /*   std::cout<<"$$$IRRED?!$$$"<<std::endl; */
      /* } */

      return std::pair<std::string, const ValueDecl*>(Suffix, BoundVar);
    }

  public:
    AdaForLoopClassifier(const ASTContext* Context) : IncrementClassifier("ADA"), Context(Context) {}

};

#endif
