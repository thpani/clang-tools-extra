#pragma once

#include "Increment/Increment.h"

class MasterIncrementClassifier : public LoopClassifier {
  const ExitClassifier ExitClassifier;
  const IntegerIterClassifier IntegerIterClassifier;
  const AArrayIterClassifier AArrayIterClassifier;
  const PArrayIterClassifier PArrayIterClassifier;
  const DataIterClassifier DataIterClassifier;

  void collectIncrementSet(const std::set<IncrementLoopInfo> From, std::set<IncrementLoopInfo> &To) const {
    for (auto ILI : From) {
      To.insert(ILI);
    }
  }
  public:
    MasterIncrementClassifier(const ASTContext* Context) :
      ExitClassifier(),
      IntegerIterClassifier(Context),
      AArrayIterClassifier(Context),
      PArrayIterClassifier(Context),
      DataIterClassifier(Context) {}
    std::set<IncrementLoopInfo> classify(const NaturalLoop *Loop, const IncrementClassifierConstraint Constr) const {
      std::set<IncrementLoopInfo> Result, CombinedSet;

      ExitClassifier.classify(Loop);

      Result = IntegerIterClassifier.classify(Loop, Constr);
      collectIncrementSet(Result, CombinedSet);

      Result = AArrayIterClassifier.classify(Loop, Constr);
      collectIncrementSet(Result, CombinedSet);

      Result = PArrayIterClassifier.classify(Loop, Constr);
      collectIncrementSet(Result, CombinedSet);

      Result = DataIterClassifier.classify(Loop, Constr);
      collectIncrementSet(Result, CombinedSet);

      const bool IsSimple = CombinedSet.size() > 0;
      LoopClassifier::classify(Loop, Constr.str(), "Simple", IsSimple);

      std::set<const VarDecl*> Counters;
      for (auto ILI : CombinedSet) {
        Counters.insert(ILI.VD);
      }

      const unsigned CounterSetSize = Counters.size();
      LoopClassifier::classify(Loop, Constr.str(), "Counters", CounterSetSize);

      return CombinedSet;
    }
};

