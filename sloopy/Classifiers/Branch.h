#pragma once

#include "IncrementClassifier.h"

class ExitClassifier : public LoopClassifier {
  public:
    void classify(const NaturalLoop* Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      LoopClassifier::classify(Loop, Success, "Exits", PredSize);
      if (PredSize == 1) {
        LoopClassifier::classify(Loop, "SingleExit");
      } else {
        LoopClassifier::classify(Loop, "NonSingleExit");
      }
      LoopClassifier::classify(Loop, "MultiExit");
    }
};

class BranchingClassifier : public LoopClassifier {
  const std::string SliceType;
  public:
    BranchingClassifier(const std::string SliceType) : SliceType(SliceType) {}
    void classify(const NaturalLoop *Loop) {
      std::set<const NaturalLoopBlock*> Visited;
      std::stack<const NaturalLoopBlock*> Worklist;
      std::stack<unsigned> Depths;

      Worklist.push(&Loop->getEntry());
      Depths.push(0);

      unsigned depth = 0;
      unsigned nodes = 0;

      while (Worklist.size() > 0) {
        const NaturalLoopBlock *Current = Worklist.top();
        Worklist.pop();
        unsigned CurrentDepth = Depths.top();
        Depths.pop();

        if (Visited.count(Current) > 0) continue;
        Visited.insert(Current);

        if (Current != &Loop->getEntry() && Current != &Loop->getExit()) {
          nodes++;
          if (CurrentDepth > depth) depth = CurrentDepth;
        }

        for (NaturalLoopBlock::const_succ_iterator I = Current->succ_begin(),
                                                   E = Current->succ_end();
                                                   I != E; I++) {
          const NaturalLoopBlock* Succ = *I;
          Worklist.push(Succ);
          Depths.push(CurrentDepth+1);
        }
      }
      if (depth>0) depth--;

      std::stringstream sstm;
      sstm << "Depth-" << depth << "-Nodes-" << nodes;

      LoopClassifier::classify(Loop, Success, SliceType+"Branch", sstm.str());
      return;
    }
};

class ControlVarClassifier : public LoopClassifier {
  const std::string SliceType;
  public:
    ControlVarClassifier(const std::string SliceType) : SliceType(SliceType) {}
    void classify(const NaturalLoop *Loop) {
      std::stringstream sstm;
      sstm << Loop->getControlVars().size();

      LoopClassifier::classify(Loop, Success, SliceType+"ControlVars", sstm.str());
      return;
    }
};
