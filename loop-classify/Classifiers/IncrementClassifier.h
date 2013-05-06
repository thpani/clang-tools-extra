#ifndef _INCREMENT_CLASSIFIER_H_
#define _INCREMENT_CLASSIFIER_H_

typedef struct {
  const std::string Name;
  const ValueDecl *Var;
  const Stmt *IncrementOp;
} PseudoConstantInfo;

class IncrementClassifier : public LoopClassifier {
  public:
    IncrementClassifier(const std::string &Marker) : LoopClassifier(Marker) {}

    virtual void run(const MatchFinder::MatchResult &Result) {
      const Stmt *LS = Result.Nodes.getNodeAs<clang::Stmt>(LoopName);
      this->Context = Result.Context;
      PseudoConstantSet.clear();

      LoopVariableFinder Finder(this);
      auto LoopVarCandidates = Finder.findLoopVarCandidates(getLoopBlocks(LS));

      std::vector<std::string> reasons;
      for (auto I : LoopVarCandidates) {
        try {
          std::string suffix = check(LS, I);
          classify(Result, getStmtMarker(LS)+Marker+suffix);
          return;
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
      if (suffix.size()==0) {
        suffix = "!"+Marker+"-NoLoopVarCandidate";
      }

      classify(Result, getStmtMarker(LS)+suffix);
      return;
    }

  protected:
    virtual const IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) = 0;
    virtual const std::string check(const Stmt *LS, const IncrementInfo I) const throw (checkerror) = 0;

    void addPseudoConstantVar(const std::string Name, const ValueDecl *Var, const Stmt *IncrementOp=NULL) const {
      const PseudoConstantInfo I = { Name, Var, IncrementOp };
      PseudoConstantSet.push_back(I);
    }
    void checkPseudoConstantSet(const Stmt *LS) const throw (checkerror) {
      for (auto Block : getLoopBlocks(LS)) {
        cloopy::PseudoConstantAnalysis A(Block);
        for (auto IncrementElement : PseudoConstantSet) {
          if (!A.isPseudoConstant(IncrementElement.Var, IncrementElement.IncrementOp)) {
            throw checkerror(IncrementElement.Name+"-ASSIGNED");
          }
        }
      }
    }

  protected:
    const ASTContext *Context;

  private:
    mutable std::vector<const PseudoConstantInfo> PseudoConstantSet;

    class LoopVariableFinder : public RecursiveASTVisitor<LoopVariableFinder> {
      public:
        LoopVariableFinder(const IncrementClassifier *Outer) : Outer(Outer) {}
        const std::vector<IncrementInfo> findLoopVarCandidates(const std::vector<const Stmt*> Search) {
          for (auto S : Search) {
            TraverseStmt(const_cast<Stmt *>(S));
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
};
#endif // _INCREMENT_CLASSIFIER_H_
