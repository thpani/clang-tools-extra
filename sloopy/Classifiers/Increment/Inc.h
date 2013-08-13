#pragma once

#include "ADT.h"
#include "Helpers.h"

static boost::variant<std::string, IncrementInfo> getIncrementInfo(const Stmt *Stmt, const std::string Marker, const ASTContext *Context, const TypePredicate TypePredicate) throw() {
  if (Stmt == NULL) return "Inc_None";
  const Expr *E = dyn_cast<Expr>(Stmt);
  if (E == NULL) return "Inc_NotExpr";
  const Expr *Expression = E->IgnoreParenCasts();
  if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Expression)) {
    // i{++,--}
    if (UOP->isIncrementDecrementOp()) {
      if (const VarDecl *VD = getVariable(UOP->getSubExpr())) {
        if (TypePredicate(VD)) {
          IncrementInfo Result = { VD, UOP, VarDeclIntPair(llvm::APInt(2, UOP->isIncrementOp() ? 1 : -1, true)) };
          return Result;
        }
      }
    }
  }
  else if (const BinaryOperator *BOP = dyn_cast<BinaryOperator>(Expression)) {
    // i = i {+-} <int>
    // i = <int> {+-} i
    if (BOP->getOpcode() == BO_Assign) {
      if (const VarDecl *VD = getVariable(BOP->getLHS())) {
        if (TypePredicate(VD)) {
          if (const BinaryOperator *RHS = dyn_cast<BinaryOperator>(BOP->getRHS()->IgnoreParenCasts())) {
            if (RHS->isAdditiveOp()) {
              const VarDecl *RRHS = getVariable(RHS->getRHS());
              const VarDecl *RLHS = getVariable(RHS->getLHS());
              if (RRHS == VD && isIntegerConstant(RHS->getLHS(), Context)) {
                IncrementInfo Result = { VD, BOP, VarDeclIntPair(getIntegerConstant(RHS->getLHS(), Context)) };
                return Result;
              }
              if (RLHS == VD && isIntegerConstant(RHS->getRHS(), Context)) {
                IncrementInfo Result = { VD, BOP, VarDeclIntPair(getIntegerConstant(RHS->getRHS(), Context)) };
                return Result;
              }
              const VarDecl *Increment;
              if (RRHS == VD && (Increment = getIntegerVariable(RHS->getLHS()))) {
                IncrementInfo Result = { VD, BOP, Increment };
                return Result;
              }
              if (RLHS == VD && (Increment = getIntegerVariable(RHS->getRHS()))) {
                IncrementInfo Result = { VD, BOP, Increment };
                return Result;
              }
            }
          }
        }
      }
    }
    // i {+-}= <int>
    else if (BOP->getOpcode() == BO_AddAssign ||
              BOP->getOpcode() == BO_SubAssign ) {
      if (const VarDecl *VD = getVariable(BOP->getLHS())) {
        if (TypePredicate(VD)) {
          if (isIntegerConstant(BOP->getRHS(), Context)) {
            IncrementInfo Result = { VD, BOP, VarDeclIntPair(getIntegerConstant(BOP->getRHS(), Context)) };
            return Result;
          }
          else if (const VarDecl *Delta = getIntegerVariable(BOP->getRHS())) {
            IncrementInfo Result = { VD, BOP, VarDeclIntPair(Delta) };
            return Result;
          }
        }
      }
    }
  }
  return "Inc_NotValid";
}
