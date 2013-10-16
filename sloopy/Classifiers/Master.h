#pragma once

#include "Increment/Increment.h"

class MasterIncrementClassifier : public LoopClassifier {
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
      IntegerIterClassifier(Context),
      AArrayIterClassifier(Context),
      PArrayIterClassifier(Context),
      DataIterClassifier(Context) {}
    std::set<IncrementLoopInfo> classify(const NaturalLoop *Loop, const IncrementClassifierConstraint Constr) const {
      std::set<IncrementLoopInfo> Result, CombinedSet;

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

class MasterProvingClassifier : public LoopClassifier {
  const IntegerIterClassifier IntegerIterClassifier;
  const AArrayIterClassifier AArrayIterClassifier;
  const PArrayIterClassifier PArrayIterClassifier;
  const DataIterClassifier DataIterClassifier;

  void collectIncrementSet(
      const std::pair<std::set<const NaturalLoopBlock*>, std::map<const NaturalLoopBlock*, llvm::BitVector>> &From,
      std::set<const NaturalLoopBlock*> &ToBlocks,
      std::map<const NaturalLoopBlock*, llvm::BitVector> &ToBV) const {
    for (auto Block : From.first) {
      ToBlocks.insert(Block);
    }
    for (auto KV : From.second) {
      ToBV[KV.first] = KV.second;
    }
  }

  const NaturalLoopBlock * someTermCondOnEachPath(const NaturalLoop *L, std::set<const NaturalLoopBlock*> ProvablyTerminatingBlocks) const throw () {
    const NaturalLoopBlock *Header = *L->getEntry().succ_begin();

    std::map<const NaturalLoopBlock *, bool> In, Out, OldOut;

    // Initialize all blocks    (1) + (2)
    for (auto Block : *L) {
      Out[Block] = ProvablyTerminatingBlocks.count(Block);
    }

    // while OUT changes
    while (Out != OldOut) {     // (3)
      OldOut = Out;
      // for each basic block other than entry
      for (auto Block : *L) {   // (4)

        // meet (propagate OUT -> IN)
        bool meet = false;
        for (NaturalLoopBlock::const_pred_iterator P = Block->pred_begin(),
                                                    E = Block->pred_end();
                                                    P != E; P++) {    // (5)
          const NaturalLoopBlock *Pred = *P;
          meet = meet or Out[Pred];
        }
        In[Block] = meet;

        if (Block == Header) continue;

        // compute OUT / f_B(x)
        Out[Block] = ProvablyTerminatingBlocks.count(Block) or In[Block];   // (6)
      }
    }

    if (In[Header]) {
      return *ProvablyTerminatingBlocks.begin();
    }
    return nullptr;
  }

  const NaturalLoopBlock * termCondOnEachPath(const NaturalLoop *L, std::set<const NaturalLoopBlock*> ProvablyTerminatingBlocks) const throw () {
    const NaturalLoopBlock *Header = *L->getEntry().succ_begin();

    for (auto ProvablyTerminatingBlock : ProvablyTerminatingBlocks) {

      std::map<const NaturalLoopBlock *, std::pair<unsigned long, bool>> In, Out, OldOut;

      // Initialize all blocks    (1) + (2)
      for (auto Block : *L) {
        Out[Block] = { 0, false };
      }

      // while OUT changes
      while (Out != OldOut) {     // (3)
        OldOut = Out;
        // for each basic block other than entry
        for (auto Block : *L) {   // (4)
          if (not Block->pred_size()) {
            continue;
          }

          // meet (propagate OUT -> IN)
          bool meet = true;
          unsigned long min = std::numeric_limits<unsigned long>::max();
          for (NaturalLoopBlock::const_pred_iterator P = Block->pred_begin(),
                                                    E = Block->pred_end();
                                                    P != E; P++) {    // (5)
            const NaturalLoopBlock *Pred = *P;
            if (Pred == &L->getEntry()) continue;
            meet = meet and Out[Pred].second;
            min = Out[Pred].first < min ? Out[Pred].first : min;
          }
          In[Block] = { min, meet };

          if (Block == Header) {
            Out[Block] = { Block->succ_size() ? 1 : 0, Block == ProvablyTerminatingBlock };
          } else {
            // compute OUT / f_B(x)
            /* llvm::errs() << Block->getBlockID() << ": " << min << "\n"; */
            assert(min < std::numeric_limits<unsigned long>::max() && "overflow");
            Out[Block] = { Block->succ_size() ? min+1 : min,
              (min == In[Header].first and Block == ProvablyTerminatingBlock) or In[Block].second
            };   // (6)
          }
        }
      }

      if (In[Header].second) {
        return ProvablyTerminatingBlock;
      }
    }

    return nullptr;
  }

  public:
    MasterProvingClassifier(const ASTContext* Context) :
      IntegerIterClassifier(Context),
      AArrayIterClassifier(Context),
      PArrayIterClassifier(Context),
      DataIterClassifier(Context) {}

    void classify(const NaturalLoop *Loop, const SimpleLoopConstraint Constr) const {
      std::set<const NaturalLoopBlock *> ProvablyTerminatingBlocks;
      std::map<const NaturalLoopBlock *, llvm::BitVector> AssumptionMap;

      const NaturalLoopBlock *Proved = nullptr;

      if (Constr.ExitCountConstr == SINGLE_EXIT and
          Loop->getExit().pred_size() != 1) {
        
        Proved = nullptr;

      } else {

        auto Result = IntegerIterClassifier.classifyProve(Loop, Constr.FormConstr == ASSUME_IMPLIES);
        collectIncrementSet(Result, ProvablyTerminatingBlocks, AssumptionMap);

      Result = AArrayIterClassifier.classifyProve(Loop);
        collectIncrementSet(Result, ProvablyTerminatingBlocks, AssumptionMap);

      Result = PArrayIterClassifier.classifyProve(Loop);
        collectIncrementSet(Result, ProvablyTerminatingBlocks, AssumptionMap);

      Result = DataIterClassifier.classifyProve(Loop);
        collectIncrementSet(Result, ProvablyTerminatingBlocks, AssumptionMap);

        switch (Constr.ControlFlowConstr) {
          case SINGLETON:
            Proved = termCondOnEachPath(Loop, ProvablyTerminatingBlocks);
            break;
          case SOME_EACH:
            Proved = termCondOnEachPath(Loop, ProvablyTerminatingBlocks);
            break;
          case SOME_SOME:
            Proved = nullptr; // TODO
            break;
        }
      }

      LoopClassifier::classify(Loop, Constr.str(), Proved!=nullptr);
      if (Proved and Constr.isSyntTerm()) {
        auto Assumption = AssumptionMap[Proved];
        LoopClassifier::classify(Loop, Constr.str()+"WithoutAssumptions", Assumption.none());
        LoopClassifier::classify(Loop, Constr.str()+"WithAssumptionWrapv", Assumption[0]);
        LoopClassifier::classify(Loop, Constr.str()+"WithAssumptionLeBoundNotMax", Assumption[1]);
        LoopClassifier::classify(Loop, Constr.str()+"WithAssumptionGeBoundNotMin", Assumption[2]);
        LoopClassifier::classify(Loop, Constr.str()+"WithAssumptionMNeq0", Assumption[3]);
        LoopClassifier::classify(Loop, Constr.str()+"WithAssumptionWrapvOrRunsInto", Assumption[4]);
        LoopClassifier::classify(Loop, Constr.str()+"WithAssumptionRightArrayContent", Assumption[5]);
      }
    }
};
