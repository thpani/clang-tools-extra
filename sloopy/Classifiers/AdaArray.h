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
      const VarDeclIntPair *Bound = NULL;
      if (const ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(CondInner)) {
        auto A = getArrayVariables(ASE);
        if (A.second != Increment.VD) {
          throw checkerror(Unknown, Marker, "Cond_LoopVar_NotIn_ArraySubscript");
        }
        Bound = new VarDeclIntPair(A.first);
      }
      else if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(CondInner)) {
        BinaryOperatorKind Opc = ConditionOp->getOpcode();
        if ((Opc >= BO_Mul && Opc <= BO_Shr) || ConditionOp->isComparisonOp()) {
          const VarDecl *BoundVar;

          // see if LHS/RHS is array var
          bool LoopVarLHS;
          auto LHS = ConditionOp->getLHS();
          auto RHS = ConditionOp->getRHS();
          auto LHSVar = getArrayVariables(LHS);
          auto RHSVar = getArrayVariables(RHS);
          const VarDecl *LHSBase = LHSVar.first;
          const VarDecl *LHSIdx = LHSVar.second;
          const VarDecl *RHSBase = RHSVar.first;
          const VarDecl *RHSIdx = RHSVar.second;

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
          else if (LHSBase != NULL && LHSIdx == Increment.VD && isIntegerConstant(RHS, Context)) {
            // LHS is loop var, RHS is bound
            LoopVarLHS = true;
            Bound = new VarDeclIntPair(getIntegerConstant(RHS, Context));
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && isIntegerConstant(LHS, Context)) {
            // RHS is loop var, LHS is bound
            LoopVarLHS = false;
            Bound = new VarDeclIntPair(getIntegerConstant(LHS, Context));
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && getVariable(RHS)) {
            // LHS is loop var, RHS is var bound
            LoopVarLHS = true;
            Bound = new VarDeclIntPair(getVariable(RHS));
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && getVariable(LHS)) {
            // RHS is loop var, LHS is var bound
            LoopVarLHS = false;
            Bound = new VarDeclIntPair(getVariable(LHS));
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && isa<UnaryExprOrTypeTraitExpr>(RHS->IgnoreParenCasts())) {
            // LHS is loop var, RHS is sizeof() bound
            const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(RHS->IgnoreParenCasts());
            if (B->getKind() != UETT_SizeOf) {
              LoopVarLHS = true;
            }
            Bound = new VarDeclIntPair();
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && isa<UnaryExprOrTypeTraitExpr>(LHS->IgnoreParenCasts())) {
            // RHS is loop var, LHS is sizeof() bound
            const UnaryExprOrTypeTraitExpr *B = dyn_cast<UnaryExprOrTypeTraitExpr>(LHS->IgnoreParenCasts());
            if (B->getKind() != UETT_SizeOf) {
              LoopVarLHS = false;
            }
            Bound = new VarDeclIntPair();
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD) {
            LoopVarLHS = true;
            Suffix = "ComplexBound";
            Bound = new VarDeclIntPair();
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD) {
            LoopVarLHS = false;
            Suffix = "ComplexBound";
            Bound = new VarDeclIntPair();
          }
          else {
            throw checkerror(Fail, Marker, "Cond_BinOp_TooComplex");
          }
          BoundVar = LoopVarLHS ? RHSBase : LHSBase; // null if integer-literal, sizeof, ...
          if (BoundVar) Bound = new VarDeclIntPair(BoundVar);
        }
        else {
          throw checkerror(Unknown, Marker, "Cond_BinOp_OpNotSupported");
        }
      }
      else {
        throw checkerror(Unknown, Marker, "Cond_OpNotSupported");
      }

      assert(Bound);
      const VarDeclIntPair BoundReturn(*Bound);
      delete Bound;
      return std::pair<std::string, VarDeclIntPair>(Suffix, BoundReturn);
    }

  public:
    BaseAArrayIterClassifier(const std::string Marker, const ASTContext* Context) : IncrementClassifier(Marker), Context(Context) {}

};

ITER_CLASSIFIERS(AArrayIter)
