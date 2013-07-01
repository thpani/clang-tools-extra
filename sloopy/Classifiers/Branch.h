#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CLASSIFIERS_BRANCH_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CLASSIFIERS_BRANCH_
#include "IncrementClassifier.h"

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

class InnerInfluencesOuterClassifier : public LoopClassifier {
  bool IsCurrentHeaderBlock (const NaturalLoopBlock *Block) {
    return Block->getBlockID() == CurrentHeaderBlockID;
  }
  unsigned CurrentHeaderBlockID;
  public:
    void classify(const MergedLoopDescriptor D, const NaturalLoop *Loop) {
      for (auto NestedLoop : D.ProperlyNestedLoops) {
        CurrentHeaderBlockID = NestedLoop->Header->getBlockID();
        if (std::find_if(Loop->begin(), Loop->end(), std::bind(&InnerInfluencesOuterClassifier::IsCurrentHeaderBlock, this, std::placeholders::_1)) != Loop->end()) {
          // if `Loop' has a block with the block id of the nested loop's header
          // => the nested loop influences the outer loop `Loop'
          LoopClassifier::classify(Loop, Success, "InfluencedByInner");
          // TODO: mark inner loop as InfluencesOuter. MOVE RETURN!!!!
          return;
        }
      }
    }
};

class MultiExitAdaClassifier : public BaseAdaForLoopClassifier {
  protected:
    virtual bool checkPreds(const NaturalLoop *Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      assert(PredSize > 0);
      return true;
    }
  public:
    MultiExitAdaClassifier(const ASTContext *Context) : BaseAdaForLoopClassifier("MultiExitIntegerIter", Context) {}
};

class MultiExitAdaIncrSetSizeClassifier : public LoopClassifier {
  public:
  void classify(const ASTContext* Context, const NaturalLoop *Loop) {
    const MultiExitAdaClassifier AFLC(Context);
    auto IncrementSet = AFLC.classify(Loop);

    std::stringstream sstm;
    sstm << IncrementSet.size();

    LoopClassifier::classify(Loop, Success, "MultiExitIntegerIterIncrSetSize", sstm.str());
  }
};

class AmortizedTypeAClassifier : public LoopClassifier {
  bool IsCurrentBlock (const NaturalLoopBlock *Block) {
    return Block->getBlockID() == CurrentBlockID;
  }
  unsigned CurrentBlockID;

  class SubExprVisitor : public RecursiveASTVisitor<SubExprVisitor> {
    public:
      SubExprVisitor(const ASTContext *Context) : Context(Context) {}
      bool isVarIncremented(const VarDecl *VD, const Stmt *S) {
        IncrementVD = const_cast<VarDecl*>(VD);
        IsVarIncremented = false;
        TraverseStmt(const_cast<Stmt*>(S));
        return IsVarIncremented;
      }

      bool VisitExpr(Expr *Expr) {
        try {
          IncrementInfo OuterIncr = ::getIncrementInfo(Expr, "", Context, &isIntegerType);
          if (OuterIncr.VD == IncrementVD) {
            IsVarIncremented = true;
            return false;
          }
        } catch (checkerror) {}
        return true;
      }

    private:
      bool IsVarIncremented;
      VarDecl *IncrementVD;
      const ASTContext *Context;
  };
  public:
    void classify(const ASTContext* Context, const NaturalLoop *Loop, const NaturalLoop *OutermostNestingLoop, const std::vector<const NaturalLoop*> NestingLoops) {
      if (Loop == OutermostNestingLoop) return;

      const MultiExitAdaClassifier AFLC(Context);
      auto IncrementSet = AFLC.classify(Loop);
      if (!IncrementSet.size()) return;

      SubExprVisitor SEV(Context);

      for (auto Increment : IncrementSet) {
        for (NaturalLoop::const_iterator I = OutermostNestingLoop->begin(),
                                        E = OutermostNestingLoop->end();
                                        I != E; I++) {
          const NaturalLoopBlock *Block = *I;
          CurrentBlockID = Block->getBlockID();
          if (std::find_if(Loop->begin(), Loop->end(), std::bind(&AmortizedTypeAClassifier::IsCurrentBlock, this, std::placeholders::_1)) != Loop->end()) {
            // block is in inner loop
            continue;
          }

          for (NaturalLoopBlock::const_iterator I = Block->begin(),
                                                E = Block->end();
                                                I != E; I++) {
            const Stmt *S = *I;
            // there is >= 1 increment of the inner loop's counter
            try {
              if (SEV.isVarIncremented(Increment.VD, S)) {
                // Inner.VD = Outer.VD sub-classifier
                for (const NaturalLoop *NestingLoop : NestingLoops) {
                  if (NestingLoop == Loop) continue;
                  auto OuterIncrementSet = AFLC.classify(NestingLoop);
                  for (auto OuterI : OuterIncrementSet) {
                    if (OuterI.VD == Increment.VD) {
                      LoopClassifier::classify(Loop, Success, "AmortA1InnerEqOuter");
                    }
                  }
                }

                LoopClassifier::classify(Loop, Success, "AmortA1");
                return;
              }
            } catch (checkerror) {}
          }
        }
      }
    }
};

class AmortizedTypeA2Classifier : public LoopClassifier {
  bool IsCurrentBlock (const NaturalLoopBlock *Block) {
    return Block->getBlockID() == CurrentBlockID;
  }
  unsigned CurrentBlockID;
  public:
    void classify(const ASTContext* Context, const NaturalLoop *Loop, const NaturalLoop *OutermostNestingLoop, const std::vector<const NaturalLoop*> NestingLoops) {
      if (Loop == OutermostNestingLoop) return;

      const MultiExitAdaClassifier AFLC(Context);
      auto IncrementSet = AFLC.classify(Loop);
      if (!IncrementSet.size()) return;

      for (auto Increment : IncrementSet) {
        for (NaturalLoop::const_iterator I = OutermostNestingLoop->begin(),
                                        E = OutermostNestingLoop->end();
                                        I != E; I++) {
          const NaturalLoopBlock *Block = *I;
          CurrentBlockID = Block->getBlockID();
          if (std::find_if(Loop->begin(), Loop->end(), std::bind(&AmortizedTypeA2Classifier::IsCurrentBlock, this, std::placeholders::_1)) != Loop->end()) {
            // block is in inner loop
            continue;
          }

          for (NaturalLoopBlock::const_iterator I = Block->begin(),
                                                E = Block->end();
                                                I != E; I++) {
            const Stmt *S = *I;
            DefUseHelper H(S);
            if (H.isDef(Increment.VD))
              goto outer_loop;
          }
        }
        // Inner.VD = Outer.VD sub-classifier
        for (const NaturalLoop *NestingLoop : NestingLoops) {
          if (NestingLoop == Loop) continue;
          auto OuterIncrementSet = AFLC.classify(NestingLoop);
          for (auto OuterI : OuterIncrementSet) {
            if (OuterI.VD == Increment.VD) {
              LoopClassifier::classify(Loop, Success, "AmortA2InnerEqOuter");
            }
          }
        }
        LoopClassifier::classify(Loop, Success, "AmortA2");
        return;
outer_loop:
        ; /* no op */
      }
    }
};

class AmortizedTypeBClassifier : public LoopClassifier {
  bool IsCurrentBlock (const NaturalLoopBlock *Block) {
    return Block->getBlockID() == CurrentBlockID;
  }
  unsigned CurrentBlockID;
  public:
    void classify(const ASTContext* Context, const NaturalLoop *Loop, const NaturalLoop *OutermostNestingLoop, const std::vector<const NaturalLoop*> NestingLoops) {
      if (Loop == OutermostNestingLoop) return;

      const MultiExitAdaClassifier AFLC(Context);
      auto IncrementSet = AFLC.classify(Loop);
      if (!IncrementSet.size()) return;

      for (auto Increment : IncrementSet) {
        for (const NaturalLoop* Outer : NestingLoops) {
          if (Outer == Loop) continue;
          auto OuterIncrementSet = AFLC.classify(Outer);
          for (auto OuterIncrement : OuterIncrementSet) {
            if (OuterIncrement.Delta == Increment.Bound) {
              LoopClassifier::classify(Loop, Success, "AmortB");
              return;
            }
          }
        }
      }
    }
};

#endif
