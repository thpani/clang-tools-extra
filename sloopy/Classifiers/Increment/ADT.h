#pragma once

class VarDeclIntPair {
  public:
    VarDeclIntPair() : Var(NULL), Int(llvm::APInt()) {}
    VarDeclIntPair(const VarDecl *Var) : Var(Var), Int(llvm::APInt()) {}
    VarDeclIntPair(const llvm::APInt Int) : Var(NULL), Int(Int) {}

    const VarDecl *Var;
    const llvm::APInt Int;

    bool operator==(const VarDeclIntPair &Other) const {
      if (Var || Other.Var) return Var == Other.Var;
      return Int.getBitWidth() > 1 && Other.Int.getBitWidth() > 1 && llvm::APInt::isSameValue(Int, Other.Int);
    }
    bool operator<(const VarDeclIntPair &Other) const {
      if (Var || Other.Var) {
        return Var < Other.Var;
      }
      return Int.getSExtValue() < Other.Int.getSExtValue();
    }
};

struct IncrementInfo {
  const VarDecl *VD;
  const Stmt *Statement;
  const VarDeclIntPair Delta;

  bool operator<(const IncrementInfo &Other) const {
    return VD < Other.VD || Delta < Other.Delta;
  }
};

struct PseudoConstantInfo {
  const std::string Name;
  const VarDecl *Var;
  const Stmt *IncrementOp;
};

struct IncrementLoopInfo {
  const VarDecl *VD;
  const Stmt *Statement;
  const VarDeclIntPair Delta;
  const VarDeclIntPair Bound;
  bool operator<(const IncrementLoopInfo &Other) const {
    return VD < Other.VD || Delta < Other.Delta || Bound < Other.Bound;
  }
};
