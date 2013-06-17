class BranchingClassifier : public LoopClassifier {
  public:
    void classify(const NaturalLoopPair P) {
      const NaturalLoop *Loop = P.first;

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

      LoopClassifier::classify(Loop, Success, "Branch", sstm.str());
      return;
    }
};
