#include "IncrementClassifier.h"

class ArrayIterClassifier : public IncrementClassifier {
  public:
    ArrayIterClassifier() : IncrementClassifier("ArrayIter") {}

  protected:
    const IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      if (Expr == NULL) throw checkerror("!ArrayIter-IncNone");
      const class Expr *Expression = Expr->IgnoreParenImpCasts();
      const UnaryOperator *UO;
      if (!(UO = dyn_cast<UnaryOperator>(Expression))) {
        throw checkerror("!ArrayIter-Inc_NotUnary");
      }
      if (!UO->isIncrementDecrementOp()) {
        throw checkerror("!ArrayIter-Inc_NotIncDecOp");
      }
      const VarDecl *IncVar;
      if (!(IncVar = getVariable(UO->getSubExpr()))) {
        throw checkerror("!ArrayIter-Inc_SubExprNoVar");
      }
      if (!(*IncVar->getType()).isPointerType()) {
        throw checkerror("!ArrayIter-Inc_SubExprNoPtr");
      }
      return { IncVar, UO, NULL };
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
