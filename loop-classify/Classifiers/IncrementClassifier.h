#ifndef _INCREMENT_CLASSIFIER_H_
#define _INCREMENT_CLASSIFIER_H_

#include "LoopClassifier.h"

typedef struct {
  const std::string Name;
  const ValueDecl *Var;
  const Stmt *IncrementOp;
} PseudoConstantInfo;

static IncrementInfo getIncrementInfo(const Expr *Expr, const std::string Marker, const ASTContext *Context, const TypePredicate TypePredicate) {
  if (Expr == NULL) throw checkerror(Fail, Marker, "Inc_None");
  const class Expr *Expression = Expr->IgnoreParenCasts();
  if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Expression)) {
    // i{++,--}
    if (UOP->isIncrementDecrementOp()) {
      if (const VarDecl *VD = getVariable(UOP->getSubExpr())) {
        if (TypePredicate(VD)) {
          return { VD, UOP, NULL };
        }
      }
    }
  }
  else if (const BinaryOperator *BOP = dyn_cast<BinaryOperator>(Expression)) {
    // i = i {+-} <int>
    // i = <int> {+-} i
    if (BOP->getOpcode() == BO_Assign) {
      if (const VarDecl *VD = getVariable(BOP->getLHS())) {
        if (TypePredicate(VD)) {
          if (const BinaryOperator *RHS = dyn_cast<BinaryOperator>(BOP->getRHS()->IgnoreParenCasts())) {
            if (RHS->isAdditiveOp()) {
              const VarDecl *RRHS = getVariable(RHS->getRHS());
              const VarDecl *RLHS = getVariable(RHS->getLHS());
              if ((RRHS == VD && isIntegerConstant(RHS->getLHS(), Context)) ||
                  (RLHS == VD && isIntegerConstant(RHS->getRHS(), Context)) ||
                  (RRHS == VD && isIntegerVariable(RHS->getLHS())) ||
                  (RLHS == VD && isIntegerVariable(RHS->getRHS()))) {
                return { VD, BOP, NULL };
              }
            }
          }
        }
      }
    }
    // i {+-}= <int>
    else if (BOP->getOpcode() == BO_AddAssign ||
              BOP->getOpcode() == BO_SubAssign ) {
      if (const VarDecl *VD = getVariable(BOP->getLHS())) {
        if (TypePredicate(VD)) {
          if (isIntegerConstant(BOP->getRHS(), Context)) {
            return { VD, BOP, NULL };
          }
          else if (const VarDecl *Delta = getIntegerVariable(BOP->getRHS())) {
            return { VD, BOP, Delta };
          }
        }
      }
    }
  }
  throw checkerror(Fail, Marker, "Inc_NotValid");
}

class IncrementClassifier : public LoopClassifier {
  private:
    mutable std::vector<const PseudoConstantInfo> PseudoConstantSet;

    class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
      public:
        LoopVariableFinder(const IncrementClassifier *Outer) : Outer(Outer) {}
        const std::vector<IncrementInfo> findLoopVarCandidates(const NaturalLoop *Loop) {
          for (auto Block : *Loop) {
            TraverseStmt(const_cast<Expr*>(Block->getCond()));
            for (auto S : *Block) {
              TraverseStmt(const_cast<Stmt*>(S));
            }
          }
          return LoopVarCandidates;
        }

        bool VisitExpr(Expr *Expr) {
          try {
            const IncrementInfo I = Outer->getIncrementInfo(Expr);
            LoopVarCandidates.push_back(I);
          } catch(checkerror) {}
          return true;
        }

      private:
        std::vector<IncrementInfo> LoopVarCandidates;
        const IncrementClassifier *Outer;
    };

    void addPseudoConstantVar(const std::string Name, const ValueDecl *Var, const Stmt *IncrementOp=NULL) const {
      const PseudoConstantInfo I = { Name, Var, IncrementOp };
      PseudoConstantSet.push_back(I);
    }

    void checkPseudoConstantSet(const NaturalLoop *L) const throw (checkerror) {
      for (auto Block : *L) {
        for (auto Stmt : *Block) {
          cloopy::PseudoConstantAnalysis A(Stmt);
          for (auto IncrementElement : PseudoConstantSet) {
            if (!A.isPseudoConstant(IncrementElement.Var, IncrementElement.IncrementOp)) {
              throw checkerror(Fail, Marker, IncrementElement.Name+"_ASSIGNED");
            }
          }
        }
      }
    }

  protected:
    const std::string Marker;

    virtual IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) = 0;
    virtual std::pair<std::string, const ValueDecl*> checkCond(const NaturalLoop *L, const IncrementInfo I) const throw (checkerror) = 0;

  public:
    IncrementClassifier(const std::string Marker) : LoopClassifier(), Marker(Marker) {}
    virtual ~IncrementClassifier() {}

    bool classify(const NaturalLoopPair P) {
      const NaturalLoop *Loop = P.first;

      LoopVariableFinder Finder(this);
      auto LoopVarCandidates = Finder.findLoopVarCandidates(Loop);

      if (LoopVarCandidates.size() == 0) {
        LoopClassifier::classify(Loop, Fail, Marker, "NoLoopVarCandidate");
        return false;
      }

      unsigned PredSize = Loop->getExit().pred_size();
      if (PredSize > 1) {
        LoopClassifier::classify(Loop, Fail, Marker, "TooManyExitArcs");
        return false;
      }
      assert(PredSize > 0);

      std::vector<std::string> reasons;
      for (auto I : LoopVarCandidates) {
        try {
          auto result = checkCond(Loop, I);
          std::string suffix = result.first;
          const ValueDecl *BoundVar = result.second;

          PseudoConstantSet.clear();
          for (auto VD : Loop->getControlVars()) {
            if (VD == I.VD) continue;
            if (VD == BoundVar && BoundVar == I.VD) continue;
            std::string name = I.VD == BoundVar ? "N" : (I.VD == I.Delta ? "D" : "X");
            /* std::string name = I.VD == BoundVar ? "N" : (I.VD == I.Delta ? "D" : VD->getNameAsString()); */
            addPseudoConstantVar(name, VD);
          }
          checkPseudoConstantSet(Loop);

          LoopClassifier::classify(Loop, Success, Marker, suffix);
          return true;
        } catch(checkerror &e) {
          reasons.push_back(e.what());
        }
      }
      std::sort(reasons.begin(), reasons.end());
      std::vector<std::string>::iterator it = std::unique (reasons.begin(), reasons.end());
      reasons.resize( std::distance(reasons.begin(),it) );

      std::stringstream ss;
      for(auto reason : reasons) {
        ss << reason << "-";
      }
      std::string suffix = ss.str();
      suffix = suffix.substr(0, suffix.size()-1);
      assert(suffix.size()!=0);

      LoopClassifier::classify(Loop, suffix);
      return false;
    }
};
#endif // _INCREMENT_CLASSIFIER_H_
