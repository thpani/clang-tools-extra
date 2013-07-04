#pragma once
#include "IncrementClassifier.h"

class BasePArrayIterClassifier : public IncrementClassifier {
  private:
    const ASTContext *Context;

  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isPointerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      return checkIncrementCond(Cond, Increment, &isPointerType, Context, Marker);
    }

  public:
    BasePArrayIterClassifier(const std::string Marker, const ASTContext* Context) : IncrementClassifier(Marker), Context(Context) {}
};

ITER_CLASSIFIERS(PArrayIter)
