#include "gtest/gtest.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include"z3++.h"

#include "LinearHelper.h"

using namespace clang;
using namespace clang::tooling;
using namespace z3;
using namespace sloopy::z3helper;

class Z3ClassConsumer : public ASTConsumer {
  z3::expr &Expr;
  public:
  Z3ClassConsumer(expr &E) : Expr(E) {}
  virtual void HandleTranslationUnit(ASTContext &Context) {
    for (auto I = Context.getTranslationUnitDecl()->decls_begin(),
              E = Context.getTranslationUnitDecl()->decls_end();
              I!=E; I++) {
      if (I->getKind() == Decl::Function) {
        for (auto IS = I->getBody()->child_begin(),
                  ES = I->getBody()->child_end();
                  IS != ES; IS++) {
          if (const IfStmt *If = dyn_cast<IfStmt>(*IS)) {
            Z3Converter Z;
            Expr = Z.Run(If->getCond());
            Z.take(); // FIXME memleak; we leave the context around because the tests will need it
          }
        }
      }
    }
  }
};

class Z3Action : public ASTFrontendAction {
  z3::expr &E;
  public:
  Z3Action(expr &E) : E(E) {}
  virtual ASTConsumer *CreateASTConsumer(
    CompilerInstance &Compiler, llvm::StringRef InFile) {
    return new Z3ClassConsumer(E);
  }
};

bool z3Expr(StringRef Code, expr &e) {
  return runToolOnCode(new Z3Action(e), Code);
};

TEST(LinearHelperTest, testZ3Converter) {
  context c;
  expr e = c.int_const("?");
  bool res;

  res = z3Expr("void a() { int i; if (i) {} }", e);
  EXPECT_TRUE(res);
  EXPECT_TRUE(e.is_const());
  EXPECT_EQ("i", e.decl().name().str());
  res = z3Expr("void a() { int i; if (+i) {} }", e);
  EXPECT_TRUE(res);
  EXPECT_TRUE(e.is_const());
  EXPECT_EQ("i", e.decl().name().str());
  res = z3Expr("void a() { int i; if (-i) {} }", e);
  EXPECT_TRUE(res);
  EXPECT_TRUE(e.is_app());
  EXPECT_EQ(Z3_OP_UMINUS, e.decl().decl_kind());
  EXPECT_EQ(1u, e.num_args());
  EXPECT_EQ("i", e.arg(0).decl().name().str());
  res = z3Expr("void a() { int i,j; if ((3*i) + (2-j)) {} }", e);
  EXPECT_TRUE(res);
  EXPECT_TRUE(e.is_app());
  EXPECT_EQ(Z3_OP_ADD, e.decl().decl_kind());
  EXPECT_EQ(2u, e.num_args());
  EXPECT_EQ(Z3_OP_MUL, e.arg(0).decl().decl_kind());
  EXPECT_EQ(2u, e.arg(0).num_args());
  EXPECT_EQ(3, as_int(e.arg(0).arg(0)));
  EXPECT_EQ("i", e.arg(0).arg(1).decl().name().str());
  EXPECT_EQ(Z3_OP_SUB, e.arg(1).decl().decl_kind());
  EXPECT_EQ(2u, e.arg(1).num_args());
  EXPECT_EQ(2, as_int(e.arg(1).arg(0)));
  EXPECT_EQ("j", e.arg(1).arg(1).decl().name().str());
}

TEST(LinearHelperTest, testLinear) {
  context c;
  expr x = c.int_const("x");
  expr y = c.int_const("y");
  const sort *domainF  = { };
  const sort domainG[] = { c.int_sort() };
  const expr *noargs = { };
  func_decl f = c.function("FUNC_f", 0, domainF, c.int_sort());
  func_decl g = c.function("FUNC_g", 1, domainG, c.int_sort());

  LinearHelper H;
  EXPECT_TRUE ( H.isLinearIn(x,   0*x     +1)); EXPECT_EQ(  0, H.getM()); EXPECT_EQ( 1, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,     x       )); EXPECT_EQ(  1, H.getM()); EXPECT_EQ( 0, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,    -x       )); EXPECT_EQ( -1, H.getM()); EXPECT_EQ( 0, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,   1*x     +1)); EXPECT_EQ(  1, H.getM()); EXPECT_EQ( 1, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,   3*x     +1)); EXPECT_EQ(  3, H.getM()); EXPECT_EQ( 1, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x, -42*x     +1)); EXPECT_EQ(-42, H.getM()); EXPECT_EQ( 1, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,     x     +1)); EXPECT_EQ(  1, H.getM()); EXPECT_EQ( 1, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,     x     -4)); EXPECT_EQ(  1, H.getM()); EXPECT_EQ(-4, H.getB());
  EXPECT_TRUE ( H.isLinearIn(x,     x-4  * (x+2) )); EXPECT_EQ(-3, H.getM()); EXPECT_EQ(-8, H.getB());
  EXPECT_FALSE( H.isLinearIn(x,    x*x           ));
  EXPECT_FALSE( H.isLinearIn(x,    (x-4) * (x+2) ));
  EXPECT_TRUE ( H.isLinearIn(x,    x + y         ));
  EXPECT_TRUE ( H.isLinearIn(x,    x * y         ));
  EXPECT_TRUE ( H.isLinearIn(x,    x + f(0, noargs) ));
  EXPECT_TRUE ( H.isLinearIn(x,    x * f(0, noargs) ));
  EXPECT_TRUE ( H.isLinearIn(x,    x + g(y)  ));
  EXPECT_TRUE ( H.isLinearIn(x,    x * g(y)  ));
  EXPECT_FALSE( H.isLinearIn(x,    x + g(x)  ));
  EXPECT_FALSE( H.isLinearIn(x,    x * g(x)  ));
}

TEST(LinearHelperTest, testMonotonic) {
  context c;
  expr x = c.int_const("x");
  expr y = c.int_const("y");
  const sort *domainF  = { };
  const sort domainG[] = { c.int_sort() };
  const expr *noargs = { };
  func_decl f = c.function("FUNC_f", 0, domainF, c.int_sort());
  func_decl g = c.function("FUNC_g", 1, domainG, c.int_sort());

  LinearHelper H;
  EXPECT_EQ(Constant,   H.isMonotonicIn(x,   0*x     +1));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,     x       ));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x,    -x       ));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,   1*x     +1));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,   3*x     +1));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x, -42*x     +1));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,     x     +1));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,     x     -4));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x,     x-4  * (x+2) ));
  EXPECT_EQ(NotMonotone,  H.isMonotonicIn(x,    x*x           ));
  EXPECT_EQ(NotMonotone,  H.isMonotonicIn(x,    (x-4) * (x+2) ));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,    x + y         ));
  EXPECT_EQ(UnknownDirection, H.isMonotonicIn(x,    x * y         ));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,    x + f(0, noargs) ));
  EXPECT_EQ(UnknownDirection, H.isMonotonicIn(x,    x * f(0, noargs) ));
  EXPECT_EQ(StrictIncreasing, H.isMonotonicIn(x,    x + g(y)  ));
  EXPECT_EQ(UnknownDirection, H.isMonotonicIn(x,    x * g(y)  ));
  EXPECT_EQ(NotMonotone, H.isMonotonicIn(x,    x + g(x)  ));
  EXPECT_EQ(NotMonotone, H.isMonotonicIn(x,    x * g(x)  ));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x, x < y   ));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x, x <= y  ));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x, y > x   ));
  EXPECT_EQ(StrictDecreasing, H.isMonotonicIn(x, y >= x  ));
}

TEST(LinearHelperTest, testLinearHelperWithConverter) {
  context c;
  expr e = c.int_const("?");
  bool res;
  LinearHelper H;

  res = z3Expr("void a() { int i,j; if ((3*i) + (6%4)) {} }", e);
  EXPECT_TRUE(res);
  EXPECT_TRUE(H.isLinearIn(e.arg(0).arg(1), e)); EXPECT_EQ(3, H.getM()); EXPECT_EQ(2, H.getB());
}
