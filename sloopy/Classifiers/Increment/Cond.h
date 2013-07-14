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
      const VarDecl *BoundVar;

/*       if (!(ConditionOp->getLHS()->getType()->isIntegerType())) */
/*         throw checkerror(Unknown, Marker, "Cond_LHS_NoInteger"); */
/*       if (!(ConditionOp->getRHS()->getType()->isIntegerType())) */
/*         throw checkerror(Unknown, Marker, "Cond_RHS_NoInteger"); */

      // see if LHS/RHS is integer var
      bool LoopVarLHS;
      const VarDecl *LHS = getTypeVariable(ConditionOp->getLHS(), TypePredicate);
      const VarDecl *RHS = getTypeVariable(ConditionOp->getRHS(), TypePredicate);
      // see if LHS/RHS is (integer var)++
      if (LHS == NULL) {
        try {
          IncrementInfo I = ::getIncrementInfo(ConditionOp->getLHS(), Marker, Context, TypePredicate);
          LHS = I.VD;
        } catch(checkerror) {}
      }
      if (RHS == NULL) {
        try {
          IncrementInfo I = ::getIncrementInfo(ConditionOp->getRHS(), Marker, Context, TypePredicate);
          RHS = I.VD;
        } catch(checkerror) {}
      }

      /* determine which is loop var, which is bound */
      if (LHS != NULL && RHS != NULL) {
        // both lhs and rhs are vars
        // check which one is the loop var (i.e. equal to the initialization variable)
        if (Increment.VD == LHS) {
          LoopVarLHS = true;
        }
        else if (Increment.VD == RHS) {
          LoopVarLHS = false;
        }
        else {
          throw checkerror("Cond_LoopVar_NotIn_BinaryOperator");
        }
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
      else if (LHS != NULL && LHS == Increment.VD) {
        LoopVarLHS = true;
        Suffix = "ComplexBound";
        Bound = new VarDeclIntPair();
      }
      else if (RHS != NULL && RHS == Increment.VD) {
        LoopVarLHS = false;
        Suffix = "ComplexBound";
        Bound = new VarDeclIntPair();
      }
      else {
        throw checkerror("Cond_BinOp_TooComplex");
      }
      CondVar = LoopVarLHS ? LHS : RHS;
      BoundVar = LoopVarLHS ? RHS : LHS; // null if integer-literal, sizeof, ...
      if (BoundVar) Bound = new VarDeclIntPair(BoundVar);
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
