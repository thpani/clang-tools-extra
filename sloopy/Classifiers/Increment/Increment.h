#pragma once

#include "Inc.h"
#include "Cond.h"
#include "IncrementClassifier.h"

class IntegerIterClassifier : public IncrementClassifier {
  protected:
    boost::variant<std::string, IncrementInfo> getIncrementInfo(const Stmt *Stmt) const throw () {
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
    boost::variant<std::string, IncrementInfo> getIncrementInfo(const Stmt *Stmt) const throw () {
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
    boost::variant<std::string, IncrementInfo> getIncrementInfo(const Stmt *Stmt) const throw () {
      if (Stmt == NULL) return "Inc_None";
      const Expr *E = dyn_cast<Expr>(Stmt);
      if (E == NULL) return "Inc_NotExpr";
      const Expr *Expression = E->IgnoreParenCasts();
      const BinaryOperator *BO;
      if (!(BO = dyn_cast<BinaryOperator>(Expression))) {
        return "Inc_NotBinary";
      }
      const VarDecl *IncVar;
      if (!(IncVar = getVariable(BO->getLHS()))) {
        return "Inc_LHSNoVar";
      }
      if (!(*IncVar->getType()).isPointerType()) {
        return "Inc_LHSNoPtr";
      }
      const MemberExpr *RHS;
      if (!(RHS = dyn_cast<MemberExpr>(BO->getRHS()->IgnoreParenCasts()))) {
        return "Inc_RHSNoMemberExpr";
      }
      const VarDecl *Base;
      if (!(Base = getVariable(RHS->getBase()))) {
        return "Inc_RHSBaseNoVar";
      }
      if (Base != IncVar) {
        return "Inc_RHSBaseNeqInc";
      }
      IncrementInfo Result = { IncVar, BO, VarDeclIntPair() };
      return Result;
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      if (Cond == NULL) throw checkerror("Cond_None");
      const Expr *Expression = Cond->IgnoreParenCasts();
      if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Expression)) {
        const VarDecl *VD = getVariable(DRE);
        if (VD != Increment.VD)
          throw checkerror("Cond_VD_Neq_IncVD");
        return std::make_pair(std::string(), VarDeclIntPair());
      } else if (const MemberExpr *ME = dyn_cast<MemberExpr>(Expression)) {
        const VarDecl *Base;
        if (!(Base = getVariable(ME->getBase()))) {
          throw checkerror("Cond_RHSBaseNoVar");
        }
        if (Base != Increment.VD) {
          throw checkerror("Cond_BaseVD_Neq_IncVD");
        }
        return std::make_pair(std::string(), VarDeclIntPair());
      } else {
        throw checkerror("Cond_UnhandledStmt");
      }
    }

  public:
    DataIterClassifier(const ASTContext *Context) : IncrementClassifier("DataIter", Context) {}
};

class AArrayIterClassifier : public IncrementClassifier {
  protected:
    boost::variant<std::string, IncrementInfo> getIncrementInfo(const Stmt *Stmt) const throw () {
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
