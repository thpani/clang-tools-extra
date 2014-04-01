#pragma once

#include <sys/types.h> /* pid_t */
#include <unistd.h>    /* _exit, fork */

#include <stack>

#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ParentMap.h"

#include "Loop.h"
#include "LoopMatchers.h"
#include "DefUse.h"
#include "Classifier.h"

using namespace clang;
using namespace clang::ast_matchers;

using namespace sloopy;

static bool isSpecified(const FunctionDecl *D, std::vector<PresumedLoc> PLs) {
  for (auto PL : PLs) {
    if ((Function.size() == 0 || D->getNameAsString() == Function) &&
        (File.size() == 0 || PL.getFilename() == File) &&
        (Line == 0 || PL.getLine() == Line)) {
          return true;
    }
  }
  return false;
}

class PostDominatorTree : public DominatorTree {
  public:
    PostDominatorTree() : DominatorTree() {
      DT = new llvm::DominatorTreeBase<CFGBlock>(true);
    }
};

// Ferrante, Ottenstein, Warren 1987
class ControlDependenceGraph {
  std::map<const CFGBlock*, std::vector<const CFGBlock*>> CDAdj;

  public:
    void dump() {
      llvm::errs() << "CDG\n===\n";
      for (auto Pair : CDAdj) {
        auto *Block = Pair.first;
        auto DepOn  = Pair.second;
        llvm::errs() << Block->getBlockID() << ": ";
        for (auto *Block2 : DepOn) {
          llvm::errs() << Block2->getBlockID() << ", ";
        }
        llvm::errs() << "\n";
      }
    }

    void build(AnalysisDeclContext &AC) {
      // obtain postdom tree
      CFG *CFG = AC.getCFG();
      PostDominatorTree PD;
      PD.buildDominatorTree(AC);

      // foreach edge A->B
      for (CFG::const_iterator I = CFG->begin(),
                               E = CFG->end();
                               I != E; I++) {
        const CFGBlock *A = *I;
        for (CFGBlock::const_succ_iterator I2 = A->succ_begin(),
                                           E2 = A->succ_end();
                                           I2 != E2; I2++) {
          const CFGBlock *B = *I2;
          if (not B) continue;
          if (!PD.dominates(B, A)) {
            // edge A->B where B does not postdominate A
            if (!PD.getBase().getNode(const_cast<CFGBlock*>(B))) {
              if (AllowInfiniteLoops) continue;
              llvm::errs() << "A->B (" <<
                              A->getBlockID() << "->" <<
                              B->getBlockID() << "), B not in PDT\n";
              llvm_unreachable(("A->B, B not in PDT (enable -" + std::string(AllowInfiniteLoops.ArgStr) + "?)").c_str());
            }
            // find least common dominator
            const CFGBlock *L = PD.findNearestCommonDominator(A, B);
            assert(L && "could not obtain least common dominator");
            // FOW'87 p325
            CFGBlock *AParent = PD.getBase().getNode(const_cast<CFGBlock*>(A))->getIDom()->getBlock();
            assert((L == A || L == AParent) && "L is neither A nor A's parent");

            // traverse backwards from B until we reach A's parent,
            // marking all nodes before A's parent control dependent on A
            llvm::DomTreeNodeBase<CFGBlock> *IDomB = PD.getBase().getNode(const_cast<CFGBlock*>(B));
            do {
              const CFGBlock *X = IDomB->getBlock();
              CDAdj[X].push_back(A);
              IDomB = IDomB->getIDom();
            }
            while (IDomB->getBlock() != AParent);
          }
        }
      }
      return;
    }

    bool dependsOn(const CFGBlock *A, const CFGBlock *B) const {
      if (CDAdj.find(A) == CDAdj.end())
        return false;
      for (auto Dep : CDAdj.at(A)) {
        if (Dep == B || dependsOn(Dep, B)) {
          return true;
        }
      }
      return false;
    }

    std::set<const CFGBlock *> dependsOn(const CFGBlock *A) const {
      std::set<const CFGBlock *> Result;
      std::stack<const CFGBlock*> Worklist;
      // DFS from A
      Worklist.push(A);
      while (Worklist.size()) {
        const CFGBlock *Current = Worklist.top();
        Worklist.pop();

        if (CDAdj.find(Current) == CDAdj.end())
          continue;
        for (auto Dep : CDAdj.at(Current)) {
          bool Inserted = Result.insert(Dep).second;
          if (Inserted) {
            Worklist.push(Dep);
          }
        }
      }
      return Result;
    }
};

static const std::set<const CFGBlock*> getExitingTerminatorConditions(const std::set<const CFGBlock*> Blocks) {
  std::set<const CFGBlock*> Result;
  for (auto Block : Blocks) {
    for (CFGBlock::const_succ_iterator I = Block->succ_begin(),
                                       E = Block->succ_end();
                                       I != E; I++) {
      const CFGBlock *Succ = *I;
      if (not Succ) continue;
      if (isTransitionBlock(Succ)) {
        // successor of transition block is EXIT
        assert(Succ->succ_size() == 1);
        Succ = *Succ->succ_begin();
      }
      if (Blocks.count(Succ) == 0) {
        /* could be empty, eg `for(;;)' */
        /* assert(Block->getTerminatorCondition()); */
        Result.insert(Block);
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
#undef DEBUG_TYPE
#define DEBUG_TYPE "slice"

  DEBUG(llvm::dbgs() << "Starting slice\n");

  const CFGBlock *Header = Loop.Header;
  std::set<const CFGBlock*> Tails = Loop.Tails;
  std::set<const CFGBlock*> Body = Loop.Body;

  std::set<const CFGBlock*> VisitedBlocks;
  std::set<const Stmt*> TrackedStmts;
  std::set<const VarDecl*> ControlVars(SC.Vars);
  std::set<const CFGBlock*> TrackedBlocks;
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
              DEBUG(
                llvm::dbgs() << "Tracking stmt: ";
                Stmt->printPretty(llvm::dbgs(), NULL, PrintingPolicy(LangOptions()));
                llvm::dbgs() << "\n";
              );
              // add used variables of defining substmts
              for (auto SubStmt : A.getDefiningStmts(Var)) {
                DefUseHelper C(SubStmt);
                for (auto Var : C.getUses()) {
                  ControlVars.insert(Var);
                  DEBUG(llvm::dbgs() << "Tracking var: " << Var->getNameAsString() << "\n");
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
                  DEBUG(
                    llvm::dbgs() << "Tracking control dependent statement: ";
                    Stmt->printPretty(llvm::dbgs(), NULL, PrintingPolicy(LangOptions()));
                    llvm::dbgs() << "\n";
                  );

                  DefUseHelper B(Stmt);
                  for (auto Var : B.getDefsAndUses()) {
                    ControlVars.insert(Var);
                    DEBUG(llvm::dbgs() << "Tracking control dependent var: " << Var->getNameAsString() << "\n");
                  }
                }
              }
            }
          }
        }
      }
    }
  } while (TrackedStmts.size() > OldSize);

  DEBUG(
    llvm::dbgs() << "Blocks: ";
    for (auto Block : TrackedBlocks) {
      llvm::dbgs() << Block->getBlockID();
      llvm::dbgs() << ", ";
  }
    llvm::dbgs() << "\n";
  );

  NaturalLoop *Sliced = new NaturalLoop();
  Sliced->build(Header, Tails, Body, ControlVars, &TrackedStmts, &TrackedBlocks, Unsliced);
  return Sliced;

}

static const NaturalLoop *buildNaturalLoop(
    const MergedLoopDescriptor &Loop,
    const std::set<const VarDecl*> ControlVars) {
  const CFGBlock *Header = Loop.Header;
  std::set<const CFGBlock*> Tails = Loop.Tails;
  std::set<const CFGBlock*> Body = Loop.Body;

  NaturalLoop *Unsliced = new NaturalLoop();
  Unsliced->build(Header, Tails, Body, ControlVars);
    return Unsliced;
}

static SlicingCriterion slicingCriterionOuterLoop(const MergedLoopDescriptor &Loop) {
  // collect initial control variables
  std::set<const VarDecl*> ControlVars;
  std::set<const CFGBlock*> ExitingBlocks;
  std::set<const CFGBlock*> NestedExitingBlocks = getExitingTerminatorConditions(Loop.Body);
  DEBUG(llvm::dbgs() << "start SC computation outer loop\n");
  for (auto Block : NestedExitingBlocks) {
    ExitingBlocks.insert(Block);
    const Stmt *Stmt = Block->getTerminatorCondition();
    DefUseHelper A(Stmt);
    for (auto Var : A.getDefsAndUses()) {
      if(ControlVars.insert(Var).second) {
        DEBUG(
          llvm::dbgs() << "init: " << Var->getNameAsString() << " (B";
          llvm::dbgs() << Block->getBlockID();
          llvm::dbgs() << ")\n";
        );
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
  DEBUG(llvm::dbgs() << "start SC computation all loops\n");
  for (auto NestedLoop : Loop.NestedLoops) {
    std::set<const CFGBlock*> NestedExitingBlocks = getExitingTerminatorConditions(NestedLoop->Body);
    for (auto Block : NestedExitingBlocks) {
      ExitingBlocks.insert(Block);
      const Stmt *Stmt = Block->getTerminatorCondition();
      DefUseHelper A(Stmt);
      for (auto Var : A.getDefsAndUses()) {
        if(ControlVars.insert(Var).second) {
          DEBUG(
            llvm::dbgs() << "init: " << Var->getNameAsString() << " (B";
            llvm::dbgs() << Block->getBlockID();
            llvm::dbgs() << ")\n";
          );
        }
      }
    }
  }
  return { ControlVars, ExitingBlocks };
}
#undef DEBUG_TYPE
#define DEBUG_TYPE ""

bool isOutermostNestingLoop(const MergedLoopDescriptor *D) {
  return D->NestingLoops.size() == 1;
}

class FunctionCallback : public MatchFinder::MatchCallback {
  private:
    const ASTContext *Context;
    std::unique_ptr<Classifier> C;
  public:
    FunctionCallback() : Context(NULL) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
      const FunctionDecl *D = Result.Nodes.getNodeAs<FunctionDecl>(FunctionName);
      if (!D->hasBody()) return;
      if (Function != "" and D->getNameAsString() != Function) return;

#ifdef NUM_PROCS
      // wait for a place in the process pool
      ProcCounter.wait_pool();
      // then fork
      pid_t pid = fork();
      if (pid == -1) {
        llvm::errs() << "can't fork, error " << errno << "\n";
        exit(EXIT_FAILURE);
      } else if (pid == 0) {
        // child
      } else {
        // parent
        return;
      }
#endif

      DEBUG(
          llvm::dbgs() << "Processing: " << Result.SourceManager->getPresumedLoc(D->getLocation()).getFilename() << " " << D->getNameAsString() << "\n";
          llvm::dbgs().flush();
      );

      std::map<const CFGBlock*, std::vector<LoopDescriptor>> Loops;

      AnalysisDeclContextManager mgr;
      AnalysisDeclContext *AC = mgr.getContext(D);
      CFG *CFG = AC->getCFG();

      if (ViewCFG) CFG->viewCFG(LangOptions());

      ControlDependenceGraph CDG;
      CDG.build(*AC);
      if (DumpCDG) CDG.dump();

      DominatorTree Dom;
      Dom.buildDominatorTree(*AC);

      PostDominatorTree PostDom;
      PostDom.buildDominatorTree(*AC);

      for (CFG::const_iterator it = CFG->begin(), end = CFG->end(); it != end; it++) {
        const CFGBlock *Tail = *it;
        for (CFGBlock::const_succ_iterator it2 = Tail->succ_begin(); it2 != Tail->succ_end(); it2++) {
          const CFGBlock *Header = *it2;
          // if Tail -> Header is a back edge
          if (Dom.dominates(Header, Tail) and
              Dom.isReachableFromEntry(Tail)) {  // Unreachable nodes are dominated by everything
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

            bool IsTriviallyNonterminating = false;
            if (not PostDom.dominates(Header, Tail)) {
              IsTriviallyNonterminating = true;
            }

            Loops[Header].push_back({Header, Tail, Body, IsTriviallyNonterminating});
          }
        }
      }


      // Merge loops with the same header.
      std::vector<MergedLoopDescriptor> LoopsAfterMerging;
      for (auto L : Loops) {
        const CFGBlock *Header = L.first;
        auto List = L.second;
        bool IsTriviallyNonterminating = true;
        
        std::set<const CFGBlock*> MergedBody;
        std::set<const CFGBlock*> MergedTails;
        for (auto L2 : List) {
          for (const CFGBlock *Block : L2.Body) {
            MergedBody.insert(Block);
          }
          MergedTails.insert(L2.Tail);
          IsTriviallyNonterminating = IsTriviallyNonterminating and L2.IsTriviallyNonterminating;
        }
        LoopsAfterMerging.push_back(MergedLoopDescriptor(Header, MergedTails, MergedBody, IsTriviallyNonterminating));
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

#undef DEBUG_TYPE
#define DEBUG_TYPE "buildNaturalLoop"

      std::map<MergedLoopDescriptor, std::vector<const NaturalLoop*>> M;
      for (auto &Loop : LoopsAfterMerging) {
        auto SC = slicingCriterionAllLoops(Loop);
        DEBUG(llvm::dbgs() << "build unsliced\n");
        const NaturalLoop *Unsliced = buildNaturalLoop(Loop, SC.Vars);
        if (!Unsliced) continue;
        /* Unsliced->view(); */
        DEBUG(llvm::dbgs() << "build sliced all\n");
        const NaturalLoop *SlicedAllLoops = buildNaturalLoop(Loop, Unsliced, CDG, SC);
        /* SlicedAllLoops->view(); */
        DEBUG(llvm::dbgs() << "build sliced outer\n");
        const NaturalLoop *SlicedOuterLoop = buildNaturalLoop(Loop, Unsliced, CDG, slicingCriterionOuterLoop(Loop));

#undef DEBUG_TYPE
#define DEBUG_TYPE ""

        auto LocationID = Unsliced->getLoopStmtID(Result.SourceManager);
        std::stringstream sstm;
        sstm << LocationID.begin()->getFilename()
             << " -func " << D->getNameAsString()
             << " -lines ";
        std::set<unsigned> Lines;
        for (auto PLBack : LocationID) {
          Lines.insert(PLBack.getLine());
        }
        for (auto Line : Lines) {
          sstm << Line << ",";
        }
        LoopLocationMap[Unsliced] = sstm.str();

        M[Loop].push_back(Unsliced);
        M[Loop].push_back(SlicedAllLoops);
        M[Loop].push_back(SlicedOuterLoop);
      }

      // the ast context doesn't change that often; cache it
      if (Context != Result.Context) {
        Context = Result.Context;
        C.reset(new Classifier(Result.Context));
      }
      for (auto Pair : M) {
        auto MLD = Pair.first;
        const NaturalLoop *Unsliced = M[MLD][0];
        const NaturalLoop *SlicedAllLoops = M[MLD][1];
        const NaturalLoop *SlicedOuterLoop = M[MLD][2];

        auto LocationID = Unsliced->getLoopStmtID(Result.SourceManager);
        if (isSpecified(D, LocationID)) {
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
          if (ViewSliced || ViewSlicedOuter || ViewUnsliced or
              DumpSliced || DumpSlicedOuter || DumpUnsliced) {
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

            if (DumpSliced) SlicedAllLoops->dump();
            if (DumpSlicedOuter) SlicedOuterLoop->dump();
            if (DumpUnsliced) Unsliced->dump();
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

        C->classify(isSpecified(D, LocationID), Unsliced, SlicedAllLoops, SlicedOuterLoop, OutermostNestingLoop, NestingLoops, ProperlyNestedLoops);
      }

      for (auto Pair : M) {
        auto MLD = Pair.first;
        auto NestingLoops = MLD.NestingLoops;
        for (const auto NestingLoop : NestingLoops) {
          if (not LoopClassifier::hasClass(M[*NestingLoop][0], "Proved")) {
            goto next_loop;
          }
        }
        LoopClassifier::classify(Pair.second[0], "FinitePaths");
next_loop:
        ;
        if (MLD.IsTriviallyNonterminating) {
          LoopClassifier::classify(Pair.second[0], "TriviallyNonterminating");
        }
      }

      // InfluencesOuter is only classified when we reach the outer loop,
      // so we have to loop once again to show all classes.
      for (auto Pair : M) {
        auto MLD = Pair.first;
        const NaturalLoop *Unsliced = M[MLD][0];
        auto LocationID = Unsliced->getLoopStmtID(Result.SourceManager);

        if (isSpecified(D, LocationID)) {
          if ((HasClass == std::string() && !LoopStats) ||
              (HasClass != std::string() && LoopClassifier::hasClass(Unsliced, HasClass))) {
            llvm::errs() << LoopLocationMap[Unsliced] << "\n";
            if (DumpClasses || DumpClassesAll) {
              dumpClasses(llvm::errs(), Classifications[Unsliced]);
            }
          }
          if (DumpBlocks || DumpControlVars || DumpControlVarsDetail || DumpClasses || DumpClassesAll || DumpAST || DumpStmt || DumpIncrementVars) {
            llvm::errs() << "----------\n";
          }
        }
      }

      for (auto Pair : M) {
        auto MLD = Pair.first;
        /* const NaturalLoop *Unsliced = M[MLD][0]; */
        const NaturalLoop *SlicedAllLoops = M[MLD][1];
        const NaturalLoop *SlicedOuterLoop = M[MLD][2];
        delete SlicedAllLoops;
        delete SlicedOuterLoop;
      }

#ifdef NUM_PROCS
      _exit(0);  /* Note that we do not use exit() */
#endif
    }
};
