#pragma once
#include "IncrementClassifier.h"

class BaseIntegerIterClassifier : public IncrementClassifier {
  private:
    const ASTContext *Context;

  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isIntegerType);
    }

    std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      return checkIncrementCond(Cond, Increment, &isIntegerType, Context, Marker);
    }

  public:
    BaseIntegerIterClassifier(const std::string Marker, const ASTContext* Context) : IncrementClassifier(Marker), Context(Context) {}
};

ITER_CLASSIFIERS(IntegerIter)
