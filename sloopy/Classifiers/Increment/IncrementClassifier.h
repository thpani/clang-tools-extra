#pragma once

#include <exception>

#include "llvm/ADT/BitVector.h"

#include "LoopClassifier.h"
#include "ADT.h"
#include "Helpers.h"
#include "LinearHelper.h"

using namespace sloopy::z3helper;

class IncrementClassifier : public LoopClassifier {
  private:
    mutable std::vector<PseudoConstantInfo> PseudoConstantSet;

    class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
      public:
        LoopVariableFinder(const IncrementClassifier *Outer) : Outer(Outer) {}
        const std::vector<IncrementInfo> findIncrements (const NaturalLoopBlock *Block) {
          LoopVarCandidates.clear();
          this->TraverseStmt(const_cast<Expr*>(Block->getTerminatorCondition()));
          for (auto S : *Block) {
            this->TraverseStmt(const_cast<Stmt*>(S));
          }
          return LoopVarCandidates;
        }
        const std::set<IncrementInfo> findLoopVarCandidates(const NaturalLoopBlock *Block) {
          auto Increments = findIncrements(Block);
          return std::set<IncrementInfo>(Increments.begin(), Increments.end());
        }
        const std::vector<IncrementInfo> findIncrements(const NaturalLoop *Loop) {
          LoopVarCandidates.clear();
          for (auto Block : *Loop) {
            this->TraverseStmt(const_cast<Expr*>(Block->getTerminatorCondition()));
            for (auto S : *Block) {
              this->TraverseStmt(const_cast<Stmt*>(S));
            }
          }
          return LoopVarCandidates;
        }
        const std::set<IncrementInfo> findLoopVarCandidates(const NaturalLoop *Loop) {
          auto Increments = findIncrements(Loop);
          return std::set<IncrementInfo>(Increments.begin(), Increments.end());
        }

        bool VisitExpr(Expr *Expr) {
          auto I = Outer->getIncrementInfo(Expr);
          if (I.which() == 1) {
            LoopVarCandidates.push_back(boost::get<IncrementInfo>(I));
          }
          return true;
        }

      private:
        std::vector<IncrementInfo> LoopVarCandidates;
        const IncrementClassifier *Outer;
    };

    class InvariantVarFinder : public RecursiveASTVisitor<InvariantVarFinder> {
      const NaturalLoop * const Loop;
      std::map<const VarDecl*, std::vector<const Stmt*>> NonInvStmts;
      std::set<const VarDecl*> Variables, NonInv, Inv;
      bool AnalysisRun = false;

      void RunAnalysis() {
        // Collect all variables
        for (auto Block : *Loop) {
          this->TraverseStmt(const_cast<Expr*>(Block->getTerminatorCondition()));
          for (auto S : *Block) {
            this->TraverseStmt(const_cast<Stmt*>(S));
          }
        }

        // Find those non-invariant
        // TODO assignment to invariant value
        for (auto Block : *Loop) {
          if (const Expr *Cond = Block->getTerminatorCondition()) {
            DefUseHelper A(Cond);
            for (const VarDecl *VD : Variables) {
              if (A.isDef(VD)) {
                NonInv.insert(VD);
                auto Defs = A.getDefiningStmts(VD);
                NonInvStmts[VD].insert(NonInvStmts[VD].end(), Defs.begin(), Defs.end());
              }
            }
          }
          for (auto Stmt : *Block) {
            DefUseHelper A(Stmt);
            for (const VarDecl *VD : Variables) {
              if (A.isDef(VD)) {
                NonInv.insert(VD);
                auto Defs = A.getDefiningStmts(VD);
                NonInvStmts[VD].insert(NonInvStmts[VD].end(), Defs.begin(), Defs.end());
              }
            }
          }
        }

        std::set_difference(Variables.begin(), Variables.end(), NonInv.begin(), NonInv.end(), std::inserter(Inv, Inv.end()));
        AnalysisRun = true;
      }

      public:
        InvariantVarFinder(const NaturalLoop *Loop) : Loop(Loop) {}

        bool VisitDeclRefExpr(DeclRefExpr *DRE) {
          if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
            Variables.insert(VD);
          }
          return true;
        }

        bool isInvariant(const VarDecl *VD) {
          if (!AnalysisRun) RunAnalysis();
          return Inv.count(VD) > 0;
        }

        typedef std::vector<const Stmt*>::iterator def_stmt_iter;
        def_stmt_iter def_stmt_begin(const VarDecl* VD) {
          if (!AnalysisRun) RunAnalysis();
          return NonInvStmts[VD].begin();
        }
        def_stmt_iter def_stmt_end(const VarDecl* VD) {
          if (!AnalysisRun) RunAnalysis();
          return NonInvStmts[VD].end();
        }
    };

    void addPseudoConstantVar(const std::string Name, const VarDecl *Var) const {
      const PseudoConstantInfo I = { Name, Var };
      PseudoConstantSet.push_back(I);
    }

    void checkPseudoConstantSet(const NaturalLoop *L) const throw (checkerror) {
      InvariantVarFinder F(L);
      for (auto IncrementElement : PseudoConstantSet) {
        if (!F.isInvariant(IncrementElement.Var)) {
          throw checkerror(IncrementElement.Name+"_ASSIGNED");
        }
      }
    }

    bool someTermCondOnEachPath(const NaturalLoop *L, std::set<const NaturalLoopBlock*> ProvablyTerminatingBlocks) const throw () {
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

      return In[Header];
    }

    struct CheckBodyResult {
      unsigned MaxAssignments;
      unsigned MinAssignments;
      IncrementSet AccumulatedIncrement;

      bool operator==(const CheckBodyResult &Other) const {
        return MaxAssignments == Other.MaxAssignments and
               MinAssignments == Other.MinAssignments and
               AccumulatedIncrement == Other.AccumulatedIncrement;
      }
      bool operator!=(const CheckBodyResult &Other) const {
        return not ((*this) == Other);
      }
    };

    CheckBodyResult checkBody(const NaturalLoop *L, const IncrementInfo I) const throw () {
#undef DEBUG_TYPE
#define DEBUG_TYPE "checkBody"
      const NaturalLoopBlock *Header = *L->getEntry().succ_begin();
      LoopVariableFinder Finder(this);

      std::map<const NaturalLoopBlock *, unsigned> IncrementCount;
      std::map<const NaturalLoopBlock *, AugInt> AccumulatedIncrement;
      for (auto Block : *L) {
        for (const IncrementInfo Increment : Finder.findIncrements(Block)) {
          if (Increment.VD == I.VD) {
            IncrementCount[Block]++;
            if (Increment.Delta.isInt()) {
              AccumulatedIncrement[Block] += Increment.Delta.Int.getSExtValue();
            } else {
              AccumulatedIncrement[Block].setUnknown();
            }
          }
        }
      }

      std::map<const NaturalLoopBlock *, CheckBodyResult> In, Out;

      // Initialize all blocks    (1) + (2)
      std::stack<const NaturalLoopBlock*> Worklist;
      for (auto Block : *L) {
        /* Instead of initializing blocks with 0 and pushing them to the worklist,
         * anticipate the first iteration and push their successsors.
         * This way we don't need to compute f_HEADER(x) on the first iteration.
         */
        Out[Block] = { IncrementCount[Block], IncrementCount[Block], { AccumulatedIncrement[Block] } };
        DEBUG(
          llvm::dbgs() << "init Out[" << Block->getBlockID() << "]: ";
          for (auto X : Out[Block].AccumulatedIncrement)
            llvm::dbgs() << X.str() << ",";
          llvm::dbgs() << "\n";
        );
        if (IncrementCount[Block] or AccumulatedIncrement[Block].isUnknown() or AccumulatedIncrement[Block] != 0) {
          for (NaturalLoopBlock::const_succ_iterator I = Block->succ_begin(),
                                                     E = Block->succ_end();
                                                     I != E; I++) {
            const NaturalLoopBlock *Succ = *I;
            if (Succ) Worklist.push(Succ);
          }
        }
      }

      std::set<const NaturalLoopBlock *> Visited;

      // while OUT changes
      while (Worklist.size()) {
        const NaturalLoopBlock *Block = Worklist.top();
        Worklist.pop();

        DEBUG( llvm::dbgs() << "Processing worklist item " << Block->getBlockID() << "\n" );

        // meet (propagate OUT -> IN)
        unsigned max = 0;
        unsigned min = std::numeric_limits<unsigned>::max();
        std::set<AugInt, AugComp> allIncs;
        for (NaturalLoopBlock::const_pred_iterator P = Block->pred_begin(),
                                                    E = Block->pred_end();
                                                    P != E; P++) {    // (5)
          const NaturalLoopBlock *Pred = *P;
          if (Pred == &L->getEntry()) continue;
          DEBUG( llvm::dbgs() << "propagation from WL item's pred " << Pred->getBlockID() << "\n" );
          DEBUG(
            llvm::dbgs() << "\tOut[" << Block->getBlockID() << "]: ";
            for (auto X : Out[Block].AccumulatedIncrement)
              llvm::dbgs() << X.str() << ",";
            llvm::dbgs() << "\n";
          );
          max = Out[Pred].MaxAssignments > max ? Out[Pred].MaxAssignments : max;
          min = Out[Pred].MinAssignments < min ? Out[Pred].MinAssignments : min;
          allIncs.insert(Out[Pred].AccumulatedIncrement.begin(), Out[Pred].AccumulatedIncrement.end());
        }
        In[Block] = { max, min, allIncs };

        DEBUG(
          llvm::dbgs() << "In[" << Block->getBlockID() << "]: ";
          for (auto X : In[Block].AccumulatedIncrement)
            llvm::dbgs() << X.str() << ",";
          llvm::dbgs() << "\n";
        );

        if (Block == Header) continue;

        Visited.insert(Block);
        bool loop = false;
        for (NaturalLoopBlock::const_succ_iterator I = Block->succ_begin(),
                                                   E = Block->succ_end();
                                                   I != E; I++) {
          const NaturalLoopBlock *Succ = *I;
          if (Visited.count(Succ)) {
            loop = true;
          }
        }

        // compute OUT / f_B(x)
        std::set<AugInt, AugComp> allIncsOut;
        if (loop) {
          allIncsOut = { AugInt::UnknownAugInt() };
        } else {
          for (auto incIn : allIncs) {
            allIncsOut.insert(incIn + AccumulatedIncrement[Block]);
          }
        }
        unsigned n = IncrementCount[Block];
        CheckBodyResult NewOut = {  // (6)
          std::min(2u, In[Block].MaxAssignments + n),
          std::min(2u, In[Block].MinAssignments + n),
          allIncsOut
        };

        if (Out[Block] != NewOut) {
          for (NaturalLoopBlock::const_succ_iterator I = Block->succ_begin(),
                                                     E = Block->succ_end();
                                                     I != E; I++) {
            const NaturalLoopBlock *Succ = *I;
            if (Succ) Worklist.push(Succ);
          }
        }

        Out[Block] = NewOut;

        DEBUG(
          llvm::dbgs() << "Out[" << Block->getBlockID() << "]: ";
          for (auto X : Out[Block].AccumulatedIncrement)
            llvm::dbgs() << X.str() << ",";
          llvm::dbgs() << "\n";
        );
      }
      DEBUG(
        llvm::dbgs() << "----\nIn[Header]: ";
        for (auto X : In[Header].AccumulatedIncrement)
          llvm::dbgs() << X.str() << ",";
        llvm::dbgs() << "\n===\n";
      );

      return In[Header];
#undef DEBUG_TYPE
#define DEBUG_TYPE ""
    }

  protected:
    const std::string Marker;
    const ASTContext *Context;

    virtual boost::variant<std::string,IncrementInfo> getIncrementInfo(const Stmt *Stmt) const throw () = 0;
    virtual std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo I) const throw (checkerror) = 0;

  public:
    IncrementClassifier(const std::string Marker, const ASTContext *Context) :
      LoopClassifier(), Marker(Marker), Context(Context) {}
    virtual ~IncrementClassifier() {}

    std::pair<std::set<const NaturalLoopBlock*>, std::map<const NaturalLoopBlock*, llvm::BitVector>>
     classifyProve(const NaturalLoop *Loop) const throw () {
#undef DEBUG_TYPE
#define DEBUG_TYPE "prove"
      LoopVariableFinder Finder(this);
      const std::set<IncrementInfo> LoopVarCandidates = Finder.findLoopVarCandidates(Loop);
      DEBUG( llvm::dbgs() << "Number of loop var candidates: " << LoopVarCandidates.size() << "\n"; );

      InvariantVarFinder F(Loop);

      // restrict loop var candidates to those incremented on each path
      std::set<IncrementInfo> LoopVarCandidatesEachPath;
      for (const IncrementInfo I : LoopVarCandidates) {
        DEBUG( llvm::dbgs() << "Checking candidate " << I.VD->getNameAsString() << "... " );
        auto MaxMin = checkBody(Loop, I);
        if (MaxMin.MinAssignments < 1) {
          // there must be >= 1 assignments on each path
          continue;
          DEBUG( llvm::dbgs() << "fail. <1 assignment on each path\n" );
        }
        bool iAssignedOutsideIncrement = false;
        for (InvariantVarFinder::def_stmt_iter P = F.def_stmt_begin(I.VD),
                                               E = F.def_stmt_begin(I.VD);
                                               P != E; P++) {
          const Stmt* S = *P;

          bool sIsIncrement = false;
          for (const IncrementInfo J : LoopVarCandidates) {
            if (J.VD == I.VD and S == J.Statement) {
              sIsIncrement = true;
              break;
            }
          }

          if (not sIsIncrement) {
            iAssignedOutsideIncrement = true;
            break;
          }
        }
        if (iAssignedOutsideIncrement) {
          continue;
          DEBUG( llvm::dbgs() << "fail. assigned outside increment\n" );
        }
        DEBUG( llvm::dbgs() << "ok\n" );
        LoopVarCandidatesEachPath.insert(I);
      }

      std::set<const NaturalLoopBlock*> ProvablyTerminatingBlocks;
      std::map<const NaturalLoopBlock*, llvm::BitVector> AssumptionMap;
      llvm::BitVector Assumption(LinearHelper::AssumptionSize);
      // find provably terminating blocks
      for (NaturalLoopBlock::const_pred_iterator PI = Loop->getExit().pred_begin(),
                                                 PE = Loop->getExit().pred_end();
                                                 PI != PE; PI++) {
        const NaturalLoopBlock *Block = *PI;

        assert(Block->getTerminator() && "pred of EXIT has no terminator stmt");

        // TODO dealing with switch/case is more complex (compare labels):
        if (Block->getTerminator()->getStmtClass() == Stmt::SwitchStmtClass) {
          continue;
        }
        
        const Expr *Cond = Block->getTerminatorCondition();

        bool foundExit = false;
        unsigned whichBranch = 0;
        for (NaturalLoopBlock::const_succ_iterator SI = Block->succ_begin(),
                                                   SE = Block->succ_end();
                                                   SI != SE; SI++) {
          const NaturalLoopBlock *Block = *SI;
          if (Block == (&Loop->getExit())) {
            foundExit = true;
            break;
          }

          whichBranch++;
        }
        assert(foundExit);
        assert(0 <= whichBranch and whichBranch <= 1);
        DEBUG( 
            llvm::dbgs() << "Trying to prove condition\n";
            Cond->printPretty(llvm::dbgs(), NULL, PrintingPolicy(LangOptions())); llvm::dbgs() << "\n";
            llvm::dbgs() << "on branch: " << (not whichBranch) << "\n"
                         << "for " << LoopVarCandidatesEachPath.size() << " candidate variables.\n";
        );
        for (const IncrementInfo I : LoopVarCandidatesEachPath) {
          LinearHelper H;

          auto MaxMin = checkBody(Loop, I);
          DEBUG( llvm::dbgs() << I.VD->getNameAsString() << " IncrementSize: " << MaxMin.AccumulatedIncrement.size() << "\n" );

          // see if we can find a proof
          if (whichBranch == 1) {
            // cond needs to become false to exit the loop
            if (not H.dropsToZero(I.VD, Cond, MaxMin.AccumulatedIncrement)) {
              continue;
            }
          } else if (whichBranch == 0) {
            // cond needs to become true to exit the loop
            if (not H.dropsToZero(I.VD, Cond, MaxMin.AccumulatedIncrement, true)) {
              continue;
            }
          } else {
            llvm_unreachable("exit branch >1");
          }

          // check if all constants are loop-invariant
          bool AllVarsConst = true;
          for (auto VD : H.getConstants()) {
            if (VD == I.VD) continue;
            AllVarsConst = AllVarsConst and F.isInvariant(VD);
            if (!AllVarsConst) {
              DEBUG( llvm::dbgs() << VD->getNameAsString() << " is not const\n" );
              break;
            }
          }
          if (!AllVarsConst) {
            continue;
          }

          auto A = H.getAssumptions();
          assert(Assumption.size() == A.size() and "bitvectors should be same size");
          AssumptionMap[Block] = Assumption;
          ProvablyTerminatingBlocks.insert(Block);
        }
      }
      DEBUG( llvm::dbgs() << "ProvablyTerminatingBlocks: " << ProvablyTerminatingBlocks.size() << "\n" );

      return std::make_pair(ProvablyTerminatingBlocks, AssumptionMap);
#undef DEBUG_TYPE
#define DEBUG_TYPE ""
    }

    std::set<IncrementLoopInfo> classify(const NaturalLoop *Loop, const IncrementClassifierConstraint Constr) const throw () {
        // do we have the right # of exit arcs?
        unsigned PredSize = Loop->getExit().pred_size();
        if (Constr.ECConstr != ANY_EXIT && PredSize != Constr.ECConstr) {
          LoopClassifier::classify(Loop, Constr.str(), Marker, "WrongExitArcs", false);
          return std::set<IncrementLoopInfo>();
        }
        if (PredSize == 0) {
          LoopClassifier::classify(Loop, Constr.str(), Marker, "NoExitArcs", false);
          return std::set<IncrementLoopInfo>();
        }

        // are there any increments in this loop?
        LoopVariableFinder Finder(this);
        const std::set<IncrementInfo> LoopVarCandidates = Finder.findLoopVarCandidates(Loop);
        if (LoopVarCandidates.size() == 0) {
          LoopClassifier::classify(Loop, Constr.str(), Marker, "NoLoopVarCandidate", false);
          return std::set<IncrementLoopInfo>();
        }

        std::set<std::string> Reasons;  // why the (IncrementInfo, Cond) pairs aren't wellformed
        std::set<IncrementLoopInfo> WellformedIncrements;
        std::set<std::string> Suffixes;
        for (NaturalLoopBlock::const_pred_iterator PI = Loop->getExit().pred_begin(),
                                                  PE = Loop->getExit().pred_end();
                                                  PI != PE; PI++) {
          bool WellformedIncrement = false;
          for (const IncrementInfo I : LoopVarCandidates) {
            try {
              const Expr *Cond = (*PI)->getTerminatorCondition();

              // increments on each path?
              if (Constr.IConstr == EACH_PATH) {
                auto pair = checkBody(Loop, I);
                if (pair.MaxAssignments > 1)
                    throw checkerror("ASSIGNED_Twice");
                if (pair.MinAssignments < 1)
                    throw checkerror("ASSIGNED_NotAllPaths");
              }

              // condition well formed?
              std::pair<std::string, VarDeclIntPair> result;
              if (Constr.EWConstr > ANY_EXITCOND) {
                result = checkCond(Cond, I);
              } else {
                result = std::make_pair("", VarDeclIntPair());
              }
              std::string Suffix = result.first;
              VarDeclIntPair Bound = result.second;

              // are variables in condition pseudo-const modulo increments?
              assert((Constr.EWConstr == ANY_EXITCOND || Cond) && "SomeWellformed constraint => condition");
              if (Cond) {
                PseudoConstantSet.clear();
                DefUseHelper CondDUH(Cond);
                for (auto VD : CondDUH.getDefsAndUses()) {
                  if (VD == I.VD) continue;
                  std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta.Var ? "D" : "X");
                  /* std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta ? "D" : VD->getNameAsString()); */
                  addPseudoConstantVar(name, VD);
                }
                if (Constr.IConstr == SOME_PATH && I.Delta.Var) {
                  addPseudoConstantVar("D", I.Delta.Var);
                }
                
                checkPseudoConstantSet(Loop);
              }

              // save everything
              IncrementLoopInfo ILI = {I.VD, I.Statement, I.Delta, Bound};
              WellformedIncrements.insert(ILI);
              WellformedIncrement = true;
              Suffixes.insert(Suffix);

              // one is enough
              /* goto next_exit; */
              // we collect (increment, bound) pairs because AmortB needs the bound
            } catch(checkerror &e) {
              Reasons.insert(e.what());
            }
          }
        }

        // We found well-formed increments
        if (WellformedIncrements.size()) {
          std::set<const VarDecl*> Counters;
          for (auto ILI : WellformedIncrements) {
            Counters.insert(ILI.VD);
          }
          const unsigned CounterSetSize = Counters.size();
          LoopClassifier::classify(Loop, Constr.str(), Marker+"Counters", CounterSetSize);

          std::stringstream Suffix;
          for (std::set<std::string>::const_iterator I = Suffixes.begin(),
                                                    E = Suffixes.end();
                                                    I != E; I++) {
            Suffix << *I;
            if (std::next(I) != E) Suffix << "-";
          }
          LoopClassifier::classify(Loop, Constr.str(), Marker, Suffix.str());
          return WellformedIncrements;
        }

        // We didn't find well formed increments
        std::stringstream Reason;
        for (std::set<std::string>::const_iterator I = Reasons.begin(),
                                                  E = Reasons.end();
                                                  I != E; I++) {
          Reason << *I;
          if (std::next(I) != E) Reason << "-";
        }
        assert(Reason.str().size());

        LoopClassifier::classify(Loop, Constr.str(), Marker, Reason.str(), false);

      return std::set<IncrementLoopInfo>();
    }
};
