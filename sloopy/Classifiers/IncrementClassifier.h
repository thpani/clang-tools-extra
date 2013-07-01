#ifndef _INCREMENT_CLASSIFIER_H_
#define _INCREMENT_CLASSIFIER_H_

#include "LoopClassifier.h"


struct ValueDeclIntPair {
  const ValueDecl *Var;
  llvm::APInt Int;
  bool operator==(const ValueDeclIntPair &Other) const {
    if (Var || Other.Var) return Var == Other.Var;
    return llvm::APInt::isSameValue(Int, Other.Int);
  }
  bool operator<(const ValueDeclIntPair &Other) const {
    if (Var || Other.Var) {
      return Var < Other.Var;
    }

    return Int.getSExtValue() < Other.Int.getSExtValue();
  }
};
struct IncrementInfo {
  const VarDecl *VD;
  const Stmt *Statement;
  const ValueDeclIntPair Delta;

  bool operator<(const IncrementInfo &Other) const {
    return VD < Other.VD || Delta < Other.Delta;
  }
};
typedef struct {
  const std::string Name;
  const ValueDecl *Var;
  const Stmt *IncrementOp;
} PseudoConstantInfo;

struct IncrementLoopInfo {
  const VarDecl *VD;
  const Stmt *Statement;
  const ValueDeclIntPair Delta;
  const ValueDeclIntPair Bound;
};

static IncrementInfo getIncrementInfo(const Expr *Expr, const std::string Marker, const ASTContext *Context, const TypePredicate TypePredicate) {
  if (Expr == NULL) throw checkerror(Fail, Marker, "Inc_None");
  const class Expr *Expression = Expr->IgnoreParenCasts();
  if (const UnaryOperator *UOP = dyn_cast<UnaryOperator>(Expression)) {
    // i{++,--}
    if (UOP->isIncrementDecrementOp()) {
      if (const VarDecl *VD = getVariable(UOP->getSubExpr())) {
        if (TypePredicate(VD)) {
          return { VD, UOP, { NULL, llvm::APInt(2, UOP->isIncrementOp() ? 1 : -1, true) }};
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
              if (RRHS == VD && isIntegerConstant(RHS->getLHS(), Context)) {
                return { VD, BOP, { NULL, getIntegerConstant(RHS->getLHS(), Context) }};
              }
              if (RLHS == VD && isIntegerConstant(RHS->getRHS(), Context)) {
                return { VD, BOP, { NULL, getIntegerConstant(RHS->getRHS(), Context) }};
              }
              if (RRHS == VD && isIntegerVariable(RHS->getLHS())) {
                return { VD, BOP, { getIntegerVariable(RHS->getLHS()), llvm::APInt() }};
              }
              if (RLHS == VD && isIntegerVariable(RHS->getRHS())) {
                return { VD, BOP, { getIntegerVariable(RHS->getRHS()), llvm::APInt() }};
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
            return { VD, BOP, { NULL, getIntegerConstant(BOP->getRHS(), Context) }};
          }
          else if (const VarDecl *Delta = getIntegerVariable(BOP->getRHS())) {
            return { VD, BOP, { Delta, llvm::APInt() }};
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
    virtual std::pair<std::string, ValueDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo I) const throw (checkerror) = 0;
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

    std::vector<IncrementLoopInfo> classify(const NaturalLoop *Loop) const {
      LoopVariableFinder Finder(this);
      auto LoopVarCandidates = Finder.findLoopVarCandidates(Loop);

      if (LoopVarCandidates.size() == 0) {
        LoopClassifier::classify(Loop, Fail, Marker, "NoLoopVarCandidate");
        return std::vector<IncrementLoopInfo>();
      }

      if (!checkPreds(Loop)) {
        LoopClassifier::classify(Loop, Fail, Marker, "TooManyExitArcs");
        return std::vector<IncrementLoopInfo>();
      }

      std::vector<std::string> reasons;
      std::vector<IncrementLoopInfo> successes;
      std::vector<std::string> suffixes;
      for (NaturalLoopBlock::const_pred_iterator PI = Loop->getExit().pred_begin(),
                                                 PE = Loop->getExit().pred_end();
                                                 PI != PE; PI++) {
        for (const IncrementInfo I : LoopVarCandidates) {
          try {
            const Expr *Cond = (*PI)->getTerminatorCondition();
            auto result = checkCond(Cond, I);
            std::string suffix = result.first;
            ValueDeclIntPair Bound = result.second;

            PseudoConstantSet.clear();
            DefUseHelper CondDUH(Cond);
            for (auto VD : CondDUH.getDefsAndUses()) {
              if (VD == I.VD) continue;
              if (VD == Bound.Var && Bound.Var == I.VD) continue;
              std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta.Var ? "D" : "X");
              /* std::string name = I.VD == Bound.Var ? "N" : (I.VD == I.Delta ? "D" : VD->getNameAsString()); */
              addPseudoConstantVar(name, VD);
            }
            checkPseudoConstantSet(Loop);

            IncrementLoopInfo ILI = {I.VD, I.Statement, I.Delta, Bound};
            successes.push_back(ILI);
            suffixes.push_back(suffix);
          } catch(checkerror &e) {
            reasons.push_back(e.what());
          }
        }
      }
      if (successes.size()) {
        // TODO concat unique suffixes
        LoopClassifier::classify(Loop, Success, Marker, suffixes[0]);
        return successes;
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
      return std::vector<IncrementLoopInfo>();
    }
};
#endif // _INCREMENT_CLASSIFIER_H_
