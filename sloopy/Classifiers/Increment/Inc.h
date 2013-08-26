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
              if (RLHS == VD && isIntegerConstant(RHS->getRHS(), Context)) {
                auto i = getIntegerConstant(RHS->getRHS(), Context);
                if (RHS->getOpcode() == BO_Sub) i = -i;
                IncrementInfo Result = { VD, BOP, VarDeclIntPair(i) };
                return Result;
              }
              const VarDecl *Increment;
              if (RLHS == VD && (Increment = getIntegerVariable(RHS->getRHS()))) {
                IncrementInfo Result = { VD, BOP, Increment };
                return Result;
              }
              if (RHS->getOpcode() == BO_Add) {
                // i = N - i is not increment
                if (RRHS == VD && (Increment = getIntegerVariable(RHS->getLHS()))) {
                  IncrementInfo Result = { VD, BOP, Increment };
                  return Result;
                }
                if (RRHS == VD && isIntegerConstant(RHS->getLHS(), Context)) {
                  auto i = getIntegerConstant(RHS->getLHS(), Context);
                  if (RHS->getOpcode() == BO_Sub) i = -i;
                  IncrementInfo Result = { VD, BOP, VarDeclIntPair(i) };
                  return Result;
                }
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
            auto i = getIntegerConstant(BOP->getRHS(), Context);
            if (BOP->getOpcode() == BO_SubAssign) i = -i;
            IncrementInfo Result = { VD, BOP, VarDeclIntPair(i) };
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
