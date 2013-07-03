#pragma once

class MultiExitIncrSetSizeClassifier : public LoopClassifier {
  const ASTContext* Context;
  void collectIncrementSet(const std::set<IncrementLoopInfo> From, std::set<IncrementLoopInfo> &To) const {
    for (auto ILI : From) {
      To.insert(ILI);
    }
  }
  public:
  MultiExitIncrSetSizeClassifier(const ASTContext* Context) : Context(Context) {}
  std::set<IncrementLoopInfo> classify(const NaturalLoop *SlicedAllLoops) const {
      std::set<IncrementLoopInfo> Result, CombinedSet;
      {
          MultiExitAdaArrayForLoopIncrSetSizeClassifier C;
          Result = C.classify(Context, SlicedAllLoops);
          collectIncrementSet(Result, CombinedSet);
      }
      {
          MultiExitIntegerIterIncrSetSizeClassifier C;
          Result = C.classify(Context, SlicedAllLoops);
          collectIncrementSet(Result, CombinedSet);
      }
      {
          MultiExitDataIterIncrSetSizeClassifier C;
          Result = C.classify(Context, SlicedAllLoops);
          collectIncrementSet(Result, CombinedSet);
      }
      {
          MultiExitArrayIterIncrSetSizeClassifier C;
          Result = C.classify(Context, SlicedAllLoops);
          collectIncrementSet(Result, CombinedSet);
      }

      if (CombinedSet.size() == 0) {
        LoopClassifier::classify(SlicedAllLoops, Success, "MultiExitNonSimple");
      } else {
        LoopClassifier::classify(SlicedAllLoops, Success, "MultiExitSimple");
      }

      std::stringstream sstm;
      sstm << CombinedSet.size();

      LoopClassifier::classify(SlicedAllLoops, Success, "MultiExitIncrSetSize", sstm.str());

      std::set<const VarDecl*> Counters;
      for (auto ILI : CombinedSet) {
        Counters.insert(ILI.VD);
      }

      sstm.str(std::string());
      sstm << Counters.size();

      LoopClassifier::classify(SlicedAllLoops, Success, "MultiExitCounters", sstm.str());

      return CombinedSet;
    }
};
