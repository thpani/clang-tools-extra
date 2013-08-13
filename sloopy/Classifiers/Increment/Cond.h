#pragma once

static std::pair<std::string, VarDeclIntPair> checkIncrementCond(
    const Expr *Cond,
    const IncrementInfo Increment,
    const TypePredicate TypePredicate,
    const ASTContext *Context,
    const std::string Marker)
  throw (checkerror) {

  std::string Suffix;

  /* COND */
  if (Cond == NULL) {
    throw checkerror("Cond_None");
  }
  /* check the condition is a binary operator
    * where either LHS or RHS are integer literals
    * or one is an integer literal
    */
  const Expr *CondInner = Cond->IgnoreParenCasts();
  const VarDecl *CondVar = NULL;
  const VarDeclIntPair *Bound;
  if (const VarDecl *VD = getVariable(CondInner)) {
    if (!TypePredicate(VD)) {
      throw checkerror("Cond_Var_WrongType");
    }
    CondVar = VD;
    Bound = new VarDeclIntPair(VD);
  }
  else if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(CondInner)) {
    if (UO->isIncrementDecrementOp() || UO->getOpcode() == UO_LNot) {
      if (const VarDecl *VD = getTypeVariable(UO->getSubExpr(), TypePredicate)) {
        CondVar = VD;
        Bound = new VarDeclIntPair(VD);
      }
      else {
        throw checkerror("Cond_Unary_NoTypeVar");
      }
    }
    else if (UO->getOpcode() == UO_Deref) {
      return checkIncrementCond(UO->getSubExpr(), Increment, TypePredicate, Context, Marker);
    }
    else {
      throw checkerror("Cond_Unary_NotIncDec");
    }
  }
  else if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(CondInner)) {
    BinaryOperatorKind Opc = ConditionOp->getOpcode();
    if ((Opc >= BO_Mul && Opc <= BO_Shr) || ConditionOp->isComparisonOp()) {

      // see if LHS/RHS is integer var
      bool LoopVarLHS;
      const VarDecl *LHS = getTypeVariable(ConditionOp->getLHS(), TypePredicate);
      const VarDecl *RHS = getTypeVariable(ConditionOp->getRHS(), TypePredicate);
      // see if LHS/RHS is (integer var)++
      if (LHS == NULL) {
        auto I = ::getIncrementInfo(ConditionOp->getLHS(), Marker, Context, TypePredicate);
        if (I.which() == 1) {
          LHS = boost::get<IncrementInfo>(I).VD;
        }
      }
      if (RHS == NULL) {
        auto I = ::getIncrementInfo(ConditionOp->getRHS(), Marker, Context, TypePredicate);
        if (I.which() == 1) {
          RHS = boost::get<IncrementInfo>(I).VD;
        }
      }

      /* determine which is loop var, which is bound */
      if (LHS == Increment.VD && getVariable(ConditionOp->getRHS())) {
        // LHS is loop var, RHS is var bound
        LoopVarLHS = true;
        Bound = new VarDeclIntPair(RHS);
      } else if (RHS == Increment.VD && getVariable(ConditionOp->getLHS())) {
        // RHS is loop var, LHS is var bound
        LoopVarLHS = false;
        Bound = new VarDeclIntPair(LHS);
      }
      else if (LHS != NULL && isIntegerConstant(ConditionOp->getRHS(), Context)) {
        // LHS is loop var, RHS is integer bound
        LoopVarLHS = true;
        Bound = new VarDeclIntPair(getIntegerConstant(ConditionOp->getRHS(), Context));
      }
      else if (RHS != NULL && isIntegerConstant(ConditionOp->getLHS(), Context)) {
        // RHS is loop var, LHS is integer bound
        LoopVarLHS = false;
        Bound = new VarDeclIntPair(getIntegerConstant(ConditionOp->getLHS(), Context));
      }
      else if (LHS != NULL && isa<UnaryExprOrTypeTraitExpr>(ConditionOp->getRHS()->IgnoreParenCasts())) {
        // LHS is loop var, RHS is sizeof/alignof() bound
        LoopVarLHS = true;
        Bound = new VarDeclIntPair();
      }
      else if (RHS != NULL && isa<UnaryExprOrTypeTraitExpr>(ConditionOp->getLHS()->IgnoreParenCasts())) {
        // RHS is loop var, LHS is sizeof/alignof() bound
        LoopVarLHS = false;
        Bound = new VarDeclIntPair();
      }
      else if (LHS != NULL) {
        LoopVarLHS = true;
        Suffix = "ComplexBound";
        Bound = new VarDeclIntPair();
      }
      else if (RHS != NULL) {
        LoopVarLHS = false;
        Suffix = "ComplexBound";
        Bound = new VarDeclIntPair();
      }
      else {
        throw checkerror("Cond_BinOp_TooComplex");
      }
      CondVar = LoopVarLHS ? LHS : RHS;
    }
    else {
      throw checkerror("Cond_BinOp_OpNotSupported");
    }
  }
  else {
    throw checkerror("Cond_OpNotSupported");
  }
  assert(CondVar);
  assert(Bound);

  /* VARS */

  if (CondVar != Increment.VD) {
    throw checkerror("CondVar_NEQ_IncVar");
  }
  if (!TypePredicate(CondVar)) {
    throw checkerror("CondVar_WrongType");
  }
  if (Bound->Var && !TypePredicate(Bound->Var)) {
    throw checkerror("BoundVar_WrongType");
  }

  return std::pair<std::string, VarDeclIntPair>(Suffix, *std::unique_ptr<const VarDeclIntPair>(Bound).get());
}
