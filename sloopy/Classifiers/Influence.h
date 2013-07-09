#pragma once

#include "Master.h"

class InnerInfluencesOuterClassifier : public LoopClassifier {
  bool IsCurrentHeaderBlock (const NaturalLoopBlock *Block) const {
    return Block->getBlockID() == CurrentHeaderBlockID;
  }
  mutable unsigned CurrentHeaderBlockID;
  public:
    void classify(const std::vector<const NaturalLoop*> ProperlyNestedLoops, const NaturalLoop *Loop) const {
      for (auto NestedLoop : ProperlyNestedLoops) {
        CurrentHeaderBlockID = (*NestedLoop->getEntry().succ_begin())->getBlockID();
        if (std::find_if(Loop->begin(), Loop->end(), std::bind(&InnerInfluencesOuterClassifier::IsCurrentHeaderBlock, this, std::placeholders::_1)) != Loop->end()) {
          // if `Loop' has a block with the block id of the nested loop's header
          // => the nested loop influences the outer loop `Loop'
          LoopClassifier::classify(Loop, Success, "InfluencedByInner");
          LoopClassifier::classify(NestedLoop, Success, "InfluencesOuter");
          if (LoopClassifier::hasClass(NestedLoop, "WeakAmortA1")) {
          LoopClassifier::classify(Loop, Success, "StronglyInfluencedByInner");
            LoopClassifier::classify(NestedLoop, Success, "StronglyInfluencesOuter");
          }
        }
      }
    }
};


class AmortizedTypeAClassifier : public LoopClassifier {
  const ASTContext *Context;
  const std::string Marker;

  bool IsCurrentBlock (const NaturalLoopBlock *Block) const {
    return Block->getBlockID() == CurrentBlockID;
  }
  mutable unsigned CurrentBlockID;

  class SubExprVisitor : public RecursiveASTVisitor<SubExprVisitor> {
    public:
      SubExprVisitor(const ASTContext *Context) : Context(Context) {}
      bool isVarIncremented(const VarDecl *VD, const Stmt *S) {
        IncrementVD = const_cast<VarDecl*>(VD);
        IsVarIncremented = false;
        this->TraverseStmt(const_cast<Stmt*>(S));
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
    AmortizedTypeAClassifier(const ASTContext* Context, const std::string Marker=std::string()) :
      Context(Context),
      Marker(Marker) {}
    void classify(const MasterIncrementClassifier &MasterIncrementClassifier, const IncrementClassifierConstraint Constr, const NaturalLoop *Loop, const NaturalLoop *OutermostNestingLoop, const std::vector<const NaturalLoop*> NestingLoops) const {
      if (Loop == OutermostNestingLoop) return;

      auto IncrementSet = MasterIncrementClassifier.classify(Loop, Constr);
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
                  auto OuterIncrementSet = MasterIncrementClassifier.classify(NestingLoop, Constr);
                  for (auto OuterI : OuterIncrementSet) {
                    if (OuterI.VD == Increment.VD) {
                      LoopClassifier::classify(Loop, Success, Marker+"AmortA1InnerEqOuter");
                    }
                  }
                }

                LoopClassifier::classify(Loop, Success, Marker+"AmortA1");
                return;
              }
            } catch (checkerror) {}
          }
        }
      }
    }
};

class AmortizedTypeA2Classifier : public LoopClassifier {
  bool IsCurrentBlock (const NaturalLoopBlock *Block) const {
    return Block->getBlockID() == CurrentBlockID;
  }
  mutable unsigned CurrentBlockID;
  public:
    void classify(const MasterIncrementClassifier &MasterIncrementClassifier, const NaturalLoop *Loop, const NaturalLoop *OutermostNestingLoop, const std::vector<const NaturalLoop*> NestingLoops) const {
      if (Loop == OutermostNestingLoop) return;

      auto IncrementSet = MasterIncrementClassifier.classify(Loop, MultiExit);
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
          auto OuterIncrementSet = MasterIncrementClassifier.classify(NestingLoop, MultiExit);
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
  bool IsCurrentBlock (const NaturalLoopBlock *Block) const {
    return Block->getBlockID() == CurrentBlockID;
  }
  mutable unsigned CurrentBlockID;
  public:
    void classify(const MasterIncrementClassifier &MasterIncrementClassifier, const NaturalLoop *Loop, const NaturalLoop *OutermostNestingLoop, const std::vector<const NaturalLoop*> NestingLoops) const {
      if (Loop == OutermostNestingLoop) return;

      auto IncrementSet = MasterIncrementClassifier.classify(Loop, MultiExit);
      if (!IncrementSet.size()) return;

      for (auto Increment : IncrementSet) {
        for (const NaturalLoop* Outer : NestingLoops) {
          if (Outer == Loop) continue;
          auto OuterIncrementSet = MasterIncrementClassifier.classify(Outer, MultiExit);
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
