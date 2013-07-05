#pragma once

#include "LoopClassifier.h"
#include "Increment/ADT.h"
#include "Increment/Helpers.h"
#include "Increment/Inc.h"
#include "Increment/Cond.h"

class IncrementClassifier : public LoopClassifier {
  private:
    mutable std::vector<const PseudoConstantInfo> PseudoConstantSet;

    class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
      public:
        LoopVariableFinder(const IncrementClassifier *Outer) : Outer(Outer) {}
        const std::set<IncrementInfo> findLoopVarCandidates(const NaturalLoop *Loop) {
          for (auto Block : *Loop) {
            TraverseStmt(const_cast<Expr*>(Block->getTerminatorCondition()));
            for (auto S : *Block) {
              TraverseStmt(const_cast<Stmt*>(S));
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

  protected:
    const std::string Marker;

    virtual IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) = 0;
    virtual std::pair<std::string, VarDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo I) const throw (checkerror) = 0;
    virtual bool checkPreds(const NaturalLoop *Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      assert(PredSize > 0);
      if (PredSize > 1) {
        return false;
      }
      return true;
    }

  public:
    IncrementClassifier(const std::string Marker) : LoopClassifier(), Marker(Marker) {}
    virtual ~IncrementClassifier() {}

    std::set<IncrementLoopInfo> classify(const NaturalLoop *Loop) const {
      LoopVariableFinder Finder(this);
      const std::set<IncrementInfo> LoopVarCandidates = Finder.findLoopVarCandidates(Loop);

      if (LoopVarCandidates.size() == 0) {
        LoopClassifier::classify(Loop, Fail, Marker, "NoLoopVarCandidate");
        return std::set<IncrementLoopInfo>();
      }

      if (!checkPreds(Loop)) {
        LoopClassifier::classify(Loop, Fail, Marker, "TooManyExitArcs");
        return std::set<IncrementLoopInfo>();
      }

      std::set<std::string> reasons;
      std::set<IncrementLoopInfo> successes;
      std::vector<std::string> suffixes;
      for (NaturalLoopBlock::const_pred_iterator PI = Loop->getExit().pred_begin(),
                                                 PE = Loop->getExit().pred_end();
                                                 PI != PE; PI++) {
        for (const IncrementInfo I : LoopVarCandidates) {
          try {
            const Expr *Cond = (*PI)->getTerminatorCondition();
            auto result = checkCond(Cond, I);
            std::string suffix = result.first;
            VarDeclIntPair Bound = result.second;

            PseudoConstantSet.clear();
            DefUseHelper CondDUH(Cond);
            if (Cond) {
            for (auto VD : CondDUH.getDefsAndUses()) {
              if (VD == I.VD) continue;
              std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta.Var ? "D" : "X");
              /* std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta ? "D" : VD->getNameAsString()); */
              addPseudoConstantVar(name, VD);
            }
            }
            checkPseudoConstantSet(Loop);

            IncrementLoopInfo ILI = {I.VD, I.Statement, I.Delta, Bound};
            successes.insert(ILI);
            suffixes.push_back(suffix);
          } catch(checkerror &e) {
            reasons.insert(e.what());
          }
        }
      }
      if (successes.size()) {
        std::set<const VarDecl*> Counters;
        for (auto ILI : successes) {
          Counters.insert(ILI.VD);
        }
        LoopClassifier::classify(Loop, Success, Marker+"Counters", Counters.size());

        // TODO concat unique suffixes
        LoopClassifier::classify(Loop, Success, Marker, suffixes[0]);
        return successes;
      }

      std::stringstream ss;
      for(auto reason : reasons) {
        ss << reason << "-";
      }
      std::string suffix = ss.str();
      suffix = suffix.substr(0, suffix.size()-1);
      assert(suffix.size()!=0);

      LoopClassifier::classify(Loop, suffix);
      return std::set<IncrementLoopInfo>();
    }
};
