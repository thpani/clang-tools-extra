class DataIterClassifier : public IncrementClassifier {
  public:
    DataIterClassifier() : IncrementClassifier("DataIter") {}

  protected:
    const IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      if (Expr == NULL) throw checkerror("!DataIter-IncNone");
      const class Expr *Expression = Expr->IgnoreParenImpCasts();
      const BinaryOperator *BO;
      if (!(BO = dyn_cast<BinaryOperator>(Expression))) {
        throw checkerror("!DataIter-Inc_NotBinary");
      }
      const VarDecl *IncVar;
      if (!(IncVar = getVariable(BO->getLHS()))) {
        throw checkerror("!DataIter-Inc_LHSNoVar");
      }
      if (!(*IncVar->getType()).isPointerType()) {
        throw checkerror("!DataIter-Inc_LHSNoPtr");
      }
      const MemberExpr *RHS;
      if (!(RHS = dyn_cast<MemberExpr>(BO->getRHS()->IgnoreParenImpCasts()))) {
        throw checkerror("!DataIter-Inc_RHSNoMemberExpr");
      }
      const VarDecl *Base;
      if (!(Base = getVariable(RHS->getBase()))) {
        throw checkerror("!DataIter-Inc_RHSBaseNoVar");
      }
      if (Base != IncVar) {
        throw checkerror("!DataIter-Inc_RHSBaseNeqInc");
      }
      return { IncVar, BO, NULL };
    }

    const std::string check (const Stmt *LS, const IncrementInfo I) const throw (checkerror) {
      addPseudoConstantVar("i", I.VD, I.Statement);
      try {
        checkPseudoConstantSet(LS);
      } catch (checkerror &e) {
        throw checkerror("!"+Marker+"_"+e.what());
      }

      return "";
    }
};
