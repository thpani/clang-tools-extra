#pragma once

#include "LoopClassifier.h"
#include "ADT.h"
#include "Helpers.h"

class IncrementClassifier : public LoopClassifier {
  private:
    mutable std::vector<const PseudoConstantInfo> PseudoConstantSet;

    class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
      public:
        LoopVariableFinder(const IncrementClassifier *Outer) : Outer(Outer) {}
        const std::set<IncrementInfo> findLoopVarCandidates(const NaturalLoop *Loop) {
          for (auto Block : *Loop) {
            this->TraverseStmt(const_cast<Expr*>(Block->getTerminatorCondition()));
            for (auto S : *Block) {
              this->TraverseStmt(const_cast<Stmt*>(S));
            }
          }
          return LoopVarCandidates;
        }

        bool VisitExpr(Expr *Expr) {
          try {
            const IncrementInfo I = Outer->getIncrementInfo(Expr);
            LoopVarCandidates.insert(I);
          } catch(checkerror) {}
          return true;
        }

      private:
        std::set<IncrementInfo> LoopVarCandidates;
        const IncrementClassifier *Outer;
    };

    void addPseudoConstantVar(const std::string Name, const VarDecl *Var, const Stmt *IncrementOp=NULL) const {
      const PseudoConstantInfo I = { Name, Var, IncrementOp };
      PseudoConstantSet.push_back(I);
    }

    void checkPseudoConstantSet(const NaturalLoop *L) const throw (checkerror) {
      for (auto Block : *L) {
        for (auto Stmt : *Block) {
          DefUseHelper A(Stmt);
          for (auto IncrementElement : PseudoConstantSet) {
            if (A.isDefExclDecl(IncrementElement.Var, IncrementElement.IncrementOp)) {
              throw checkerror(Fail, Marker, IncrementElement.Name+"_ASSIGNED");
            }
          }
        }
      }
    }


    void checkBody(const NaturalLoop *L, const IncrementInfo I) const throw (checkerror) {
      const NaturalLoopBlock *Header = *L->getEntry().succ_begin();

      // Initialize all blocks
      std::map<const NaturalLoopBlock *, unsigned> In, Out, OldOut;
      for (auto Block : *L) {     // (2)
        Out[Block] = 0;
      }

      // Initialize header
      unsigned n = 0;
      for (auto Stmt : *Header) {
        DefUseHelper H(Stmt);
        n += H.countDefs(I.VD);
      }
      Out[Header] = n;          // (1)

      // while OUT changes
      while (Out != OldOut) {     // (3)
        OldOut = Out;
        // for each basic block other than entry
        for (auto Block : *L) {   // (4)

          // propagate OUT -> IN
          unsigned max = std::numeric_limits<unsigned>::min();
          for (NaturalLoopBlock::const_pred_iterator P = Block->pred_begin(),
                                                     E = Block->pred_end();
                                                     P != E; P++) {    // (5)
            const NaturalLoopBlock *Pred = *P;
            if (Out[Pred] > max) {
              max = Out[Pred];
            }
          }
          In[Block] = max;
          /* llvm::errs() << "In " << Block->getBlockID() << " " << In[Block] << "\n"; */

          if (Block == Header) continue;

          // compute OUT / f_B(x)
          unsigned n = 0;
          for (auto Stmt : *Block) {
            DefUseHelper H(Stmt);
            n += H.countDefs(I.VD);
          }
          Out[Block] = std::min(2U, In[Block] + n); // (6)
          /* llvm::errs() << "Out " << Block->getBlockID() << " " << Out[Block] << "\n"; */
        }
      }

      /* llvm::errs() << "Header " << In[Header] << "\n"; */
      if (In[Header] < 1)
          throw checkerror(Fail, Marker, "ASSIGNED_NotAllPaths");
      if (In[Header] > 1)
          throw checkerror(Fail, Marker, "ASSIGNED_Twice");
    }

  protected:
    const std::string Marker;
    const ASTContext *Context;

    virtual IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) = 0;
    virtual std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo I) const throw (checkerror) = 0;

  public:
    IncrementClassifier(const std::string Marker, const ASTContext *Context) :
      LoopClassifier(), Marker(Marker), Context(Context) {}
    virtual ~IncrementClassifier() {}

    std::set<IncrementLoopInfo> classify(const NaturalLoop *Loop, const IncrementClassifierConstraint Constr) const {
      // do we have the right # of exit arcs?
      unsigned PredSize = Loop->getExit().pred_size();
      assert(PredSize > 0);
      if (Constr.ECConstr != ANY_EXIT && PredSize != Constr.ECConstr) {
        LoopClassifier::classify(Loop, Fail, Constr.str(Marker), "WrongExitArcs");
        return std::set<IncrementLoopInfo>();
      }

      // are there any increments in this loop?
      LoopVariableFinder Finder(this);
      const std::set<IncrementInfo> LoopVarCandidates = Finder.findLoopVarCandidates(Loop);
      if (LoopVarCandidates.size() == 0) {
        LoopClassifier::classify(Loop, Fail, Constr.str(Marker), "NoLoopVarCandidate");
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
              checkBody(Loop, I);
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
            if (Cond) {
              PseudoConstantSet.clear();
              DefUseHelper CondDUH(Cond);
                for (auto VD : CondDUH.getDefsAndUses()) {
                  if (VD == I.VD) continue;
                  std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta.Var ? "D" : "X");
                  /* std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta ? "D" : VD->getNameAsString()); */
                  addPseudoConstantVar(name, VD);
                }
              checkPseudoConstantSet(Loop);
            }

            // save everything
            IncrementLoopInfo ILI = {I.VD, I.Statement, I.Delta, Bound};
            WellformedIncrements.insert(ILI);
            WellformedIncrement = true;
            Suffixes.insert(Suffix);

            // one is enough
            goto next_exit;
          } catch(checkerror &e) {
            Reasons.insert(e.what());
          }
        }
        if (Constr.EWConstr == ALL_WELLFORMED && !WellformedIncrement) {
          LoopClassifier::classify(Loop, Fail, Constr.str(Marker), "NotAllWellformed");
        }

next_exit:
        ; /* no op */
      }

      // We found well-formed increments
      if (WellformedIncrements.size()) {
        std::set<const VarDecl*> Counters;
        for (auto ILI : WellformedIncrements) {
          Counters.insert(ILI.VD);
        }
        LoopClassifier::classify(Loop, Success, Constr.str(Marker+"Counters"), Counters.size());

        std::stringstream Suffix;
        for (std::set<std::string>::const_iterator I = Suffixes.begin(),
                                                   E = Suffixes.end();
                                                   I != E; I++) {
          Suffix << *I;
          if (std::next(I) != E) Suffix << "-";
        }
        LoopClassifier::classify(Loop, Success, Constr.str(Marker), Suffix.str());
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

      LoopClassifier::classify(Loop, Reason.str());
      return std::set<IncrementLoopInfo>();
    }
};
