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

class IntegerIterClassifier : public BaseIntegerIterClassifier {
  public:
    IntegerIterClassifier(const ASTContext *Context) : BaseIntegerIterClassifier("IntegerIter", Context) {}
};

class MultiExitIntegerIterClassifier : public BaseIntegerIterClassifier {
  protected:
    virtual bool checkPreds(const NaturalLoop *Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      assert(PredSize > 0);
      return true;
    }
  public:
    MultiExitIntegerIterClassifier(const ASTContext *Context) : BaseIntegerIterClassifier("MultiExitIntegerIter", Context) {}
};

class MultiExitIntegerIterIncrSetSizeClassifier : public LoopClassifier {
  public:
  void classify(const ASTContext* Context, const NaturalLoop *Loop) {
    const MultiExitIntegerIterClassifier AFLC(Context);
    auto IncrementSet = AFLC.classify(Loop);

    std::stringstream sstm;
    sstm << IncrementSet.size();

    LoopClassifier::classify(Loop, Success, "MultiExitIntegerIterIncrSetSize", sstm.str());
  }
};
