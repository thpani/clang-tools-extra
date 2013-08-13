#pragma once

#include "Inc.h"
#include "Cond.h"
#include "IncrementClassifier.h"

class IntegerIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Stmt *Stmt) const throw (checkerror) {
      return ::getIncrementInfo(Stmt, Marker, Context, &isIntegerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      return checkIncrementCond(Cond, Increment, &isIntegerType, Context, Marker);
    }

  public:
    IntegerIterClassifier(const ASTContext *Context) : IncrementClassifier("IntegerIter", Context) {}
};

class PArrayIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Stmt *Stmt) const throw (checkerror) {
      return ::getIncrementInfo(Stmt, Marker, Context, &isPointerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      return checkIncrementCond(Cond, Increment, &isPointerType, Context, Marker);
    }

  public:
    PArrayIterClassifier(const ASTContext *Context) : IncrementClassifier("PArrayIter", Context) {}
};

class DataIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Stmt *Stmt) const throw (checkerror) {
      if (Stmt == NULL) throw checkerror("Inc_None");
      const Expr *E = dyn_cast<Expr>(Stmt);
      if (E == NULL) throw checkerror("Inc_NotExpr");
      const Expr *Expression = E->IgnoreParenCasts();
      const BinaryOperator *BO;
      if (!(BO = dyn_cast<BinaryOperator>(Expression))) {
        throw checkerror("Inc_NotBinary");
      }
      const VarDecl *IncVar;
      if (!(IncVar = getVariable(BO->getLHS()))) {
        throw checkerror("Inc_LHSNoVar");
      }
      if (!(*IncVar->getType()).isPointerType()) {
        throw checkerror("Inc_LHSNoPtr");
      }
      const MemberExpr *RHS;
      if (!(RHS = dyn_cast<MemberExpr>(BO->getRHS()->IgnoreParenCasts()))) {
        throw checkerror("Inc_RHSNoMemberExpr");
      }
      const VarDecl *Base;
      if (!(Base = getVariable(RHS->getBase()))) {
        throw checkerror("Inc_RHSBaseNoVar");
      }
      if (Base != IncVar) {
        throw checkerror("Inc_RHSBaseNeqInc");
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
    IncrementInfo getIncrementInfo(const Stmt *Stmt) const throw (checkerror) {
      return ::getIncrementInfo(Stmt, Marker, Context, &isIntegerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      std::string Suffix;

      /* COND */
      if (Cond == NULL) {
        throw checkerror("Cond_None");
      }

      const Expr *CondInner = Cond->IgnoreParenCasts();
      const VarDeclIntPair *Bound = NULL;
      if (const ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(CondInner)) {
        auto A = getArrayVariables(ASE);
        if (A.second != Increment.VD) {
          throw checkerror("Cond_LoopVar_NotIn_ArraySubscript");
        }
        Bound = new VarDeclIntPair(A.first);
      }
      else if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(CondInner)) {
        BinaryOperatorKind Opc = ConditionOp->getOpcode();
        if ((Opc >= BO_Mul && Opc <= BO_Shr) || ConditionOp->isComparisonOp()) {
          // see if LHS/RHS is array var
          auto LHS = ConditionOp->getLHS();
          auto RHS = ConditionOp->getRHS();
          auto LHSVar = getArrayVariables(LHS);
          auto RHSVar = getArrayVariables(RHS);
          const VarDecl *LHSBase = LHSVar.first;
          const VarDecl *LHSIdx = LHSVar.second;
          const VarDecl *RHSBase = RHSVar.first;
          const VarDecl *RHSIdx = RHSVar.second;

          /* determine which is loop var, which is bound */
          if (LHSBase != NULL && LHSIdx == Increment.VD && getVariable(RHS)) {
            // LHS is loop var, RHS is var bound
            Bound = new VarDeclIntPair(getVariable(RHS));
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && getVariable(LHS)) {
            // RHS is loop var, LHS is var bound
            Bound = new VarDeclIntPair(getVariable(LHS));
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && isIntegerConstant(RHS, Context)) {
            // LHS is loop var, RHS is bound
            Bound = new VarDeclIntPair(getIntegerConstant(RHS, Context));
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && isIntegerConstant(LHS, Context)) {
            // RHS is loop var, LHS is bound
            Bound = new VarDeclIntPair(getIntegerConstant(LHS, Context));
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD && isa<UnaryExprOrTypeTraitExpr>(RHS->IgnoreParenCasts())) {
            // LHS is loop var, RHS is sizeof/alignof() bound
            Bound = new VarDeclIntPair();
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD && isa<UnaryExprOrTypeTraitExpr>(LHS->IgnoreParenCasts())) {
            // RHS is loop var, LHS is sizeof/alignof() bound
            Bound = new VarDeclIntPair();
          }
          else if (LHSBase != NULL && LHSIdx == Increment.VD) {
            Suffix = "ComplexBound";
            Bound = new VarDeclIntPair();
          }
          else if (RHSBase != NULL && RHSIdx == Increment.VD) {
            Suffix = "ComplexBound";
            Bound = new VarDeclIntPair();
          }
          else {
            throw checkerror("Cond_BinOp_TooComplex");
          }
        }
        else {
          throw checkerror("Cond_BinOp_OpNotSupported");
        }
      }
      else {
        throw checkerror("Cond_OpNotSupported");
      }

      assert(Bound);
      return std::pair<std::string, VarDeclIntPair>(Suffix, *std::unique_ptr<const VarDeclIntPair>(Bound).get());
    }

  public:
    AArrayIterClassifier(const ASTContext *Context) : IncrementClassifier("AArrayIter", Context) {}

};
