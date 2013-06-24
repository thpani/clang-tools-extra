#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFIER_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFIER_H_

#include <sstream>
#include <algorithm>
#include <vector>
#include <stack>
#include "clang/AST/ASTContext.h"
#include "PseudoConstantAnalysis.h"

using namespace clang;
using namespace clang::tooling;

struct IncrementInfo {
  const VarDecl *VD;
  const Stmt *Statement;
  const VarDecl *Delta;
};

enum ClassificationKind {
  Fail,
  Success,
  Unknown
};

static std::string classificationKindToString(const ClassificationKind Kind) {
  return std::string(Kind == Fail ? "!" : (Kind == Unknown ? "?" : ""));
}

static std::string reasonToString(const ClassificationKind Kind, const std::string Marker, const std::string Suffix) {
  return classificationKindToString(Kind) + Marker + (Suffix.size() > 0 ? "-" : "") + Suffix;
}

class checkerror {
  public:
    checkerror(const ClassificationKind Kind, const std::string Marker, const std::string Suffix) :
      reason(reasonToString(Kind, Marker, Suffix)) {}

    const std::string what() const throw() {
      return reason;
    }
  private:
    const std::string reason;
};

typedef bool (*TypePredicate)(const VarDecl *);
static bool isIntegerType(const VarDecl *VD) {
  return VD->getType()->isIntegerType();
}
static bool isPointerType(const VarDecl *VD) {
  return VD->getType()->isPointerType();
}
static bool isIntegerConstant(const Expr *Expression, const ASTContext *Context) {
  llvm::APSInt Result;
  if (Expression->EvaluateAsInt(Result, const_cast<ASTContext&>(*Context))) {
    return true;
  }
  return false;
}

/* static llvm::APInt getIntegerConstant(const Expr *Expression, const ASTContext *Context) { */
/*   llvm::APSInt Result; */
/*   if (Expression->EvaluateAsInt(Result, const_cast<ASTContext&>(*Context))) { */
/*     return Result; */
/*   } */
/*   llvm_unreachable("no integer constant"); */
/* } */

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

static const std::pair<const VarDecl*, const VarDecl*> getArrayVariables(const Expr *Expression) {
  if (const ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(Expression->IgnoreParenCasts())) {
    const VarDecl *Base = getVariable(ASE->getBase());
    if (const VarDecl *Idx = getIntegerVariable(ASE->getIdx())) {
      return std::pair<const VarDecl*, const VarDecl*>(Base, Idx);
    }
  }
  return std::pair<const VarDecl*, const VarDecl*>(NULL, NULL);
}

static bool isVariable(const Expr *Expression) {
  return getVariable(Expression) != NULL;
}
static bool isIntegerVariable(const Expr *Expression) {
  return getIntegerVariable(Expression) != NULL;
}

static const ValueDecl *getIntegerField(const Expr *Expression) {
  if (Expression==NULL) return NULL;
  if (const ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(Expression->IgnoreParenCasts())) {
    const Expr *Base = ASE->getBase()->IgnoreParenCasts();
    if (const VarDecl *VD = getVariable(Base)) {
      if (VD->getType()->isArrayType()) {
          if (VD->getType()->castAsArrayTypeUnsafe()->getElementType()->isIntegerType()) {
            return VD;
          }
      }
      else {
        // TODO
        assert(VD->getType()->isPointerType());
      }
    }
    if (const MemberExpr *ME = dyn_cast<MemberExpr>(Base->IgnoreParenCasts())) {
      if (const FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
        if (FD->getType()->isArrayType()) {
          if (FD->getType()->castAsArrayTypeUnsafe()->getElementType()->isIntegerType()) {
            return FD;
          }
        }
        else {
          // TODO
          assert(FD->getType()->isPointerType());
        }
      }
    }
  }
  if (const VarDecl *VD = getIntegerVariable(Expression)) {
    return VD;
  }
  if (const MemberExpr *ME = dyn_cast<MemberExpr>(Expression->IgnoreParenCasts())) {
    // TODO
    if (const FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
      if (FD->getType()->isIntegerType()) {
        return FD;
      }
    }
  }
  return NULL;
}

// Classifier := Stmt [!?@] {ADA,Array,...} - Suffix
// Suffix := Str | Str _ Suffix
class LoopClassifier {
  public:
    void classify(const NaturalLoop* Loop) const {
      Classifications[Loop->getUnsliced()].insert(Loop->getLoopStmtMarker());
    }
    void classify(const NaturalLoop* Loop, const std::string Reason) const {
      std::string S = Loop->getLoopStmtMarker() + "@" + Reason;
      Classifications[Loop->getUnsliced()].insert(S);
    }
    void classify(const NaturalLoop* Loop, const ClassificationKind Kind, const std::string Marker, const std::string Suffix="") const {
      std::string S = Loop->getLoopStmtMarker() + "@" + reasonToString(Kind, Marker, Suffix);
      Classifications[Loop->getUnsliced()].insert(S);
    }
};

class AnyLoopCounter {
  public:
    void classify(const NaturalLoop* Loop) const {
      Classifications[Loop->getUnsliced()].insert("ANY");
    }
};

class SimpleLoopCounter : public LoopClassifier {
  public:
    void classify(const NaturalLoop* Loop) const {
      unsigned PredSize = Loop->getExit().pred_size();
      if (PredSize == 1) {
        LoopClassifier::classify(Loop, "SIMPLE");
      }
    }
};

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_CLASSIFIER_H_
