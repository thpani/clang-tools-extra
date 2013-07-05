#pragma once

/* type predicates */

typedef bool (*TypePredicate)(const VarDecl *);

static bool isIntegerType(const VarDecl *VD) {
  return VD->getType()->isIntegerType();
}

static bool isPointerType(const VarDecl *VD) {
  return VD->getType()->isPointerType();
}

/* integer constants */

static bool isIntegerConstant(const Expr *Expression, const ASTContext *Context) {
  llvm::APSInt Result;
  if (Expression->EvaluateAsInt(Result, const_cast<ASTContext&>(*Context))) {
    return true;
  }
  return false;
}

static llvm::APInt getIntegerConstant(const Expr *Expression, const ASTContext *Context) {
  llvm::APSInt Result;
  if (Expression->EvaluateAsInt(Result, const_cast<ASTContext&>(*Context))) {
    return Result;
  }
  llvm_unreachable("no integer constant");
}

/* variables */

static const VarDecl *getVariable(const Expr *Expression) {
  if (Expression==NULL) return NULL;
  const Expr *AdjustedExpr = Expression->IgnoreParenCasts();
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(AdjustedExpr)) {
    if (const VarDecl *Var = dyn_cast<VarDecl>(DRE->getDecl())) {
      return Var;
    }
  }
  return NULL;
}

static const VarDecl *getTypeVariable(const Expr *Expression, const TypePredicate TypePredicate) {
  const VarDecl *Var = getVariable(Expression);
  if (Var != NULL && TypePredicate(Var)) {
    return Var;
  }
  return NULL;
}

static const VarDecl *getIntegerVariable(const Expr *Expression) {
  return getTypeVariable(Expression, &isIntegerType);
}

/* arrays */

static const std::pair<const VarDecl*, const VarDecl*> getArrayVariables(const Expr *Expression) {
  if (const ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(Expression->IgnoreParenCasts())) {
    const VarDecl *Base = getVariable(ASE->getBase());
    if (const VarDecl *Idx = getIntegerVariable(ASE->getIdx())) {
      return std::pair<const VarDecl*, const VarDecl*>(Base, Idx);
    }
  }
  return std::pair<const VarDecl*, const VarDecl*>(NULL, NULL);
}
