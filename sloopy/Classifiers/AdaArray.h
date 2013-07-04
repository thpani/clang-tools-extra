#pragma once

#include "IncrementClassifier.h"

class BaseAArrayIterClassifier : public IncrementClassifier {
  private:
    const ASTContext *Context;

  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isIntegerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      std::string Suffix;

      /* COND */
      if (Cond == NULL) {
        throw checkerror(Unknown, Marker, "Cond_None");
      }

      const Expr *CondInner = Cond->IgnoreParenCasts();
      VarDeclIntPair Bound;
      if (const ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(CondInner)) {
        auto A = getArrayVariables(ASE);
        if (A.second != Increment.VD) {
          throw checkerror(Unknown, Marker, "Cond_LoopVar_NotIn_ArraySubscript");
        }
        Bound = { A.first, llvm::APInt() };
      }
      else if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(CondInner)) {
        BinaryOperatorKind Opc = ConditionOp->getOpcode();
        if ((Opc >= BO_Mul && Opc <= BO_Shr) || ConditionOp->isComparisonOp()) {
          const VarDecl *BoundVar;

          // see if LHS/RHS is array var
          bool LoopVarLHS;
          auto LHS = getArrayVariables(ConditionOp->getLHS());
          auto RHS = getArrayVariables(ConditionOp->getRHS());
          const VarDecl *LHSBase = LHS.first;
          const VarDecl *LHSIdx = LHS.second;
          const VarDecl *RHSBase = RHS.first;
          const VarDecl *RHSIdx = RHS.second;

          /* determine which is loop var, which is bound */
          if (LHSBase != NULL && RHSBase != NULL) {
            // both lhs and rhs are vars
            // check which one is subscripted by the loop var
            if (Increment.VD == LHSIdx) {
              LoopVarLHS = true;
            }
            else if (Increment.VD == RHSIdx) {
              LoopVarLHS = false;
            }
            else {
              throw checkerror(Unknown, Marker, "Cond_LoopVar_NotIn_ArraySubscript");
            }
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && isIntegerConstant(ConditionOp->getRHS(), Context)) {
            // LHS is loop var, RHS is bound
            LoopVarLHS = true;
            Bound = { NULL, getIntegerConstant(ConditionOp->getRHS(), Context) };
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && isIntegerConstant(ConditionOp->getLHS(), Context)) {
            // RHS is loop var, LHS is bound
            LoopVarLHS = false;
            Bound = { NULL, getIntegerConstant(ConditionOp->getLHS(), Context) };
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && isVariable(ConditionOp->getRHS())) {
            // LHS is loop var, RHS is var bound
            LoopVarLHS = true;
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && isVariable(ConditionOp->getLHS())) {
            // RHS is loop var, LHS is var bound
            LoopVarLHS = false;
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenCasts())) {
            // LHS is loop var, RHS is sizeof() bound
            if (const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenCasts())) {
              if (B->getKind() == UETT_SizeOf) {
                LoopVarLHS = true;
              }
            }
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenCasts())) {
            // RHS is loop var, LHS is sizeof() bound
            if (const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenCasts())) {
              if (B->getKind() == UETT_SizeOf) {
                LoopVarLHS = false;
              }
            }
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD) {
            LoopVarLHS = true;
            Suffix = "ComplexBound";
            Bound = { NULL, llvm::APInt() };
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD) {
            LoopVarLHS = false;
            Suffix = "ComplexBound";
            Bound = { NULL, llvm::APInt() };
          }
          else {
            throw checkerror(Fail, Marker, "Cond_BinOp_TooComplex");
          }
          BoundVar = LoopVarLHS ? RHSBase : LHSBase; // null if integer-literal, sizeof, ...
          if (BoundVar) Bound = { BoundVar, llvm::APInt() };
        }
        else {
          throw checkerror(Unknown, Marker, "Cond_BinOp_OpNotSupported");
        }
      }
      else {
        throw checkerror(Unknown, Marker, "Cond_OpNotSupported");
      }

      return std::pair<std::string, VarDeclIntPair>(Suffix, Bound);
    }

  public:
    BaseAArrayIterClassifier(const std::string Marker, const ASTContext* Context) : IncrementClassifier(Marker), Context(Context) {}

};

ITER_CLASSIFIERS(AArrayIter)
