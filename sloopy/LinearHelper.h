#pragma once

#include <stdexcept>

#include "llvm/ADT/BitVector.h"
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

      public:
      Z3Converter(bool NextExpression=false) : Ctx(new z3::context), NextExpression(NextExpression) {}

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
          case UO_LNot:
            {
              z3::expr sube = Visit(Sub);
              if (sube.is_bool()) {
                return !sube;
              } else if (sube.is_int()) {
                return sube != 0;
              } else {
                throw exception("unhandled type");
              }
            }
            break;
          default:
            throw exception("unhandled stmt");

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
      std::set<const VarDecl*> Constants;
      std::set<const z3::expr*> Z3AssumeWrapv;
      std::set<const VarDecl*> AssumeWrapv;
      bool AssumeLeBoundLtMaxVal = false;
      bool AssumeGeBoundGtMinVal = false;

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

      enum MultResult { NotMult, Numeral, Expression };
      z3::expr simplify(z3::expr E) {
        z3::params p(E.ctx());
        p.set(":som", true);
        E = E.simplify(p);

        // workaround http://stackoverflow.com/questions/18233389/why-is-with-numeral-argument-not-flattened-by-simplify
        if (E.decl().decl_kind() == Z3_OP_MUL and E.num_args() == 2 and E.arg(1).decl().decl_kind() == Z3_OP_MUL) {
          std::vector<Z3_ast> v;
          v.push_back(E.arg(0));
          for (unsigned i=0; i<E.arg(1).num_args(); i++) {
            v.push_back(E.arg(1).arg(i));
          }
          const Z3_ast *args = &v[0];
          Z3_ast r = Z3_mk_mul(E.ctx(), E.arg(1).num_args()+1, args);
          E.check_error();
          return z3::expr(E.ctx(), r);
        }

        return E;
      }
      MultResult mult_in(const z3::expr &X, const z3::expr &E, machine_int &m) {
        if (E.decl().decl_kind() != Z3_OP_MUL)
          return MultResult::NotMult;

        const z3::expr S = simplify(E);
        /* std::cerr << S << "\n"; */

        bool foundX = false;
        bool foundNum = false;
        bool foundExpr = false;
        for (unsigned i = 0; i < S.num_args(); i++) {
          z3::expr C = S.arg(i);

          if (eq(C, X)) {
            if (foundX) return MultResult::NotMult;
            foundX = true;
          } else if (C.is_numeral()) {
            assert(not foundNum); // should be folded by simplify
            m = as_int(C);
            foundNum = true;
          //} else if (not eq(X, C) and C.is_const()) {
            //return MultResult::Expression;
          } else if (not containsX(X, C)) {
            foundExpr = true;
          } else {
            // contains X
            return MultResult::NotMult;
          }
        }

        if (!foundX) return MultResult::Expression;
        if (foundExpr) return MultResult::Expression;
        if (foundNum) return MultResult::Numeral;
        llvm_unreachable("");
      }

      Monotonicity mToDirection(machine_int m) {
        if (m > 0) return StrictIncreasing;
        else if (m < 0) return StrictDecreasing;
        else if (m == 0) return Constant;
        else llvm_unreachable("false");
      }

      public:
      Monotonicity isLinearIn(const z3::expr &X, const z3::expr &E) {
        z3::expr S = simplify(E);

        if (S.decl().decl_kind() != Z3_OP_ADD) {
          try {
            S = S + 0;
          } catch (z3::exception) {
            return NotMonotone;
          }
        }

        assert(S.num_args());

        machine_int m = 0, b = 0;
        bool setB = false, setM = false, foundExpr = false;
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
          } else if (MultResult Res = mult_in(X, C, m)) {
            if (Res == MultResult::Numeral) {
              assert(!setM);
              setM = true;
            } else {
              foundExpr = true;
            }
          } else if (not containsX(X, C)) {
            /* blank */
          } else {
            return NotMonotone;
          }
        }
        
        if (setM) {
          return mToDirection(m);
        }
        if (foundExpr) {
          return UnknownDirection;
        }
        assert(m == 0);
        return Constant;
      }

      /* Check if X is linear in A, not in B, and will run into B given dir. */
      bool dropsToZero(const z3::expr &X, const int dir, const Z3_decl_kind DeclKind, const z3::expr &A, const z3::expr &B) {
        Monotonicity Mon;
        Mon = isLinearIn(X, A);
        if ((Mon == StrictIncreasing or
             Mon == StrictDecreasing or
             Mon == UnknownDirection
             ) and not containsX(X, B)) {
          switch (DeclKind) {
            case Z3_OP_LE:
              AssumeLeBoundLtMaxVal = true;
            case Z3_OP_LT:
              if (Mon == UnknownDirection or
                 (Mon == StrictIncreasing and dir < 0) or
                 (Mon == StrictDecreasing and dir > 0))
                Z3AssumeWrapv.insert(&X);
              break;
            case Z3_OP_GE:
              AssumeGeBoundGtMinVal = true;
            case Z3_OP_GT:
              if (Mon == UnknownDirection or
                 (Mon == StrictIncreasing and dir > 0) or
                 (Mon == StrictDecreasing and dir < 0))
                Z3AssumeWrapv.insert(&X);
              break;
            default:
              llvm_unreachable("unhandled decl_kind");
          }
          return true;
        }
        return false;
      }

      bool dropsToZero(const z3::expr &X, const z3::expr &E, const int dir) {
        if (dir == 0) return false;

        z3::expr S = E;

        Monotonicity Mon = isLinearIn(X, E);
        if (Mon == StrictIncreasing or Mon == StrictDecreasing) {
          Z3AssumeWrapv.insert(&X);
          return true;
        }

        if (S.decl().decl_kind() == Z3_OP_NOT) {
          Z3_decl_kind ChildDeclKind = S.arg(0).decl().decl_kind();
          if ((Z3_OP_LE <= ChildDeclKind and ChildDeclKind <= Z3_OP_GT) or
               ChildDeclKind == Z3_OP_NOT) {
            Z3_ast (*mk_func) (Z3_context, Z3_ast, Z3_ast);
            switch (ChildDeclKind) {
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
                return dropsToZero(X, S.arg(0).arg(0), dir);
              default:
                llvm_unreachable("unhandled decl kind");
            }
            Z3_ast r = mk_func(E.ctx(), S.arg(0).arg(0), S.arg(0).arg(1));
            return dropsToZero(X, z3::expr(E.ctx(), r), dir);
          }
        }

        Z3_decl_kind DeclKind = S.decl().decl_kind();

        if (not (Z3_OP_LE <= DeclKind and DeclKind <= Z3_OP_GT)) {
            return false;
        }

        z3::expr lhs = S.arg(0);
        z3::expr rhs = S.arg(1);
        if (dropsToZero(X, dir, DeclKind, lhs, rhs))
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
          default:
            llvm_unreachable("unhandled decl kind");
        }
        if (dropsToZero(X, dir, DeclKind, rhs, lhs))
          return true;

        return false;
      }

      Monotonicity isLinearIn(const VarDecl *X, const Expr *E) {
        Z3Converter Z3C;
        try {
          z3::expr z3E = Z3C.Run(E);
          if (DumpZ3) std::cerr << z3E << "\n";
          try {
            z3::expr z3X = Z3C.exprFor(X);
            Constants = Z3C.getConstants();
            Monotonicity Result = isLinearIn(z3X, z3E);
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

      bool dropsToZero(const VarDecl *X, const Expr *E, const int dir, const bool negate=false) {
        Z3Converter Z3C;
        try {
          z3::expr z3E = Z3C.Run(E);
          if (negate) {
            if (!z3E.is_bool()) {
              return false;
            }
            z3E = !z3E;
          }
          if (DumpZ3) std::cerr << z3E << "\n";
          try {
            z3::expr z3X = Z3C.exprFor(X);
            Constants = Z3C.getConstants();
            bool Result = dropsToZero(z3X, z3E, dir);
            for (auto expr : Z3AssumeWrapv) {
              AssumeWrapv.insert(Z3C.exprFor(*expr));
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

      /* 0 -> wrapv
       * 1 -> <= N, N < max
       * 2 -> >= N, N > min
       */
      llvm::BitVector getAssumptions() {
        llvm::BitVector Assumption(3);
        for (auto VD : AssumeWrapv) {
          if (not VD->getType().getTypePtr()->isUnsignedIntegerOrEnumerationType()) {
            Assumption.set(0);
            break;
          }
        }
        Assumption[1] = AssumeLeBoundLtMaxVal;
        Assumption[2] = AssumeGeBoundGtMinVal;
        return Assumption;
      }
    };
  }
}
