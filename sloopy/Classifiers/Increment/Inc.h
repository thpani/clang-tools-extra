#pragma once

#include "ADT.h"
#include "Helpers.h"

static IncrementInfo getIncrementInfo(const Expr *Expr, const std::string Marker, const ASTContext *Context, const TypePredicate TypePredicate) throw(checkerror) {
  if (Expr == NULL) throw checkerror("Inc_None");
  const class Expr *Expression = Expr->IgnoreParenCasts();
  if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Expression)) {
    // i{++,--}
    if (UOP->isIncrementDecrementOp()) {
      if (const VarDecl *VD = getVariable(UOP->getSubExpr())) {
        if (TypePredicate(VD)) {
          return { VD, UOP, VarDeclIntPair(llvm::APInt(2, UOP->isIncrementOp() ? 1 : -1, true)) };
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
                return { VD, BOP, VarDeclIntPair(getIntegerConstant(RHS->getLHS(), Context)) };
              }
              if (RLHS == VD && isIntegerConstant(RHS->getRHS(), Context)) {
                return { VD, BOP, VarDeclIntPair(getIntegerConstant(RHS->getRHS(), Context)) };
              }
              const VarDecl *Increment;
              if (RRHS == VD && (Increment = getIntegerVariable(RHS->getLHS()))) {
                return { VD, BOP, Increment };
              }
              if (RLHS == VD && (Increment = getIntegerVariable(RHS->getRHS()))) {
                return { VD, BOP, Increment };
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
            return { VD, BOP, VarDeclIntPair(getIntegerConstant(BOP->getRHS(), Context)) };
          }
          else if (const VarDecl *Delta = getIntegerVariable(BOP->getRHS())) {
            return { VD, BOP, VarDeclIntPair(Delta) };
          }
        }
      }
    }
  }
  throw checkerror("Inc_NotValid");
}
