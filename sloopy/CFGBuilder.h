#pragma once

#include <sys/types.h> /* pid_t */
#include <unistd.h>  /* _exit, fork */

#include <stack>

#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ParentMap.h"

#include "Loop.h"
#include "LoopMatchers.h"
#include "DefUse.h"
#include "Classifiers/Ada.h"
#include "Classifiers/AdaArray.h"
#include "Classifiers/Branch.h"
#include "Classifiers/DataIter.h"
#include "Classifiers/ArrayIter.h"
#include "Classifiers/Influence.h"
#include "Classifiers/MultiExit.h"
/* #include "Classifiers/EmptyBody.h" */
/* #include "Classifiers/Cond.h" */

using namespace clang;
using namespace clang::ast_matchers;

using namespace sloopy;

llvm::cl::opt<bool> LoopStats("loop-stats");
llvm::cl::opt<bool> IgnoreInfiniteLoops("ignore-infinite-loops");
llvm::cl::opt<bool> ViewSliced("view-sliced");
llvm::cl::opt<bool> ViewSlicedOuter("view-sliced-outer");
llvm::cl::opt<bool> ViewUnsliced("view-unsliced");
llvm::cl::opt<bool> ViewCFG("view-cfg");
llvm::cl::opt<bool> DumpBlocks("dump-blocks");
llvm::cl::opt<bool> DumpIncrementVars("dump-increment-vars");
llvm::cl::opt<bool> DumpControlVars("dump-control-vars");
llvm::cl::opt<bool> DumpControlVarsDetail("dump-control-vars-detail");
llvm::cl::opt<bool> DumpClasses("dump-classes");
llvm::cl::opt<bool> DumpAST("dump-ast");
llvm::cl::opt<bool> DumpStmt("dump-stmt");
llvm::cl::opt<std::string> Function("func");
llvm::cl::opt<std::string> File("file");
llvm::cl::opt<unsigned> Line("line");

std::map<const NaturalLoop*, std::string> LoopLocationMap;

static bool isSpecified(const FunctionDecl *D, const PresumedLoc &PL) {
  return (Function.size() == 0 || D->getNameAsString() == Function) &&
         (File.size() == 0 || PL.getFilename() == File) &&
         (Line == 0 || PL.getLine() == Line);
}

class PostDominatorTree : public DominatorTree {
  public:
    PostDominatorTree() : DominatorTree() {
      DT = new llvm::DominatorTreeBase<CFGBlock>(true);
    }
};

// Ferrante, Ottenstein, Warren 1987
class ControlDependenceGraph {
  public:
    bool buildControlDependenceGraph(AnalysisDeclContext &AC) {
      CFG *CFG = AC.getCFG();
      PostDominatorTree PD;
      PD.buildDominatorTree(AC);

      for (CFG::const_iterator I = CFG->begin(),
                               E = CFG->end();
                               I != E; I++) {
        const CFGBlock *A = *I;
        for (CFGBlock::const_succ_iterator I2 = A->succ_begin(),
                                           E2 = A->succ_end();
                                           I2 != E2; I2++) {
          const CFGBlock *B = *I2;
          if (!PD.dominates(B, A)) {
            // edge A->B where B does not postdominate A
            const CFGBlock *L = PD.findNearestCommonDominator(A, B);
            assert(L);
            CFGBlock *AParent = PD.getBase().getNode(const_cast<CFGBlock*>(A))->getIDom()->getBlock();
            assert(L == A || L == AParent);

            // Walk NodeB immediate dominators chain and find common dominator node.
            llvm::DomTreeNodeBase<CFGBlock> *IDomB = PD.getBase().getNode(const_cast<CFGBlock*>(B));
            if (IDomB == NULL) {
              if (IgnoreInfiniteLoops) return false;
              llvm_unreachable("infinite loop");
            }
            do {
              Map[const_cast<const CFGBlock*>(IDomB->getBlock())].push_back(A);
              IDomB = IDomB->getIDom();
            }
            while (IDomB->getBlock() != AParent);
          }
        }
      }
      return true;
    }

    bool dependsOn(const CFGBlock *A, const CFGBlock *B) const {
      auto I = Map.find(A);
      if (I != Map.end()) {
        for (auto Dep : I->second) {
          if (Dep == B || dependsOn(Dep, B)) return true;
        }
      }
      return false;
    }

    std::set<const CFGBlock *> dependsOn(const CFGBlock *A) const {
      std::set<const CFGBlock *> Result;
      std::stack<const CFGBlock*> Worklist;
      Worklist.push(A);
      while (Worklist.size()) {
        const CFGBlock *Current = Worklist.top();
        Worklist.pop();

        auto I = Map.find(Current);
        if (I != Map.end()) {
          for (auto Dep : I->second) {
            bool Inserted = Result.insert(Dep).second;
            if (Inserted) {
              Worklist.push(Dep);
            }
          }
        }
      }
      return Result;
    }

  private:
    std::map<const CFGBlock*, std::vector<const CFGBlock*>> Map;
};

static bool isTransitionBlock(const CFGBlock *Block) {
  if (Block->size() == 0 &&    // no statements
      Block->getTerminator() == NULL && Block->getLabel() == NULL &&
      Block->succ_size() >= 1) // EXIT has 0
    {
      return true;
    }
  return false;
}

static const std::set<const CFGBlock*> getExitingTerminatorConditions(const std::set<const CFGBlock*> Blocks) {
  std::set<const CFGBlock*> Result;
  for (auto Block : Blocks) {
    for (CFGBlock::const_succ_iterator I = Block->succ_begin(),
                                       E = Block->succ_end();
                                       I != E; I++) {
      const CFGBlock *Succ = *I;
      if (isTransitionBlock(Succ)) continue;
      if (Blocks.count(Succ) == 0) {
        if (Block->getTerminatorCondition()) {
          Result.insert(Block);
        } else {
          llvm_unreachable("Exiting block has no condition");
        }
      }
    }
  }
  return Result;
}

static const Stmt *getGotoLCA(const Stmt *S, const FunctionDecl *D) {
  if (const GotoStmt *GS = dyn_cast<GotoStmt>(S)) {
    // find lca of GOTO and LABEL stmts
    LabelStmt *LS = GS->getLabel()->getStmt();

    ParentMap PM(D->getBody());
    std::vector<const Stmt*> GSAncestors;
    for (const Stmt *P = GS; P; P = PM.getParent(P)) GSAncestors.push_back(P);
    std::vector<const Stmt*> LSAncestors;
    for (const Stmt *P = LS; P; P = PM.getParent(P)) LSAncestors.push_back(P);

    std::vector<const Stmt*>::reverse_iterator GSI, LSI, GSE, LSE;
    for (GSI = GSAncestors.rbegin(),
         LSI = LSAncestors.rbegin(),
         GSE = GSAncestors.rend(),
         LSE = LSAncestors.rend();
         *GSI == *LSI && GSI != GSE && LSI != LSE;
         GSI++, LSI++);
    // GSI, LSI point 1 past the LCA
    return *(GSI-1);
  }
  return S;
}

// compute a fixed point of the program slice
static const NaturalLoop *buildNaturalLoop(
    const MergedLoopDescriptor &Loop,
    const NaturalLoop *Unsliced,
    const ControlDependenceGraph &CDG,
    const SlicingCriterion SC) {
  const CFGBlock *Header = Loop.Header;
  std::set<const CFGBlock*> Tails = Loop.Tails;
  std::set<const CFGBlock*> Body = Loop.Body;

  std::set<const CFGBlock*> VisitedBlocks;
  std::set<const Stmt*> TrackedStmts;
  std::set<const VarDecl*> ControlVars(SC.Vars);
  std::set<const CFGBlock*> TrackedBlocks(SC.Locations);
  size_t OldSize;
  do {
    OldSize = TrackedStmts.size();

    // for all statements in the loop's CFG
    for (auto Block : Body) {
      for (auto Element : *Block) {
        auto Opt = Element.getAs<CFGStmt>();
        assert(Opt);
        const Stmt *Stmt = Opt->getStmt();
        if (TrackedStmts.count(Stmt) > 0) continue;
        DefUseHelper A(Stmt);
        for (auto Var : ControlVars) {
          if (A.isDef(Var)) {
            // if one of the control vars is modified in this stmt, track the stmt
            if (TrackedStmts.insert(Stmt).second) {
              if (DumpControlVarsDetail) {
                llvm::errs() << "1s: ";
                Stmt->printPretty(llvm::errs(), NULL, PrintingPolicy(LangOptions()));
                llvm::errs() << "\n";
              }
              // add used variables of defining substmts
              for (auto SubStmt : A.getDefiningStmts(Var)) {
                DefUseHelper C(SubStmt);
                for (auto Var : C.getUses()) {
                  ControlVars.insert(Var);
                  if (DumpControlVarsDetail) {
                    llvm::errs() << "1: " << Var->getNameAsString() << "\n";
                  }
                }
              }
            }

            // see if we have already collected control-dependent nodes,
            // or have yet to do it
            if (VisitedBlocks.insert(Block).second) {
              // collect new control variables from each block
              // this block is control dependent on and track that block
              for (auto DepBlock : CDG.dependsOn(Block)) {
                if (Body.count(DepBlock) == 0) continue;
                if (TrackedBlocks.insert(DepBlock).second) {
                  const class Stmt *Stmt = DepBlock->getTerminatorCondition();
                  TrackedStmts.insert(Stmt);
                  if (DumpControlVarsDetail) {
                    llvm::errs() << "2s: ";
                    Stmt->printPretty(llvm::errs(), NULL, PrintingPolicy(LangOptions()));
                    llvm::errs() << "\n";
                  }

                  DefUseHelper B(Stmt);
                  for (auto Var : B.getDefsAndUses()) {
                    ControlVars.insert(Var);
                    if (DumpControlVarsDetail) {
                      /* Stmt->dump(); */
                      llvm::errs() << "2: " << Var->getNameAsString() << "\n";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  } while (TrackedStmts.size() > OldSize);


  if (DumpBlocks) {
    llvm::errs() << "Blocks: ";
    for (auto Block : Body) {
      llvm::errs() << Block->getBlockID();
      llvm::errs() << ", ";
    }
    llvm::errs() << "\n";
  }

  NaturalLoop *Sliced = new NaturalLoop();
  if (Sliced->build(Header, Tails, Body, ControlVars, &TrackedStmts, &TrackedBlocks, Unsliced)) {
    return Sliced;
  }

  return NULL;
}

static const NaturalLoop *buildNaturalLoop(
    const MergedLoopDescriptor &Loop,
    const std::set<const VarDecl*> ControlVars) {
  const CFGBlock *Header = Loop.Header;
  std::set<const CFGBlock*> Tails = Loop.Tails;
  std::set<const CFGBlock*> Body = Loop.Body;

  NaturalLoop *Unsliced = new NaturalLoop();
  if (Unsliced->build(Header, Tails, Body, ControlVars)) {
    return Unsliced;
  }
  return NULL;
}

static SlicingCriterion slicingCriterionOuterLoop(const MergedLoopDescriptor &Loop) {
  // collect initial control variables
  std::set<const VarDecl*> ControlVars;
  std::set<const CFGBlock*> ExitingBlocks;
  std::set<const CFGBlock*> NestedExitingBlocks = getExitingTerminatorConditions(Loop.Body);
  for (auto Block : NestedExitingBlocks) {
    ExitingBlocks.insert(Block);
    const Stmt *Stmt = Block->getTerminatorCondition();
    DefUseHelper A(Stmt);
    for (auto Var : A.getDefsAndUses()) {
      if(ControlVars.insert(Var).second) {
        if (DumpControlVarsDetail) {
          llvm::errs() << "init: " << Var->getNameAsString() << " (B";
          llvm::errs() << Block->getBlockID();
          llvm::errs() << ")\n";
        }
      }
    }
  }
  return { ControlVars, ExitingBlocks };
}

static SlicingCriterion slicingCriterionAllLoops(const MergedLoopDescriptor &Loop) {
  // collect initial control variables
  std::set<const VarDecl*> ControlVars;
  std::set<const CFGBlock*> ExitingBlocks;
  assert(Loop.NestedLoops.size() > 0);
  for (auto NestedLoop : Loop.NestedLoops) {
    std::set<const CFGBlock*> NestedExitingBlocks = getExitingTerminatorConditions(NestedLoop->Body);
    for (auto Block : NestedExitingBlocks) {
      ExitingBlocks.insert(Block);
      const Stmt *Stmt = Block->getTerminatorCondition();
      DefUseHelper A(Stmt);
      for (auto Var : A.getDefsAndUses()) {
        if(ControlVars.insert(Var).second) {
          if (DumpControlVarsDetail) {
            llvm::errs() << "init: " << Var->getNameAsString() << " (B";
            llvm::errs() << Block->getBlockID();
            llvm::errs() << ")\n";
          }
        }
      }
    }
  }
  return { ControlVars, ExitingBlocks };
}

void classify(
    const bool isSpecified,
    const NaturalLoop *SlicedAllLoops,
    const NaturalLoop *SlicedOuterLoop,
    const NaturalLoop *OutermostNestingLoop,
    const std::vector<const NaturalLoop*> NestingLoops,
    const std::vector<const NaturalLoop*> ProperlyNestedLoops,
    const ASTContext *Context) {
  // Any
  AnyLoopCounter ALC;
  ALC.classify(SlicedAllLoops);

  // Statements
  LoopClassifier LoopCounter;
  LoopCounter.classify(SlicedAllLoops);

  // Branching
  BranchingClassifier B("AllLoops");
  B.classify(SlicedAllLoops);
  BranchingClassifier B2("OuterLoop");
  B2.classify(SlicedOuterLoop);

  // ControlVars
  ControlVarClassifier CVC("AllLoops");
  CVC.classify(SlicedAllLoops);
  ControlVarClassifier CVC2("OuterLoop");
  CVC2.classify(SlicedOuterLoop);

  // Simple Plans
  DataIterClassifier DIC;
  DIC.classify(SlicedOuterLoop);
  AdaArrayForLoopClassifier AA(Context);
  auto I = AA.classify(SlicedOuterLoop);
  if (!I.size()) {
    IntegerIterClassifier A(Context);
    auto Increments = A.classify(SlicedOuterLoop);
  }
  ArrayIterClassifier Ar(Context);
  Ar.classify(SlicedOuterLoop);

  // Test
  {
    MultiExitIncrSetSizeClassifier C(Context);
    auto IMEAC = C.classify(SlicedAllLoops);
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
#if 0 // called by MultiExitIncrSetSizeClassifier
  {
    MultiExitIntegerIterIncrSetSizeClassifier C;
    C.classify(Context, SlicedAllLoops);
  }
  {
    MultiExitDataIterIncrSetSizeClassifier C;
    C.classify(Context, SlicedAllLoops);
  }
  {
    MultiExitAdaArrayForLoopIncrSetSizeClassifier C;
    C.classify(Context, SlicedAllLoops);
  }
  {
    MultiExitArrayIterIncrSetSizeClassifier C;
    C.classify(Context, SlicedAllLoops);
  }
#endif

  // Influence
  InnerInfluencesOuterClassifier IIOC;
  IIOC.classify(ProperlyNestedLoops, SlicedOuterLoop);

  AmortizedTypeA2Classifier ATA2C;
  ATA2C.classify(Context, SlicedAllLoops, OutermostNestingLoop, NestingLoops);
  AmortizedTypeAClassifier ATAC;
  ATAC.classify(Context, SlicedAllLoops, OutermostNestingLoop, NestingLoops);
  AmortizedTypeBClassifier ATBC;
  ATBC.classify(Context, SlicedAllLoops, OutermostNestingLoop, NestingLoops);

  // Simple control flow
  SimpleLoopCounter SLC;
  SLC.classify(SlicedAllLoops);
}

bool isOutermostNestingLoop(const MergedLoopDescriptor *D) {
  return D->NestingLoops.size() == 1;
}

class FunctionCallback : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result) {
      const FunctionDecl *D = Result.Nodes.getNodeAs<FunctionDecl>(FunctionName);
      if (!D->hasBody()) return;

      std::map<const CFGBlock*, std::vector<LoopDescriptor>> Loops;

      AnalysisDeclContextManager mgr;
      AnalysisDeclContext *AC = mgr.getContext(D);
      CFG *CFG = AC->getCFG();

      ControlDependenceGraph CDG;
      // only for SV-COMP
      if (!CDG.buildControlDependenceGraph(*AC)) return;

      DominatorTree Dom;
      Dom.buildDominatorTree(*AC);

      /* Clang builds "Transition" blocks for loop stmt (c.f. `CFG.cpp').
       * Continue blocks will also point to this block, instead of the loop
       * header. Thus only the arc Transition->Header will be recognized as a
       * back edge, where potentially multiple back edges exist.
       * As a workaround, for every "empty" block (= transition block), we
       * introduce edges "jumping" past the transition block.
       */
      for (CFG::iterator it = CFG->begin(),
                         end = CFG->end();
                         it != end; it++) {
        CFGBlock *Block = *it;
        if (isTransitionBlock(Block)) {
          CFGBlock *Succ = *Block->succ_begin();
          // we found a transition block
          for (CFGBlock::pred_iterator PI = Block->pred_begin(),
                                      PE = Block->pred_end();
                                      PI != PE; PI++) {
            CFGBlock *Pred = *PI;
            Pred->addSuccessor(Succ, CFG->getBumpVectorContext());
            Pred->setLoopTarget(Block->getLoopTarget());
          }
        }
      }

      for (CFG::const_iterator it = CFG->begin(), end = CFG->end(); it != end; it++) {
        const CFGBlock *Tail = *it;
        for (CFGBlock::const_succ_iterator it2 = Tail->succ_begin(); it2 != Tail->succ_end(); it2++) {
          const CFGBlock *Header = *it2;
          // if Tail -> Header is a back edge
          if (Dom.dominates(Header, Tail) && Dom.isReachableFromEntry(Tail)) {  // TODO
            // collect loop blocks via DFS on reverse CFG
            std::set<const CFGBlock*> Body = { Header };
            std::stack<const CFGBlock*> worklist;
            worklist.push(Tail);
            while (worklist.size() > 0) {
              const CFGBlock *current = worklist.top();
              worklist.pop();

              if (!Body.insert(current).second) {
                continue;
              }

              for (CFGBlock::const_pred_iterator I = current->pred_begin(),
                                                 E = current->pred_end();
                                                 I != E; I++) {
                worklist.push(*I);
              }
            }

            Loops[Header].push_back({Header, Tail, Body});
          }
        }
      }

      // Merge loops with the same header.
      std::vector<MergedLoopDescriptor> LoopsAfterMerging;
      for (auto L : Loops) {
        const CFGBlock *Header = L.first;
        auto List = L.second;
        
        std::set<const CFGBlock*> MergedBody;
        std::set<const CFGBlock*> MergedTails;
        for (auto L2 : List) {
          for (const CFGBlock *Block : L2.Body) {
            MergedBody.insert(Block);
          }
          MergedTails.insert(L2.Tail);
        }
        LoopsAfterMerging.push_back(MergedLoopDescriptor(Header, MergedTails, MergedBody));
      }

      // Determine nested
      for (auto &Loop1 : LoopsAfterMerging) {
        for (auto &Loop2 : LoopsAfterMerging) {
          if (std::includes(Loop1.Body.begin(), Loop1.Body.end(),
                            Loop2.Body.begin(), Loop2.Body.end())) {
            if (std::includes(Loop2.Body.begin(), Loop2.Body.end(),
                              Loop1.Body.begin(), Loop1.Body.end())) {
              // subset(1, 2) && subset(2, 1) => 1 = 2
              Loop2.addTriviallyNestedLoop(&Loop2);
            } else {
              Loop1.addNestedLoop(&Loop2);
            }
          }
        }
      }

      std::map<MergedLoopDescriptor, std::vector<const NaturalLoop*>> M;
      for (auto &Loop : LoopsAfterMerging) {
        auto SC = slicingCriterionAllLoops(Loop);
        const NaturalLoop *Unsliced = buildNaturalLoop(Loop, SC.Vars);
        if (!Unsliced) continue;
        const NaturalLoop *SlicedAllLoops = buildNaturalLoop(Loop, Unsliced, CDG, SC);
        const NaturalLoop *SlicedOuterLoop = buildNaturalLoop(Loop, Unsliced, CDG, slicingCriterionOuterLoop(Loop));

        auto LocationPair = Unsliced->getLoopStmtLocation(Result.SourceManager);
        std::stringstream sstm;
        sstm << LocationPair.first.getFilename()
             << " -func " << D->getNameAsString()
             << " -line " << LocationPair.first.getLine()
             << " -linebe ";
        for (auto PLBack : LocationPair.second) {
          sstm << PLBack.getLine() << ",";
        }
        LoopLocationMap[Unsliced] = sstm.str();

        M[Loop].push_back(Unsliced);
        M[Loop].push_back(SlicedAllLoops);
        M[Loop].push_back(SlicedOuterLoop);
      }

      for (auto &MLD : LoopsAfterMerging) {
        const NaturalLoop *Unsliced = M[MLD][0];
        const NaturalLoop *SlicedAllLoops = M[MLD][1];
        const NaturalLoop *SlicedOuterLoop = M[MLD][2];

        auto LocationPair = Unsliced->getLoopStmtLocation(Result.SourceManager);
        PresumedLoc PL = LocationPair.first;
        if (isSpecified(D, PL)) {
          if (!LoopStats) {
            llvm::errs() << LoopLocationMap[Unsliced] << "\n";
          }
          if (DumpControlVars) {
            for (auto VD : Unsliced->getControlVars()) {
              llvm::errs() << "Control variable: " << VD->getNameAsString() << " (" << VD->getType().getAsString() << ")\n";
            }
          }
          if (DumpStmt) {
            const Stmt *S = getGotoLCA(Unsliced->getLoopStmt(), D);
            S->printPretty(llvm::errs(), NULL, PrintingPolicy(LangOptions()));
          }
          if (DumpAST) {
            const Stmt *S = getGotoLCA(Unsliced->getLoopStmt(), D);
            S->dump();
          }
          if (ViewSliced || ViewSlicedOuter || ViewUnsliced || ViewCFG) {
            llvm::errs() << "Back edges: ";
            for (auto Tail : MLD.Tails) {
              llvm::errs() << Tail->getBlockID();
              llvm::errs() << "->";
              llvm::errs() << MLD.Header->getBlockID();
              llvm::errs() << ", ";
            }
            llvm::errs() << "loop stmt: ";
            llvm::errs() << Unsliced->getLoopStmtMarker();
            llvm::errs() << "\n";

            if (ViewSliced) SlicedAllLoops->view();
            if (ViewSlicedOuter) SlicedOuterLoop->view();
            if (ViewUnsliced) Unsliced->view();
          }
        }

        // find outermost nesting loop
        assert(MLD.NestingLoops.size() > 0);
        auto I = std::find_if(MLD.NestingLoops.begin(), MLD.NestingLoops.end(), isOutermostNestingLoop);
        assert(I != MLD.NestingLoops.end());

        std::vector<const NaturalLoop*> NestingLoops;
        for (auto I = MLD.NestingLoops.begin(),
                  E = MLD.NestingLoops.end();
                  I != E; I++) {
          NestingLoops.push_back(M[**I][1]);
        }
        const NaturalLoop *OutermostNestingLoop = M[**I][1];

        std::vector<const NaturalLoop*> ProperlyNestedLoops;
        for (auto I = MLD.ProperlyNestedLoops.begin(),
                  E = MLD.ProperlyNestedLoops.end();
                  I != E; I++) {
          ProperlyNestedLoops.push_back(M[**I][1]);
        }

        classify(isSpecified(D, PL), SlicedAllLoops, SlicedOuterLoop, OutermostNestingLoop, NestingLoops, ProperlyNestedLoops, Result.Context);

        if (isSpecified(D, PL)) {
          if (DumpClasses) {
            for (auto Class : Classifications[Unsliced]) {
              llvm::errs() << Class << " ";
            }
            llvm::errs() << "\n";
          }
          if (DumpBlocks || DumpControlVars || DumpControlVarsDetail || DumpClasses || DumpAST || DumpStmt || DumpIncrementVars) {
            llvm::errs() << "----------\n";
          }
        }
      }
    }
};
