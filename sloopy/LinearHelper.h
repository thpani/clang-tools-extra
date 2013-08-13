#pragma once

#include <stdexcept>

#include "clang/AST/StmtVisitor.h"
#include "z3++.h"

#include "CmdLine.h"

using namespace clang;

namespace sloopy {
  namespace z3helper {

    class exception {
      const std::string m_msg;
      public:
      exception(const std::string msg):m_msg(msg) {}
      std::string msg() const { return m_msg.c_str(); }
    };

    typedef __int64 machine_int;

    class Z3Converter : public ConstStmtVisitor<Z3Converter, z3::expr> {
      unsigned AddrOfCounter = 0;
      llvm::OwningPtr<z3::context> Ctx;
      const bool NextExpression;
      std::map<const VarDecl*, z3::expr> MapClangZ3;

      bool error = false;
      z3::expr setError() {
#if 0
        error = true;
        return Ctx->int_val(0);
#endif
        throw exception("unhandled stmt");
      }

      public:
      Z3Converter(bool NextExpression=false) : Ctx(new z3::context), NextExpression(NextExpression) {}

      bool getError() {
        return error;
      }

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
        AddrOfCounter = 0;
        MapClangZ3.clear();
        return Visit(S);
      }

      z3::expr Visit(const Expr* S) {
        return ConstStmtVisitor<Z3Converter, z3::expr>::Visit(S->IgnoreParenCasts());
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
            {
              std::stringstream ss;
              ss << "AddrOf" << AddrOfCounter++;
              z3::expr result = Ctx->int_const(ss.str().c_str());
              return result;
            }
          default:
            return setError();
            /* UO_Deref */ 	
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
          args.push_back(Visit(CE->getArg(i)));
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
              Z3_ast args[2] = { lhs, rhs };
              Z3_ast r = Z3_mk_sub(*Ctx, 2, args);
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
            return setError();
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
        return setError();
      }

      z3::expr VisitStmt(const Stmt *S) {
        return setError();
      }
    };

    machine_int as_int(const z3::expr &E) {
      machine_int ret;
      assert(Z3_TRUE == Z3_get_numeral_int64(E.ctx(), E, &ret));
      return ret;
    }

    enum Monotonicity {
      NotMonotone,        // not monotone
      UnknownDirection,   // monotone, but unknown whether increasing or decreasing (m*x)
      Constant,           // constant
      StrictIncreasing,   // strictly increasing
      StrictDecreasing    // strictly decreasing
    };

    // m*x + b
    class LinearHelper {
      machine_int m = 0, b = 0;
      std::set<const VarDecl*> Constants;
      std::set<const z3::expr*> Z3AssumeWrapv;
      std::set<const VarDecl*> AssumeWrapv;

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

      enum class MultResult { NotMult, Numeral, Expression };
      MultResult mult_in(const z3::expr &X, const z3::expr &E) {
        if (E.num_args() != 2 || E.decl().decl_kind() != Z3_OP_MUL)
          return MultResult::NotMult;

        z3::expr C0 = E.arg(0);
        z3::expr C1 = E.arg(1);

        if (eq(C0, X)) {
          if (C1.is_numeral()) {
            m = as_int(C1);
            return MultResult::Numeral;
          } else if (not eq(X, C1) and C1.is_const()) {
            return MultResult::Expression;
          } else if (not containsX(X, C1)) {
            return MultResult::Expression;
          }
        } else if (eq(C1, X)) {
          if (C0.is_numeral()) {
            m = as_int(C0);
            return MultResult::Numeral;
          } else if (not eq(X, C0) and C0.is_const()) {
            return MultResult::Expression;
          } else if (not containsX(X, C0)) {
            return MultResult::Expression;
          }
        }

        return MultResult::NotMult;
      }

      Monotonicity mToDirection() {
        if (m > 0) return StrictIncreasing;
        else if (m < 0) return StrictDecreasing;
        else if (m == 0) return Constant;
        else llvm_unreachable("false");
      }

      // FIXME actually, not StrictDecreasing.
      // FIXME rather, weak decreasing but we know it falls to 0
      Monotonicity comparisonMon(const z3::expr &X, const Z3_decl_kind DeclKind, const z3::expr &A, const z3::expr &B) {
        Monotonicity Mon;
        Mon = isLinearIn(X, A);
        if (Mon == StrictIncreasing and not containsX(X, B)) {
          switch (DeclKind) {
            case Z3_OP_LE:              // TODO assumes N < max_val
            case Z3_OP_LT:
              return StrictDecreasing;
            case Z3_OP_GE:
            case Z3_OP_GT:
              Z3AssumeWrapv.insert(&X);
              return StrictDecreasing;
            default:
              llvm_unreachable("unhandled decl_kind");
          }
        }
        if (Mon == StrictDecreasing and not containsX(X, B)) {
          switch (DeclKind) {
            case Z3_OP_LE:
            case Z3_OP_LT:
              Z3AssumeWrapv.insert(&X);
              return StrictDecreasing;
            case Z3_OP_GE:              // TODO assumes N > min_val
            case Z3_OP_GT:
              return StrictDecreasing;
              llvm_unreachable("unhandled decl_kind");
            default:
              llvm_unreachable("unhandled decl_kind");
          }
        }
        if (Mon and not containsX(X, B))
          return UnknownDirection;

        return NotMonotone;
      }

      public:
      Monotonicity isLinearIn(const z3::expr &X, const z3::expr &E) {
        z3::expr S = E.simplify();

        if (S.is_numeral()) {
          b = as_int(S);
          return Constant;
        }

        if (eq(S, X)) {
          m = 1;
          b = 0;
          return StrictIncreasing;
        }

        MultResult Res = mult_in(X, S);
        if (Res != MultResult::NotMult) {
          b = 0;
          if (Res == MultResult::Numeral) {
            return mToDirection();
          }
          return UnknownDirection;
        }

        if (S.decl().decl_kind() != Z3_OP_ADD)
          return NotMonotone;

        assert(S.num_args());

        bool setB = false, setM = false;
        for (unsigned i = 0; i < S.num_args(); i++) {
          z3::expr C = S.arg(i);
          if (eq(C, X)) {
            assert(!setM);
            m = 1;
            setM = true;
          } else if (C.is_numeral()) {
            assert(!setB);
            b = as_int(C);
            setB = true;
          } else if (mult_in(X, C) != MultResult::NotMult) {
            assert(!setM);
            setM = true;
          } else if (!containsX(X, C)) {
            /* blank */
          } else {
            return NotMonotone;
          }
        }
        
        // FIXME it's not safe to use m and b here
        return mToDirection();
      }

      Monotonicity isMonotonicIn(const z3::expr &X, const z3::expr &E) {
        z3::expr S = E.simplify();

        if (Monotonicity Res = isLinearIn(X, S)) return Res;

        Z3_decl_kind DeclKind = S.decl().decl_kind();
        switch (DeclKind) {
          case Z3_OP_LE:
          case Z3_OP_GE:
          case Z3_OP_LT:
          case Z3_OP_GT:
            break;
          case Z3_OP_NOT:
            return isMonotonicIn(X, S.arg(0));
          default:
            return NotMonotone;
        }
        z3::expr lhs = S.arg(0);
        z3::expr rhs = S.arg(1);
        if (Monotonicity Mon = comparisonMon(X, DeclKind, lhs, rhs))
          return Mon;
        // FIXME
        if (Monotonicity Mon = comparisonMon(X, DeclKind, rhs, lhs))
          return Mon;

        return NotMonotone;
      }

      Monotonicity runOnClangExpr(const VarDecl *X, const Expr *E, bool lin) {
        Z3Converter Z3C;
        try {
          z3::expr z3E = Z3C.Run(E);
          if (Z3C.getError()) return NotMonotone;
          if (DumpZ3) std::cerr << z3E << "\n";
          try {
            z3::expr z3X = Z3C.exprFor(X);
            Constants = Z3C.getConstants();
            Monotonicity Result = lin ? isLinearIn(z3X, z3E) : isMonotonicIn(z3X, z3E);
            for (auto expr : Z3AssumeWrapv) {
              AssumeWrapv.insert(Z3C.exprFor(*expr));
            }
            return Result;
          } catch (std::out_of_range) {
            return Constant;
          }
        } catch (exception) {
          return NotMonotone;
        }
      }

      Monotonicity isLinearIn(const VarDecl *X, const Expr *E) {
        return runOnClangExpr(X, E, true);
      }

      Monotonicity isMonotonicIn(const VarDecl *X, const Expr *E) {
        return runOnClangExpr(X, E, false);
      }

      machine_int getB() { return b; }
      machine_int getM() { return m; }
      std::set<const VarDecl*> getConstants() { return Constants; }
      std::set<const VarDecl*> getAssumeWrapv() { return AssumeWrapv; }
    };
  }
}
