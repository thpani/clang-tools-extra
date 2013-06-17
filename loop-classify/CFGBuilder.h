#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CFG_BUILDER_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CFG_BUILDER_H_

#include <sys/types.h> /* pid_t */
#include <unistd.h>  /* _exit, fork */

#include <stack>

#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "Loop.h"
#include "LoopMatchers.h"
#include "DefUse.h"
#include "Classifiers/Ada.h"
#include "Classifiers/AdaArray.h"
#include "Classifiers/Branch.h"
#include "Classifiers/DataIter.h"
#include "Classifiers/ArrayIter.h"
/* #include "Classifiers/EmptyBody.h" */
/* #include "Classifiers/Cond.h" */

using namespace clang;
using namespace clang::ast_matchers;

using namespace sloopy;

llvm::cl::opt<bool> LoopStats("loop-stats");
llvm::cl::opt<bool> IgnoreInfiniteLoops("ignore-infinite-loops");
llvm::cl::opt<bool> ViewSlice("view-slice");
llvm::cl::opt<bool> DumpControlVars("dump-control-vars");
llvm::cl::opt<bool> DumpClasses("dump-classes");

std::map<const NaturalLoop*, std::string> LoopLocationMap;

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

    bool dependsOn(const CFGBlock *A, const CFGBlock *B) {
      std::map<const CFGBlock*, std::vector<const CFGBlock*>>::iterator I;
      if ((I = Map.find(A)) != Map.end()) {
        for (auto Dep : I->second) {
          if (Dep == B || dependsOn(Dep, B)) return true;
        }
      }
      return false;
    }

    std::set<const CFGBlock *> dependsOn(const CFGBlock *A) {
      std::set<const CFGBlock *> Result;
      std::map<const CFGBlock*, std::vector<const CFGBlock*>>::iterator I;
      std::stack<const CFGBlock*> Worklist;
      Worklist.push(A);
      while (Worklist.size()) {
        const CFGBlock *Current = Worklist.top();
        Worklist.pop();

        if ((I = Map.find(Current)) != Map.end()) {
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

static const std::set<const CFGBlock*> getExitingTerminatorConditions(const std::set<const CFGBlock*> Blocks) {
  std::set<const CFGBlock*> Result;
  for (auto Block : Blocks) {
    for (CFGBlock::const_succ_iterator I = Block->succ_begin(),
                                        E = Block->succ_end();
                                        I != E; I++) {
      const CFGBlock *Succ = *I;
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

class FunctionCallback : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result) {

      const FunctionDecl *D = Result.Nodes.getNodeAs<FunctionDecl>(FunctionName);
      if (!D->hasBody()) return;

      std::set<const NaturalLoop> Loops;

      AnalysisDeclContextManager mgr;
      AnalysisDeclContext *AC = mgr.getContext(D);
      CFG *CFG = AC->getCFG();
      DominatorTree Dom;
      Dom.buildDominatorTree(*AC);
      ControlDependenceGraph CDG;
      if (!CDG.buildControlDependenceGraph(*AC)) return;

      for (CFG::const_iterator it = CFG->begin(); it != CFG->end(); it++) {
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

            // compute a fixed point of the program slice

            // collect initial control variables
            std::set<const VarDecl*> ControlVars;
            auto ExitingBlocks = getExitingTerminatorConditions(Body);
            for (auto Block : ExitingBlocks) {
              const Stmt *Stmt = Block->getTerminatorCondition();
              DefUseHelper A(Stmt);
              for (auto Var : A.getDefsAndUses()) {
                ControlVars.insert(Var);
                if (DumpControlVars) {
                  llvm::errs() << "init: " << Var->getNameAsString() << "\n";
                }
              }
            }

            std::set<const CFGBlock*> VisitedBlocks;
            std::set<const Stmt*> TrackedStmts;
            std::set<const CFGBlock*> TrackedBlocks(ExitingBlocks);
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
                  // a statement in the loop
                  /* for (auto SubStmt : Stmt) { */
                    DefUseHelper A(Stmt);
                    for (auto Var : ControlVars) {
                      if (A.isDef(Var)) {
                        // if one of the control vars is modified in this stmt, track the stmt
                        if (TrackedStmts.insert(Stmt).second) {
                          if (DumpControlVars) {
                            llvm::errs() << "1s: ";
                            Stmt->printPretty(llvm::errs(), NULL, PrintingPolicy(LangOptions()));
                            llvm::errs() << "\n";
                          }
                          // add used variables of defining substmts
                          for (auto SubStmt : A.getDefiningStmts(Var)) {
                            DefUseHelper C(SubStmt);
                            for (auto Var : C.getUses()) {
                              ControlVars.insert(Var);
                              if (DumpControlVars) {
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
                              if (DumpControlVars) {
                                llvm::errs() << "2s: ";
                                Stmt->printPretty(llvm::errs(), NULL, PrintingPolicy(LangOptions()));
                                llvm::errs() << "\n";
                              }

                              DefUseHelper B(Stmt);
                              for (auto Var : B.getDefsAndUses()) {
                                ControlVars.insert(Var);
                                if (DumpControlVars) {
                                  /* Stmt->dump(); */
                                  llvm::errs() << "2: " << Var->getNameAsString() << "\n";
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  /* } */
                }
              }
            } while (TrackedStmts.size() > OldSize);

            if (DumpControlVars) {
              for (auto VD : ControlVars) {
                llvm::errs() << "Control variable: " << VD->getNameAsString() << "\n";
              }
            }

            NaturalLoop *Loop = new NaturalLoop();
            NaturalLoop *Loop2 = new NaturalLoop();
            if (Loop->build(Header, Tail, Body, ControlVars, &TrackedStmts, &TrackedBlocks)) {
              if (ViewSlice) Loop->view();
              
              SourceLocation SL = Loop->getLoopStmt()->getSourceRange().getBegin();
              PresumedLoc PL = Result.SourceManager->getPresumedLoc(SL);
              std::stringstream sstm;
              sstm << PL.getLine() << " " << D->getNameAsString() << " " << PL.getFilename();
              LoopLocationMap[Loop] = sstm.str();

              Loop2->build(Header, Tail, Body, ControlVars);
              NaturalLoopPair P = { Loop, Loop2 };

              // Any
              AnyLoopCounter ALC;
              ALC.classify(Loop);

              // Statements
              LoopClassifier LoopCounter;
              LoopCounter.classify(Loop);

              // Branching
              BranchingClassifier B;
              B.classify(P);

              // Simple Plans
              DataIterClassifier D;
              D.classify(P);
              AdaArrayForLoopClassifier AA(Result.Context);
              bool isAdaArray = AA.classify(P);
              if (!isAdaArray) {
                AdaForLoopClassifier A(Result.Context);
                A.classify(P);
              }
              ArrayIterClassifier Ar(Result.Context);
              Ar.classify(P);

              // Simple control flow
              SimpleLoopCounter SLC;
              SLC.classify(Loop);

              if (DumpClasses) {
                for (auto Class : Classifications[Loop]) {
                  llvm::errs() << Class << " ";
                }
                llvm::errs() << "\n";
              }
            }

            if (ViewSlice || DumpControlVars) {
              llvm::errs() << "----------\n";
            }
            /* pid_t pid = fork(); */
            /* if (pid == 0) { */
            /*   Loop.view(); */
            /*   _exit(0); */
            /* } */
            /* Loop2.view(); */
          }
        }
      }
    }
};

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_CFG_BUILDER_H_
