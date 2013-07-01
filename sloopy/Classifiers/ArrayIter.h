#include "IncrementClassifier.h"

class ArrayIterClassifier : public IncrementClassifier {
  private:
    const ASTContext *Context;

  protected:
    IncrementInfo getIncrementInfo(const Expr *Expr) const throw (checkerror) {
      return ::getIncrementInfo(Expr, Marker, Context, &isPointerType);
    }

    std::pair<std::string, ValueDeclIntPair> checkCond(const Expr *Cond, const IncrementInfo Increment) const throw (checkerror) {
      // TODO
      return std::pair<std::string, ValueDeclIntPair>(std::string(), { NULL, llvm::APInt() });
    }

  public:
    ArrayIterClassifier(const ASTContext* Context) : IncrementClassifier("PArrayIter"), Context(Context) {}
};
