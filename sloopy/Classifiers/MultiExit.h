#pragma once

template <class IntegerIterClassifier,
          class AArrayIterClassifier,
          class PArrayIterClassifier,
          class DataIterClassifier,
          typename Marker>
class BaseClassifier : public LoopClassifier {
  const ASTContext* Context;
  void collectIncrementSet(const std::set<IncrementLoopInfo> From, std::set<IncrementLoopInfo> &To) const {
    for (auto ILI : From) {
      To.insert(ILI);
    }
  }
  public:
  BaseClassifier(const ASTContext* Context) : Context(Context) {}
  std::set<IncrementLoopInfo> classify(const NaturalLoop *Loop) const {
      std::set<IncrementLoopInfo> Result, CombinedSet;
      {
          IntegerIterClassifier C(Context);
          Result = C.classify(Loop);
          collectIncrementSet(Result, CombinedSet);
      }
      {
          AArrayIterClassifier C(Context);
          Result = C.classify(Loop);
          collectIncrementSet(Result, CombinedSet);
      }
      {
          PArrayIterClassifier C(Context);
          Result = C.classify(Loop);
          collectIncrementSet(Result, CombinedSet);
      }
      {
          DataIterClassifier C(Context);
          Result = C.classify(Loop);
          collectIncrementSet(Result, CombinedSet);
      }

      if (CombinedSet.size() == 0) {
        LoopClassifier::classify(Loop, Success, Marker::asString()+"NonSimple");
      } else {
        LoopClassifier::classify(Loop, Success, Marker::asString()+"Simple");
      }

      std::stringstream sstm;
      /* sstm << CombinedSet.size(); */

      /* LoopClassifier::classify(Loop, Success, Marker::asString()+"Counters", sstm.str()); */

      std::set<const VarDecl*> Counters;
      for (auto ILI : CombinedSet) {
        Counters.insert(ILI.VD);
      }

      sstm.str(std::string());
      sstm << Counters.size();

      LoopClassifier::classify(Loop, Success, Marker::asString()+"Counters", sstm.str());

      return CombinedSet;
    }
};

typedef STR_HOLDER("MultiExit") Str_MultiExit;
typedef BaseClassifier<
  MultiExitIntegerIterClassifier,
  MultiExitAArrayIterClassifier,
  MultiExitPArrayIterClassifier,
  MultiExitDataIterClassifier,
  Str_MultiExit>
  MultiExitClassifier;

typedef STR_HOLDER("MultiExitNoCond") Str_MultiExitNoCond;
typedef BaseClassifier<
  MultiExitNoCondIntegerIterClassifier,
  MultiExitNoCondAArrayIterClassifier,
  MultiExitNoCondPArrayIterClassifier,
  MultiExitNoCondDataIterClassifier,
  Str_MultiExitNoCond>
  MultiExitNoCondClassifier;
