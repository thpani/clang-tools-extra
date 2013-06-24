class DataIterClassifier : public IncrementClassifier {
  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      if (Expr == NULL) throw checkerror(Fail, Marker, "Inc_None");
      const class Expr *Expression = Expr->IgnoreParenCasts();
      const BinaryOperator *BO;
      if (!(BO = dyn_cast<BinaryOperator>(Expression))) {
        throw checkerror(Fail, Marker, "Inc_NotBinary");
      }
      const VarDecl *IncVar;
      if (!(IncVar = getVariable(BO->getLHS()))) {
        throw checkerror(Fail, Marker, "Inc_LHSNoVar");
      }
      if (!(*IncVar->getType()).isPointerType()) {
        throw checkerror(Fail, Marker, "Inc_LHSNoPtr");
      }
      const MemberExpr *RHS;
      if (!(RHS = dyn_cast<MemberExpr>(BO->getRHS()->IgnoreParenCasts()))) {
        throw checkerror(Fail, Marker, "Inc_RHSNoMemberExpr");
      }
      const VarDecl *Base;
      if (!(Base = getVariable(RHS->getBase()))) {
        throw checkerror(Fail, Marker, "Inc_RHSBaseNoVar");
      }
      if (Base != IncVar) {
        throw checkerror(Fail, Marker, "Inc_RHSBaseNeqInc");
      }
      return { IncVar, BO, NULL };
    }

    std::pair<std::string, const ValueDecl*> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      // TODO
      return std::pair<std::string, const ValueDecl*>(std::string(), NULL);
    }

  public:
    DataIterClassifier() : IncrementClassifier("DataIter") {}
};
