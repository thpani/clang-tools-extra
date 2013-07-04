#pragma once

#include "IncrementClassifier.h"

template <class BaseClassifier, typename Marker>
class BaseSingleExitClassifier : public BaseClassifier {
  public:
    BaseSingleExitClassifier(const ASTContext *Context) : BaseClassifier(Marker::asString(), Context) {}
};

template <class BaseClassifier, typename Marker>
class BaseMultiExitClassifier : public BaseClassifier {
  protected:
    virtual bool checkPreds(const NaturalLoop *Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      assert(PredSize > 0);
      return true;
    }
  public:
    BaseMultiExitClassifier(const ASTContext *Context) : BaseClassifier(Marker::asString(), Context) {}
};

template<class BaseClassifier, typename Marker>
class BaseMultiExitNoCondClassifier : public BaseMultiExitClassifier<BaseClassifier, Marker> {
  protected:
    virtual std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo I) const throw (checkerror) {
      return std::make_pair("", VarDeclIntPair::create());
    }
  public:
    BaseMultiExitNoCondClassifier(const ASTContext *Context) : BaseMultiExitClassifier<BaseClassifier, Marker>(Context) {}
};

template <class MultiExitClassifier, typename Marker>
class BaseMultiExitIncrSetSizeClassifier : public LoopClassifier {
  public:
  std::set<IncrementLoopInfo> classify(const ASTContext* Context, const NaturalLoop *Loop) {
    const MultiExitClassifier AFLC(Context);
    auto IncrementSet = AFLC.classify(Loop);

    std::stringstream sstm;
    sstm << IncrementSet.size();

    LoopClassifier::classify(Loop, Success, Marker::asString(), sstm.str());
    return IncrementSet;
  }
};

#define STR_HOLDER(str) struct { static std::string asString() { return str; } }

#define ITER_CLASSIFIERS(name) \
  typedef STR_HOLDER(#name) Str_##name; \
  typedef STR_HOLDER("MultiExit"#name) Str_MultiExit##name; \
  typedef STR_HOLDER("MultiExitNoCond"#name) Str_MultiExitNoCond##name; \
  typedef STR_HOLDER("MultiExit"#name"IncrSetSize") Str_MultiExit##name##IncrSetSize; \
  typedef BaseSingleExitClassifier<Base##name##Classifier, Str_##name> name##Classifier; \
  typedef BaseMultiExitClassifier<Base##name##Classifier, Str_MultiExit##name> MultiExit##name##Classifier; \
  typedef BaseMultiExitNoCondClassifier<Base##name##Classifier, Str_MultiExitNoCond##name> MultiExitNoCond##name##Classifier; \
  typedef BaseMultiExitIncrSetSizeClassifier<MultiExit##name##Classifier, Str_MultiExit##name##IncrSetSize> MultiExit##name##IncrSetSizeClassifier;
