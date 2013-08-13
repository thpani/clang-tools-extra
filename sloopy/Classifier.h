#pragma once

#include "Classifiers/Branch.h"
#include "Classifiers/Influence.h"
#include "Classifiers/Master.h"
/* #include "Classifiers/EmptyBody.h" */
/* #include "Classifiers/Cond.h" */

class Classifier {
  const AnyLoopCounter ALC;
  const ExitClassifier SLC;
  const BranchingClassifier B;
  const BranchingClassifier B2;
  const ControlVarClassifier CVC;
  const ControlVarClassifier CVC2;
  const MasterIncrementClassifier MasterC;
  const AmortizedTypeAClassifier ATAC;
  const AmortizedTypeA2Classifier ATA2C;
  const AmortizedTypeBClassifier ATBC;
  const InnerInfluencesOuterClassifier IIOC;
  const AmortizedTypeAClassifier WeakATAC;
  public:
    Classifier(const ASTContext *Context) :
      ALC(), SLC(),
      B("AllLoops"), B2("OuterLoop"),
      CVC("AllLoops"), CVC2("OuterLoop"),
      MasterC(Context),
      ATAC(Context), ATA2C(), ATBC(), IIOC(), WeakATAC(Context, "Weak") {}
    void classify(
        const bool isSpecified,
        const NaturalLoop *Unsliced,
        const NaturalLoop *SlicedAllLoops,
        const NaturalLoop *SlicedOuterLoop,
        const NaturalLoop *OutermostNestingLoop,
        const std::vector<const NaturalLoop*> NestingLoops,
        const std::vector<const NaturalLoop*> ProperlyNestedLoops) const {
      time_t Begin = time(NULL);

      // ANY + Stmt
      ALC.classify(SlicedAllLoops);

      // Simple control flow
      SLC.classify(SlicedAllLoops);

      // Branching
      B.classify(SlicedAllLoops);
      B2.classify(SlicedOuterLoop);

      // ControlVars
      CVC.classify(SlicedAllLoops);
      CVC2.classify(SlicedOuterLoop);

      MasterC.classify(SlicedAllLoops, SingleExit);
      MasterC.classify(SlicedAllLoops, StrongSingleExit);
      auto IMEAC = MasterC.classify(SlicedAllLoops, MultiExit);
      MasterC.classify(SlicedAllLoops, StrongMultiExit);
      if (isSpecified && DumpIncrementVars) {
        for (auto I : IMEAC) {
          llvm::errs() << "(incr: " << I.VD->getNameAsString() << ", ";
          llvm::errs() << "bound: ";
          if (I.Bound.Var) {
            llvm::errs() << I.Bound.Var->getNameAsString();
          } else {
            llvm::errs() << I.Bound.Int.getSExtValue();
          }
          llvm::errs() << ", ";
          llvm::errs() << "delta: ";
          if (I.Delta.Var) {
            llvm::errs() << I.Delta.Var->getNameAsString();
          } else {
            llvm::errs() << I.Delta.Int.getSExtValue();
          }
          llvm::errs() << ")\n";
        }
      }

      // Influence

      if (EnableAmortized) {
        ATA2C.classify(MasterC, SlicedAllLoops, OutermostNestingLoop, NestingLoops);
        ATAC.classify(MasterC, MultiExit, SlicedAllLoops, OutermostNestingLoop, NestingLoops);
        /* Make sure to pass Unsliced!!!
        * MultiExitNoCond classifier needs the Unsliced CFG to find all increments!
        */
        WeakATAC.classify(MasterC, MultiExitNoCond, Unsliced, OutermostNestingLoop, NestingLoops);
        ATBC.classify(MasterC, SlicedAllLoops, OutermostNestingLoop, NestingLoops);

        /* Uses hasClass!!!
        * Make sure to run this AFTER WeakAmortizedTypeAClassifier!!!
        */
        IIOC.classify(ProperlyNestedLoops, SlicedOuterLoop);
      }

      LoopClassifier::classify(Unsliced, "Time", (int)(time(NULL)-Begin));
    }
};

