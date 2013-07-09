#pragma once

class VarDeclIntPair {
  public:
    VarDeclIntPair() : Var(NULL), Int(llvm::APInt()) {}
    VarDeclIntPair(const VarDecl *Var) : Var(Var), Int(llvm::APInt()) {}
    VarDeclIntPair(const llvm::APInt Int) : Var(NULL), Int(Int) {}

    const VarDecl *Var;
    llvm::APInt Int;

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

enum ExitsCountConstraint {
  ANY_EXIT,
  SINGLE_EXIT
};

enum ExitsWellformedConstraint {
  ANY_EXITCOND,
  SOME_WELLFORMED,
  ALL_WELLFORMED
};

enum IncrementsConstraint {
  SOME_PATH,
  EACH_PATH
};

struct IncrementClassifierConstraint {
  const ExitsCountConstraint ECConstr;
  const ExitsWellformedConstraint EWConstr;
  const IncrementsConstraint IConstr;

  std::string str(const std::string Marker=std::string()) const {
    std::stringstream Result;

    switch (IConstr) {
      case SOME_PATH:
        break;
      case EACH_PATH:
        Result << "Strong";
        break;
    }

    switch(ECConstr) {
      case SINGLE_EXIT:
        Result << "SingleExit";
        break;
      case ANY_EXIT:
        Result << "MultiExit";
        break;
    }

    switch(EWConstr) {
      case ANY_EXITCOND:
        Result << "NoCond";
        break;
      case SOME_WELLFORMED:
        break;
      case ALL_WELLFORMED:
        Result << "AllWellformed";
        break;
    }

    if (Marker.size()) {
      Result << ">" << Marker;
    }

    return Result.str();
  }

  std::string strIn() const {
    switch(ECConstr) {
      case SINGLE_EXIT:
        return "SingleExit";
      case ANY_EXIT:
        return "MultiExit";
    }
  }
};

static const IncrementClassifierConstraint SingleExit = { SINGLE_EXIT, SOME_WELLFORMED, SOME_PATH };
static const IncrementClassifierConstraint StrongSingleExit = { SINGLE_EXIT, SOME_WELLFORMED, EACH_PATH };
static const IncrementClassifierConstraint MultiExit = { ANY_EXIT, SOME_WELLFORMED, SOME_PATH };
static const IncrementClassifierConstraint StrongMultiExit = { ANY_EXIT, SOME_WELLFORMED, EACH_PATH };
static const IncrementClassifierConstraint MultiExitNoCond = { ANY_EXIT, ANY_EXITCOND, SOME_PATH };
