#pragma once

#include <stdexcept>

#include "llvm/ADT/BitVector.h"
#include "llvm/Support/Debug.h"
#include "clang/AST/StmtVisitor.h"

#include "z3++.h"

#include "CmdLine.h"

using namespace clang;

namespace sloopy {

  struct AugComp;

  class AugInt {
    friend struct AugComp;

    typedef int longest_int;
    bool Unknown = false;
    longest_int Value = 0;

    public:
    AugInt() {}
    AugInt(int i) : Value(i) {}
    static AugInt UnknownAugInt () {
      AugInt A;
      A.Unknown = true;
      return A;
    }

    bool isUnknown() const {
      return Unknown;
    }
    void setUnknown() {
      Unknown = true;
    }
    longest_int getVal() const {
      assert(not Unknown);
      return Value;
    }

    std::string str() const {
      if (Unknown) return "Unknown";
      else {
        std::stringstream ss;
        ss << Value;
        return ss.str();
      }
    }

    AugInt operator+(const AugInt &Other) const {
      if (Unknown or Other.Unknown) return UnknownAugInt();
      if ((Value > 0 and Other.Value > 0 and Value > (std::numeric_limits<longest_int>::max() - Other.Value)) or 
          (Value < 0 and Other.Value < 0 and Value < (std::numeric_limits<longest_int>::min() - Other.Value))) {
        llvm_unreachable("overflow");
        return UnknownAugInt();
      }
      return AugInt(Value + Other.Value);
    }
    void operator+=(const int i) {
      if (Unknown) return;

      AugInt Result = *this + AugInt(i);

      Unknown = Result.Unknown;
      Value   = Result.Value;
    }
    bool operator<(const AugInt &Other) const {
      assert (not Unknown and not Other.Unknown);
      return Value < Other.Value;
    }
    bool operator>(const AugInt &Other) const {
      assert (not Unknown and not Other.Unknown);
      return Value > Other.Value;
    }
    // Unknown == Unknown - othw fixed point iterations dont terminate
    bool operator==(const AugInt &Other) const {
      /* if (Unknown or Other.Unknown) return false; */
      return Unknown == Other.Unknown and Value == Other.Value;
    }
    bool operator!=(const AugInt &Other) const {
      /* if (Unknown or Other.Unknown) return false; */
      return not ((*this) == Other);
    }
  };

  struct AugComp {
    bool operator() (const AugInt& lhs, const AugInt& rhs) const {
      return lhs.Unknown < rhs.Unknown or
            (lhs.Unknown == rhs.Unknown and lhs.Value < rhs.Value);
    }
  };

  typedef std::set<AugInt, AugComp> IncrementSet;

  namespace z3helper {

    typedef __int64 machine_int;

    bool anyUnknown(const IncrementSet &Increments) {
      for (auto I : Increments) {
        if (I.isUnknown()) return true;
      }
      return false;
    }

    bool anyZero(const IncrementSet &Increments) {
      return Increments.count(0);
    }

    bool singletonOne(const IncrementSet &Increments) {
      if (Increments.size() != 1) return false;
      AugInt i = *Increments.begin();
      return i == 1 or i == -1;
    }

    bool allLtZero(const IncrementSet &Increments) {
      if (anyUnknown(Increments)) return false;
      for (auto I : Increments) {
        if (not (I < 0)) return false;
      }
      return true;
    }

    bool allGtZero(const IncrementSet &Increments) {
      if (anyUnknown(Increments)) return false;
      for (auto I : Increments) {
        if (not (I > 0)) return false;
      }
      return true;
    }

    class exception {
      const std::string m_msg;
      public:
      exception(const std::string msg):m_msg(msg) {}
      std::string msg() const { return m_msg.c_str(); }
    };

    raw_ostream &operator<<(raw_ostream &S, const z3::expr &E) {
      S << Z3_ast_to_string(E.ctx(), E);
      return S;
    }

    machine_int as_int(const z3::expr &E) {
      machine_int ret;
      assert(Z3_TRUE == Z3_get_numeral_int64(E.ctx(), E, &ret));
      return ret;
    }

    class Z3Converter : public ConstStmtVisitor<Z3Converter, z3::expr> {
      llvm::OwningPtr<z3::context> Ctx;
      const z3::func_decl AddrOf, Deref;
      const bool NextExpression;
      std::map<const VarDecl*, z3::expr> MapClangZ3;

      public:

      Z3Converter(bool NextExpression=false) : Ctx(new z3::context),
        AddrOf(Ctx->function("__SLOOPY__AddrOf", Ctx->int_sort(), Ctx->int_sort())),
        Deref(Ctx->function("__SLOOPY__Deref", Ctx->int_sort(), Ctx->int_sort())),
        NextExpression(NextExpression) {}

      std::set<const VarDecl*> getConstants() {
        std::set<const VarDecl*> Result;
        for (auto Pair : MapClangZ3) {
          Result.insert(Pair.first);
        }
        return Result;
      }

      z3::context *take() {
        return Ctx.take();
      }

      z3::expr Run(const Expr* S) {
        MapClangZ3.clear();
        return Visit(S, false);
      }

      z3::expr Visit(const Expr* S, bool deep=true) {
        z3::expr E = ConstStmtVisitor<Z3Converter, z3::expr>::Visit(S->IgnoreParenCasts());

        if (not deep) return E;

        if (E.is_int()) {
          return E;
        } else if (E.is_bool()) {
          Z3_ast r = Z3_mk_ite(E.ctx(), E, E.ctx().int_val(1), E.ctx().int_val(0));
          return z3::expr(*Ctx, r);
        } else {
          llvm_unreachable("unhandled sort");
        }
      }

      const VarDecl *exprFor(const z3::expr &expr) {
        /* llvm::errs() << VD << "\n"; */
        for (auto Pair : MapClangZ3) {
          if (eq(Pair.second, expr)) {
            return Pair.first;
          }
        }
        llvm_unreachable("expr not in map");
      }
      z3::expr exprFor(const VarDecl *VD) throw (std::out_of_range) {
        /* llvm::errs() << VD << "\n"; */
        return MapClangZ3.at(VD);
      }

      /* z3::expr VisitArraySubscriptExpr(const ArraySubscriptExpr *ASE) { */
      /*   const Expr *Base = ASE->getBase(); */
      /*   const Expr *Idx = ASE->getIdx(); */
      /*   return z3::select(Visit(Base), Visit(Idx)); */
      /* } */

      z3::expr VisitUnaryOperator(const UnaryOperator *UO) {
        const Expr *Sub = UO->getSubExpr();
        switch (UO->getOpcode()) {
          case UO_PostInc:
            if (NextExpression)
              return Visit(Sub) + 1;
            return Visit(Sub);
          case UO_PostDec:
            if (NextExpression)
              return Visit(Sub) - 1;
            return Visit(Sub);
          case UO_PreInc:
            return Visit(Sub) + 1;
          case UO_PreDec:
            return Visit(Sub) - 1;
          case UO_Plus:
            return Visit(Sub);
          case UO_Minus:
            return -Visit(Sub);
          case UO_AddrOf:
            return AddrOf(Visit(Sub));
          case UO_Deref:
            return Deref(Visit(Sub));
          case UO_LNot:
            {
              z3::expr sube = Visit(Sub, false);
              if (sube.is_bool()) {
                return !sube;
              } else if (sube.is_numeral()) {
                return sube.ctx().int_val(as_int(sube) ? 0 : 1);
              } else if (sube.is_int()) {
                return sube == 0;
              } else {
                throw exception("unhandled type");
              }
            }
            break;
          default:
            throw exception("unhandled stmt");

            /* UO_Not */ 	
            /* UO_LNot */ 	
            /* UO_Real */ 	
            /* UO_Imag */ 	
            /* UO_Extension */ 	
        }
      }

      z3::expr VisitCallExpr(const CallExpr *CE) {
        // declaration
        const std::string Name = dyn_cast<NamedDecl>(CE->getCalleeDecl())->getNameAsString();
        const unsigned numArgs = CE->getNumArgs();
        std::vector<z3::sort> sorts;
        for (unsigned i=0; i<numArgs; i++) {
          sorts.push_back(Ctx->int_sort());
        }
        const z3::func_decl f = Ctx->function(Name.c_str(), CE->getNumArgs(), &sorts[0], Ctx->int_sort());

        // call
        std::vector<z3::expr> args;
        for (unsigned i=0; i<numArgs; i++) {
          auto argExpr = Visit(CE->getArg(i));
          args.push_back(argExpr);
        }
        return f(CE->getNumArgs(), &args[0]);
      }

      z3::expr VisitBinaryOperator(const BinaryOperator *BO) {
        const Expr *LHS = BO->getLHS();
        const Expr *RHS = BO->getRHS();
        z3::expr lhs = Visit(LHS);
        z3::expr rhs = Visit(RHS);
        switch (BO->getOpcode()) {
          case BO_Mul:
            return lhs * rhs;
          case BO_Div:
            return lhs * rhs;
          case BO_Rem:
            {
              Z3_ast r = Z3_mk_mod(*Ctx, lhs, rhs);
              return z3::expr(*Ctx, r);
            }
          case BO_Add:
            return lhs + rhs;
          case BO_Sub:
            return lhs - rhs;
          case BO_LT:
            return lhs < rhs;
          case BO_GT:
            return lhs > rhs;
          case BO_LE:
            return lhs <= rhs;
          case BO_GE:
            return lhs >= rhs;
          case BO_EQ:
            return lhs == rhs;
          case BO_NE:
            return lhs != rhs;
            /* case BO_Shl: */
            /*   return Visit(LHS) << Visit(RHS); */
            /* case BO_Shr: */
            /*   return Visit(LHS) >> Visit(RHS); */
            /* case BO_Comma: */
            /*   return rhs; */
          default:
            throw exception("unhandled stmt");
            /* BO_PtrMemD */
            /* BO_PtrMemI */
            /* case BO_And: */
            /* case BO_Xor: */
            /* case BO_Or: */
            /* case BO_LAnd: */
            /* case BO_LOr: */
            /* case BO_Assign: */
            /* case BO_MulAssign: */
            /* case BO_DivAssign: */
            /* case BO_RemAssign: */
            /* case BO_AddAssign: */
            /* case BO_SubAssign: */
            /* case BO_ShlAssign: */
            /* case BO_ShrAssign: */
            /* case BO_AndAssign: */
            /* case BO_XorAssign: */
            /* case BO_OrAssign: */
        }
      }

      z3::expr VisitIntegerLiteral(const IntegerLiteral *L) {
        return Ctx->int_val(L->getValue().getSExtValue());
      }

      z3::expr VisitDeclRefExpr(const DeclRefExpr *E) {
        if (const VarDecl *VD = dyn_cast<VarDecl>(E->getDecl())) {
          const std::string Name = E->getNameInfo().getAsString();
          z3::expr result = Ctx->int_const(Name.c_str());
          MapClangZ3.insert({VD, result});
          return result;
        }
        throw exception("unhandled stmt");
      }

      z3::expr VisitStmt(const Stmt *S) {
        throw exception("unhandled stmt");
      }
    };

    enum Monotonicity {
      NotMonotone,        // not monotone
      UnknownContent,     // *p
      UnknownDirection,   // monotone, but unknown whether increasing or decreasing (m*x)
      Constant,           // constant
      StrictIncreasing,   // strictly increasing
      StrictDecreasing,   // strictly decreasing
    };

    // m*x + b
    class LinearHelper {
      std::set<const VarDecl*> Constants;
      std::set<const z3::expr*> Z3AssumeWrapv, Z3AssumeWrapvOrRunsInto;
      std::set<const VarDecl*> AssumeWrapv, AssumeWrapvOrRunsInto;
      bool AssumeRightArrayContent = false;
      bool AssumeMNeq0 = false;
      bool AssumeLeBoundLtMaxVal = false;
      bool AssumeGeBoundGtMinVal = false;
      const unsigned PtrSize = 4;

      bool containsX(const z3::expr &X, const z3::expr &E) {
        if (E.is_const())
          return eq(E, X);

        assert(E.num_args());

        for (unsigned i = 0; i < E.num_args(); i++) {
          if (containsX(X, E.arg(i)))
            return true;
        }
        return false;
      }

      z3::expr simplify(z3::expr E) {
        z3::params p(E.ctx());
        p.set(":som", true);
        E = E.simplify(p);
        DEBUG_WITH_TYPE("z3", llvm::dbgs() << "simplifying " << E << "\n");

        // workaround http://stackoverflow.com/questions/18233389/why-is-with-numeral-argument-not-flattened-by-simplify
        if (E.decl().decl_kind() == Z3_OP_MUL and E.num_args() == 2) {
          if (E.arg(1).decl().decl_kind() == Z3_OP_MUL) {
            std::vector<Z3_ast> v;
            v.push_back(E.arg(0));
            for (unsigned i=0; i<E.arg(1).num_args(); i++) {
              v.push_back(E.arg(1).arg(i));
            }
            const Z3_ast *args = &v[0];
            Z3_ast r = Z3_mk_mul(E.ctx(), E.arg(1).num_args()+1, args);
            E.check_error();
            E = z3::expr(E.ctx(), r);
          }
        }

        DEBUG_WITH_TYPE("z3", llvm::dbgs() << "\tto " << Z3_ast_to_string(E.ctx(), E) << "\n");
        return E;
      }

      // m*x, if m is numeral Numeral, if m is Expression Expression
      enum class MultResult { NotMult, Numeral, Expression, UnknownContent };

      MultResult mult_in(const z3::expr &X, const z3::expr &E, machine_int &mRef) {
        if (E.decl().decl_kind() != Z3_OP_MUL)
          return MultResult::NotMult;

        const z3::expr S = simplify(E);

        bool foundX = false;
        bool foundNum = false;
        bool foundExprM = false;
        bool foundDeref = false;
        machine_int m;
        for (unsigned i = 0; i < S.num_args(); i++) {
          z3::expr C = S.arg(i);

          if (eq(C, X)) {
            if (foundX) return MultResult::NotMult;
            foundX = true;
          } else if (C.is_numeral()) {
            assert(not foundNum); // should be folded by simplify
            m = as_int(C);
            foundNum = true;
          } else if (not containsX(X, C)) {
            foundExprM = true;
          } else if (C.decl().name().str() == "__SLOOPY__AddrOf" and eq(C.arg(0), X)) {
            assert(!foundX);
            m = PtrSize;
            foundX = true;
          } else if (C.decl().name().str() == "__SLOOPY__Deref" and eq(C.arg(0), X)) {
            foundDeref = true;
          } else {
            // contains X
            return MultResult::NotMult;
          }
        }

        if (foundDeref) return MultResult::UnknownContent;

        if (!foundX) return MultResult::NotMult;  // constant
        if (foundExprM) return MultResult::Expression;
        if (foundNum) {
          mRef = m;
          return MultResult::Numeral;
        }
        llvm_unreachable("");
      }

      public:
      static const unsigned AssumptionSize = 6;

      std::pair<Monotonicity,machine_int> isLinearIn(const z3::expr &X, const z3::expr &E) {
        z3::expr S = simplify(E);

        if (S.decl().decl_kind() != Z3_OP_ADD) {
          try {
            S = S + 0;
          } catch (z3::exception) {
            return { NotMonotone, 0 };
          }
        }

        assert(S.num_args());

        machine_int m = 0, b = 0;
        bool setB = false, setM = false, foundExprM = false, foundDeref = false;
        for (unsigned i = 0; i < S.num_args(); i++) {
          z3::expr C = S.arg(i);
          if (eq(C, X)) {
            assert(!setM);
            m = 1;
            setM = true;
          } else if (C.is_numeral()) {
            assert(as_int(C) == 0 or not setB);
            b += as_int(C);
            setB = true;
          } else if (not containsX(X, C)) {
            /* blank */
          } else if (C.decl().name().str() == "__SLOOPY__AddrOf" and eq(C.arg(0), X)) {
            assert(!setM);
            m = PtrSize;
            setM = true;
          } else if (C.decl().name().str() == "__SLOOPY__Deref" and eq(C.arg(0), X)) {
            foundDeref = true;
          } else {
            MultResult Res = mult_in(X, C, m);
            if (Res == MultResult::Numeral) {
              assert(!setM);
              setM = true;
            } else if (Res == MultResult::UnknownContent) {
              foundDeref = true;
            } else if (Res != MultResult::NotMult) {
              foundExprM = true;
            } else {
              return { NotMonotone, 0 };
            }
          }
        }
        
        if (foundDeref) {
          return { UnknownContent, 0 };
        }
        if (foundExprM) {
          return { UnknownDirection, 0 };
        }

        assert(setM or m == 0);

        if (m > 0) return { StrictIncreasing, m };
        else if (m < 0) return { StrictDecreasing, m };
        else if (m == 0) return { Constant, m };
        else llvm_unreachable("false");
      }

      /* Check if X is linear in A, not in B, and will run into B given dir. */
      bool dropsToZero(const z3::expr &X, const IncrementSet Increments, const Z3_decl_kind DeclKind, const z3::expr &A, const z3::expr &B) {
        auto Pair = isLinearIn(X, A);
        Monotonicity Mon = Pair.first;
        machine_int m = Pair.second;

        if (containsX(X,B)) return false;

        if (Z3_OP_EQ == DeclKind and not anyUnknown(Increments) and not anyZero(Increments)) {
          if (Mon == StrictIncreasing or
              Mon == StrictDecreasing or
              Mon == UnknownDirection or
              Mon == UnknownContent) {
            if (Mon == UnknownContent) {
              AssumeRightArrayContent = true;
            } else if (Mon == UnknownDirection) {
              AssumeMNeq0 = true;
            }
            return true;
          }
        }

        if (Z3_OP_DISTINCT == DeclKind and singletonOne(Increments)) {
          if ((Mon == StrictIncreasing and ((B.is_numeral() and as_int(B) == 0) or m == 1)) or
              (Mon == StrictDecreasing and ((B.is_numeral() and as_int(B) == 0) or m == -1)) or
              Mon == UnknownContent) {
            if (Mon == UnknownContent) {
              AssumeRightArrayContent = true;
            } else {
              Z3AssumeWrapvOrRunsInto.insert(&X);
            }
            return true;
          }
        }

        if (Z3_OP_LE <= DeclKind and DeclKind <= Z3_OP_GT and
            (allLtZero(Increments) or allGtZero(Increments))) {
          if (Mon == StrictIncreasing or
              Mon == StrictDecreasing or
              Mon == UnknownDirection or
              Mon == UnknownContent) {
            if (Mon == UnknownContent) {
              AssumeRightArrayContent = true;
            } else if (Mon == UnknownDirection) {
              AssumeMNeq0 = true;
            }
            switch (DeclKind) {
              case Z3_OP_LE:
                AssumeLeBoundLtMaxVal = true;
              case Z3_OP_LT:
                if ((Mon == StrictIncreasing and allGtZero(Increments)) or
                    (Mon == StrictDecreasing and allLtZero(Increments))) {
                  return true;
                } else {
                  Z3AssumeWrapv.insert(&X);
                  return true;
                }
              case Z3_OP_GE:
                AssumeGeBoundGtMinVal = true;
              case Z3_OP_GT:
                if ((Mon == StrictIncreasing and allLtZero(Increments)) or
                    (Mon == StrictDecreasing and allGtZero(Increments))) {
                  return true;
                } else {
                  Z3AssumeWrapv.insert(&X);
                  return true;
                }
              default:
                llvm_unreachable("unhandled decl_kind");
            }
          }
        }

        return false;
      }

      bool dropsToZero(const z3::expr &X, const z3::expr &E, const IncrementSet Increments) {
        z3::expr S = E;

        /* linear expression */
        // while (E) is the same as while (E != 0)
        if (isLinearIn(X, S).first) {
          S = S != 0;
          DEBUG_WITH_TYPE("z3", llvm::dbgs() << "treating linear expression as " << S << "\n");
        }

        // propagate negation invards
        if (S.decl().decl_kind() == Z3_OP_NOT) {
          Z3_decl_kind ChildDeclKind = S.arg(0).decl().decl_kind();
          if ((Z3_OP_LE <= ChildDeclKind and ChildDeclKind <= Z3_OP_GT) or
              (Z3_OP_EQ <= ChildDeclKind and ChildDeclKind <= Z3_OP_DISTINCT) or
              ChildDeclKind == Z3_OP_NOT) {
            Z3_ast (*mk_func) (Z3_context, Z3_ast, Z3_ast);
            switch (ChildDeclKind) {
              case Z3_OP_EQ:
                return dropsToZero(X, S.arg(0).arg(0) != S.arg(0).arg(1), Increments);
              case Z3_OP_DISTINCT:
                return dropsToZero(X, S.arg(0).arg(0) == S.arg(0).arg(1), Increments);
              case Z3_OP_LE:
                mk_func = Z3_mk_gt;
                break;
              case Z3_OP_GE:
                mk_func = Z3_mk_lt;
                break;
              case Z3_OP_LT:
                mk_func = Z3_mk_ge;
                break;
              case Z3_OP_GT:
                mk_func = Z3_mk_le;
                break;
              case Z3_OP_NOT:
                return dropsToZero(X, S.arg(0).arg(0), Increments);
              default:
                llvm_unreachable("unhandled decl kind");
            }
            Z3_ast r = mk_func(S.ctx(), S.arg(0).arg(0), S.arg(0).arg(1));
            return dropsToZero(X, z3::expr(S.ctx(), r), Increments);
          }
        }

        Z3_decl_kind DeclKind = S.decl().decl_kind();

        /* inequality */

        if (not((Z3_OP_LE <= DeclKind and DeclKind <= Z3_OP_GT) or
                (Z3_OP_EQ <= DeclKind and DeclKind <= Z3_OP_DISTINCT))) {
            return false;
        }

        z3::expr lhs = S.arg(0);
        z3::expr rhs = S.arg(1);
        if (dropsToZero(X, Increments, DeclKind, lhs, rhs))
          return true;
        switch (DeclKind) {
          case Z3_OP_LE:
            DeclKind = Z3_OP_GE;
            break;
          case Z3_OP_GE:
            DeclKind = Z3_OP_LE;
            break;
          case Z3_OP_LT:
            DeclKind = Z3_OP_GT;
            break;
          case Z3_OP_GT:
            DeclKind = Z3_OP_LT;
            break;
          case Z3_OP_EQ:
          case Z3_OP_DISTINCT:
            break;
          default:
            llvm_unreachable("unhandled decl kind");
        }
        if (dropsToZero(X, Increments, DeclKind, rhs, lhs))
          return true;

        return false;
      }

      Monotonicity isLinearIn(const VarDecl *X, const Expr *E) {
        Z3Converter Z3C;
        try {
          z3::expr z3E = Z3C.Run(E);
          DEBUG_WITH_TYPE("z3", llvm::dbgs() << "is linear in? " << z3E << "\n");
          try {
            z3::expr z3X = Z3C.exprFor(X);
            Constants = Z3C.getConstants();
            return isLinearIn(z3X, z3E).first;
          } catch (std::out_of_range) {
            return Constant;
          }
        } catch (exception) {
          return NotMonotone;
        }
      }

      bool dropsToZero(const VarDecl *X, const Expr *E, const IncrementSet Increments, const bool negate=false) {
        Z3Converter Z3C;
        try {
          z3::expr z3E = Z3C.Run(E);
          if (negate) {
            if (not z3E.is_bool()) {
              z3E = z3E == 0;
            }
            z3E = !z3E;
          }
          DEBUG_WITH_TYPE("z3", llvm::dbgs() << "drops to zero? " << z3E << "\n");
          try {
            z3::expr z3X = Z3C.exprFor(X);
            Constants = Z3C.getConstants();
            bool Result = dropsToZero(z3X, z3E, Increments);
            for (auto expr : Z3AssumeWrapv) {
              AssumeWrapv.insert(Z3C.exprFor(*expr));
            }
            for (auto expr : Z3AssumeWrapvOrRunsInto) {
              AssumeWrapvOrRunsInto.insert(Z3C.exprFor(*expr));
            }
            return Result;
          } catch (std::out_of_range) {
            return false;
          }
        } catch (exception) {
          return false;
        }
      }

      std::set<const VarDecl*> getConstants() { return Constants; }

      /* 0 wrapv
       * 1 <= N, N < max
       * 2 >= N, N > min
       * 3 m*i+b, m != 0
       * 4 wrapv or runs in the right direction (equality)
       */
      llvm::BitVector getAssumptions() {
        llvm::BitVector Assumption(AssumptionSize);
        for (auto VD : AssumeWrapv) {
          if (not VD->getType().getTypePtr()->isUnsignedIntegerOrEnumerationType()) {
            Assumption.set(0);
            break;
          }
        }
        Assumption[1] = AssumeLeBoundLtMaxVal;
        Assumption[2] = AssumeGeBoundGtMinVal;
        Assumption[3] = AssumeMNeq0;
        for (auto VD : AssumeWrapvOrRunsInto) {
          if (not VD->getType().getTypePtr()->isUnsignedIntegerOrEnumerationType()) {
            Assumption.set(4);
            break;
          }
        }
        Assumption[5] = AssumeRightArrayContent;
        return Assumption;
      }
    };
  }
}
