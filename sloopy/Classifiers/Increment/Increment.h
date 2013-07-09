#pragma once

#include "Inc.h"
#include "Cond.h"
#include "IncrementClassifier.h"

class IntegerIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isIntegerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      return checkIncrementCond(Cond, Increment, &isIntegerType, Context, Marker);
    }

  public:
    IntegerIterClassifier(const ASTContext *Context) : IncrementClassifier("IntegerIter", Context) {}
};

class PArrayIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isPointerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      return checkIncrementCond(Cond, Increment, &isPointerType, Context, Marker);
    }

  public:
    PArrayIterClassifier(const ASTContext *Context) : IncrementClassifier("PArrayIter", Context) {}
};

class DataIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      if (Expr == NULL) throw checkerror(Fail, Marker, "Inc_None");
      const class Expr *Expression = Expr->IgnoreParenCasts();
      const BinaryOperator *BO;
      if (!(BO = dyn_cast<BinaryOperator>(Expression))) {
        throw checkerror(Fail, Marker, "Inc_NotBinary");
      }
      const VarDecl *IncVar;
      if (!(IncVar = getVariable(BO->getLHS()))) {
        throw checkerror(Fail, Marker, "Inc_LHSNoVar");
      }
      if (!(*IncVar->getType()).isPointerType()) {
        throw checkerror(Fail, Marker, "Inc_LHSNoPtr");
      }
      const MemberExpr *RHS;
      if (!(RHS = dyn_cast<MemberExpr>(BO->getRHS()->IgnoreParenCasts()))) {
        throw checkerror(Fail, Marker, "Inc_RHSNoMemberExpr");
      }
      const VarDecl *Base;
      if (!(Base = getVariable(RHS->getBase()))) {
        throw checkerror(Fail, Marker, "Inc_RHSBaseNoVar");
      }
      if (Base != IncVar) {
        throw checkerror(Fail, Marker, "Inc_RHSBaseNeqInc");
      }
      return { IncVar, BO, VarDeclIntPair() };
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      // TODO
      return std::make_pair(std::string(), VarDeclIntPair());
    }

  public:
    DataIterClassifier(const ASTContext *Context) : IncrementClassifier("DataIter", Context) {}
};

class AArrayIterClassifier : public IncrementClassifier {
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
    AArrayIterClassifier(const ASTContext *Context) : IncrementClassifier("AArrayIter", Context) {}

};
