#pragma once

/* variable, int, or unknown */
class VarDeclIntPair {
  public:
    VarDeclIntPair() : Var(NULL), Int(llvm::APInt()) {}
    VarDeclIntPair(const VarDecl *Var) : Var(Var), Int(llvm::APInt()) {}
    VarDeclIntPair(const llvm::APInt Int) : Var(NULL), Int(Int) {}

    const VarDecl *Var;
    llvm::APInt Int;

    bool isInt() const {
      return Int.getBitWidth() > 1;
    }
    bool operator==(const VarDeclIntPair &Other) const {
      if (Var || Other.Var) return Var == Other.Var;
      return isInt() && Other.isInt() && llvm::APInt::isSameValue(Int, Other.Int);
    }
    bool operator<(const VarDeclIntPair &Other) const {
      return Var < Other.Var or
            (Var == Other.Var and Int.getSExtValue() < Other.Int.getSExtValue());
    }
};

struct IncrementInfo {
  const VarDecl *VD;
  const Expr *Statement;
  VarDeclIntPair Delta;

  bool operator<(const IncrementInfo &Other) const {
    return std::tie(VD, Delta) < std::tie(Other.VD, Other.Delta);
  }
};

struct PseudoConstantInfo {
  const std::string Name;
  const VarDecl *Var;
};

struct IncrementLoopInfo {
  const VarDecl *VD;
  const Expr *Statement;
  const VarDeclIntPair Delta;
  const VarDeclIntPair Bound;
  bool operator<(const IncrementLoopInfo &Other) const {
    return std::tie(VD, Delta, Bound) < std::tie(Other.VD, Other.Delta, Other.Bound);
  }
};

enum ExitsCountConstraint {
  ANY_EXIT,
  SINGLE_EXIT
};

enum ExitsWellformedConstraint {
  ANY_EXITCOND,
  SOME_WELLFORMED
};

enum IncrementsConstraint {
  SOME_PATH,
  EACH_PATH
};

enum CondFormConstraint {
  IMPLIES,
  ASSUME_IMPLIES
};

enum ControlFlowConstraint {
  SINGLETON,
  SOME_EACH,
  SOME_SOME
};

enum InvariantConstraint {
  MAYBE_INVARIANT,
  INVARIANT
};

struct SimpleLoopConstraint {
  const ExitsCountConstraint ExitCountConstr;
  const ControlFlowConstraint ControlFlowConstr;
  const CondFormConstraint FormConstr;
  const InvariantConstraint InvariantConstr;

  bool isSyntTerm() const {
    return (ExitCountConstr == ANY_EXIT and
            ControlFlowConstr == SINGLETON and
            FormConstr == IMPLIES and
            InvariantConstr == INVARIANT);
  }

  std::string str() const {
    if (isSyntTerm()) {
      return "Proved";
    }

    std::stringstream Result;

    switch (ExitCountConstr) {
      case SINGLE_EXIT:
        Result << "SingleExit";
        break;
      case ANY_EXIT:
        Result << "AnyExit";
        break;
    }

    switch (ControlFlowConstr) {
      case SINGLETON:
        Result << "ProvedCf";
        break;
      case SOME_EACH:
        Result << "StrongCf";
        break;
      case SOME_SOME:
        Result << "WeakCf";
        break;
    }

    switch (InvariantConstr) {
      case MAYBE_INVARIANT:
        break;
      case INVARIANT:
        Result << "Invariant";
        break;
    }

    switch(FormConstr) {
      case IMPLIES:
        Result << "Terminating";
        break;
      case ASSUME_IMPLIES:
        Result << "Wellformed";
        break;
    }

    return Result.str();
  }

};

static const SimpleLoopConstraint AnyExitProvedCfTerminating = { ANY_EXIT, SINGLETON, IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint AnyExitStrongCfTerminating = { ANY_EXIT, SOME_EACH, IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint AnyExitWeakCfTerminating = { ANY_EXIT, SOME_SOME, IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint AnyExitProvedCfWellformed = { ANY_EXIT, SINGLETON, ASSUME_IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint AnyExitStrongCfWellformed = { ANY_EXIT, SOME_EACH, ASSUME_IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint AnyExitWeakCfWellformed = { ANY_EXIT, SOME_SOME, ASSUME_IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint SingleExitProvedCfTerminating = { SINGLE_EXIT, SINGLETON, IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint SingleExitStrongCfTerminating = { SINGLE_EXIT, SOME_EACH, IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint SingleExitWeakCfTerminating = { SINGLE_EXIT, SOME_SOME, IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint SingleExitProvedCfWellformed = { SINGLE_EXIT, SINGLETON, ASSUME_IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint SingleExitStrongCfWellformed = { SINGLE_EXIT, SOME_EACH, ASSUME_IMPLIES, MAYBE_INVARIANT };
static const SimpleLoopConstraint SingleExitWeakCfWellformed = { SINGLE_EXIT, SOME_SOME, ASSUME_IMPLIES, MAYBE_INVARIANT };

static const SimpleLoopConstraint SyntacticTerm = { ANY_EXIT, SINGLETON, IMPLIES, INVARIANT };
static const SimpleLoopConstraint AnyExitStrongCfInvariantTerminating = { ANY_EXIT, SOME_EACH, IMPLIES, INVARIANT };
static const SimpleLoopConstraint AnyExitWeakCfInvariantTerminating = { ANY_EXIT, SOME_SOME, IMPLIES, INVARIANT };
static const SimpleLoopConstraint AnyExitProvedCfInvariantWellformed = { ANY_EXIT, SINGLETON, ASSUME_IMPLIES, INVARIANT };
static const SimpleLoopConstraint AnyExitStrongCfInvariantWellformed = { ANY_EXIT, SOME_EACH, ASSUME_IMPLIES, INVARIANT };
static const SimpleLoopConstraint AnyExitWeakCfInvariantWellformed = { ANY_EXIT, SOME_SOME, ASSUME_IMPLIES, INVARIANT };
static const SimpleLoopConstraint SingleExitProvedCfInvariantTerminating = { SINGLE_EXIT, SINGLETON, IMPLIES, INVARIANT };
static const SimpleLoopConstraint SingleExitStrongCfInvariantTerminating = { SINGLE_EXIT, SOME_EACH, IMPLIES, INVARIANT };
static const SimpleLoopConstraint SingleExitWeakCfInvariantTerminating = { SINGLE_EXIT, SOME_SOME, IMPLIES, INVARIANT };
static const SimpleLoopConstraint SingleExitProvedCfInvariantWellformed = { SINGLE_EXIT, SINGLETON, ASSUME_IMPLIES, INVARIANT };
static const SimpleLoopConstraint SingleExitStrongCfInvariantWellformed = { SINGLE_EXIT, SOME_EACH, ASSUME_IMPLIES, INVARIANT };
static const SimpleLoopConstraint SingleExitWeakCfInvariantWellformed = { SINGLE_EXIT, SOME_SOME, ASSUME_IMPLIES, INVARIANT };

struct IncrementClassifierConstraint {
  const ExitsCountConstraint ECConstr;
  const ExitsWellformedConstraint EWConstr;
  const IncrementsConstraint IConstr;

  std::string str() const {
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
