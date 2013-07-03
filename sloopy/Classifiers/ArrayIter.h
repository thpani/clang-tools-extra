#pragma once
#include "IncrementClassifier.h"

class BaseArrayIterClassifier : public IncrementClassifier {
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
    BaseArrayIterClassifier(const std::string Marker, const ASTContext* Context) : IncrementClassifier(Marker), Context(Context) {}
};

class ArrayIterClassifier : public BaseArrayIterClassifier {
  public:
    ArrayIterClassifier(const ASTContext *Context) : BaseArrayIterClassifier("PArrayIter", Context) {}
};

class MultiExitArrayIterClassifier : public BaseArrayIterClassifier {
  protected:
    virtual bool checkPreds(const NaturalLoop *Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      assert(PredSize > 0);
      return true;
    }
  public:
    MultiExitArrayIterClassifier(const ASTContext *Context) : BaseArrayIterClassifier("MultiExitPArrayIter", Context) {}
};

class MultiExitArrayIterIncrSetSizeClassifier : public LoopClassifier {
  public:
  std::set<IncrementLoopInfo> classify(const ASTContext* Context, const NaturalLoop *Loop) {
    const MultiExitArrayIterClassifier AFLC(Context);
    auto IncrementSet = AFLC.classify(Loop);

    std::stringstream sstm;
    sstm << IncrementSet.size();

    LoopClassifier::classify(Loop, Success, "MultiExitPArrayIterIncrSetSize", sstm.str());
    return IncrementSet;
  }
};
