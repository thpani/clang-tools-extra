#pragma once

#include "Classifiers/Branch.h"
#include "Classifiers/Influence.h"
#include "Classifiers/Master.h"
/* #include "Classifiers/EmptyBody.h" */
/* #include "Classifiers/Cond.h" */

#include "Time.h"

class Classifier {
  const AnyLoopCounter ALC;
  const ExitClassifier SLC;
  const BranchingClassifier B;
  const BranchingClassifier B2;
  const ControlVarClassifier CVC;
  const ControlVarClassifier CVC2;
  const MasterIncrementClassifier MasterC;
  const MasterProvingClassifier MasterPC;
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
      MasterPC(Context),
      ATAC(Context), ATA2C(), ATBC(), IIOC(), WeakATAC(Context, "Weak") {}
    void classify(
        const bool isSpecified,
        const NaturalLoop *Unsliced,
        const NaturalLoop *SlicedAllLoops,
        const NaturalLoop *SlicedOuterLoop,
        const NaturalLoop *OutermostNestingLoop,
        const std::vector<const NaturalLoop*> NestingLoops,
        const std::vector<const NaturalLoop*> ProperlyNestedLoops) const {
      long Begin = now();

      if (MachineLearning) {
        ALC.classify(Unsliced); // ANY + Stmt
        MasterPC.classify(Unsliced, SyntacticTerm);
        MasterPC.classify(Unsliced, AnyExitWeakCfWellformed);
      } else {
      // ANY + Stmt
      ALC.classify(Unsliced);

      if (not Psyntterm_only) {

        // Simple control flow
        SLC.classify(Unsliced);

        // Branching
        B.classify(SlicedAllLoops);
        B2.classify(SlicedOuterLoop);

        // ControlVars
        CVC.classify(SlicedAllLoops);
        CVC2.classify(SlicedOuterLoop);

        MasterC.classify(Unsliced, SingleExit);
        MasterC.classify(Unsliced, StrongSingleExit);
        auto IMEAC = MasterC.classify(Unsliced, MultiExit);
        MasterC.classify(Unsliced, StrongMultiExit);
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
      }
      MasterPC.classify(Unsliced, SyntacticTerm);
      if (not Psyntterm_only) {
        MasterPC.classify(Unsliced, AnyExitProvedCfTerminating);
        MasterPC.classify(Unsliced, AnyExitStrongCfTerminating);
        MasterPC.classify(Unsliced, AnyExitWeakCfTerminating);
        MasterPC.classify(Unsliced, AnyExitProvedCfWellformed);
        MasterPC.classify(Unsliced, AnyExitStrongCfWellformed);
        MasterPC.classify(Unsliced, AnyExitWeakCfWellformed);
        MasterPC.classify(Unsliced, SingleExitProvedCfTerminating);
        MasterPC.classify(Unsliced, SingleExitStrongCfTerminating);
        MasterPC.classify(Unsliced, SingleExitWeakCfTerminating);
        MasterPC.classify(Unsliced, SingleExitProvedCfWellformed);
        MasterPC.classify(Unsliced, SingleExitStrongCfWellformed);
        MasterPC.classify(Unsliced, SingleExitWeakCfWellformed);

        MasterPC.classify(Unsliced, AnyExitStrongCfInvariantTerminating);
        MasterPC.classify(Unsliced, AnyExitWeakCfInvariantTerminating);
        MasterPC.classify(Unsliced, AnyExitProvedCfInvariantWellformed);
        MasterPC.classify(Unsliced, AnyExitStrongCfInvariantWellformed);
        MasterPC.classify(Unsliced, AnyExitWeakCfInvariantWellformed);
        MasterPC.classify(Unsliced, SingleExitProvedCfInvariantTerminating);
        MasterPC.classify(Unsliced, SingleExitStrongCfInvariantTerminating);
        MasterPC.classify(Unsliced, SingleExitWeakCfInvariantTerminating);
        MasterPC.classify(Unsliced, SingleExitProvedCfInvariantWellformed);
        MasterPC.classify(Unsliced, SingleExitStrongCfInvariantWellformed);
        MasterPC.classify(Unsliced, SingleExitWeakCfInvariantWellformed);

        // Influence

        if (EnableAmortized) {
          ATA2C.classify(MasterC, Unsliced, OutermostNestingLoop, NestingLoops);
          ATAC.classify(MasterC, MultiExit, Unsliced, OutermostNestingLoop, NestingLoops);
          /* Make sure to pass Unsliced!!!
          * MultiExitNoCond classifier needs the Unsliced CFG to find all increments!
          */
          WeakATAC.classify(MasterC, MultiExitNoCond, Unsliced, OutermostNestingLoop, NestingLoops);
          ATBC.classify(MasterC, Unsliced, OutermostNestingLoop, NestingLoops);

          /* Uses hasClass!!!
          * Make sure to run this AFTER WeakAmortizedTypeAClassifier!!!
          */
          IIOC.classify(ProperlyNestedLoops, Unsliced);
        }
      }
      }

      LoopClassifier::classify(Unsliced, "Time", (int)(now()-Begin));
    }
};

