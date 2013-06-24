#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_MATCHERS_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_MATCHERS_H_

using namespace clang::ast_matchers;

const char LoopName[] = "forLoop";
const char FunctionName[] = "functionDecl";
const char VarDeclName[] = "varDecl";

const DeclarationMatcher FunctionMatcher = functionDecl(isDefinition()).bind(FunctionName);
const StatementMatcher VarDeclRefMatcher = declRefExpr().bind(VarDeclName);
const StatementMatcher DoStmtMatcher = doStmt().bind(LoopName);
const StatementMatcher ForStmtMatcher = forStmt().bind(LoopName);
const StatementMatcher ForRangeStmtMatcher = forRangeStmt().bind(LoopName);
const StatementMatcher WhileStmtMatcher = whileStmt().bind(LoopName);

const StatementMatcher AnyLoopMatcher = anyOf(
    DoStmtMatcher,
    ForStmtMatcher,
    ForRangeStmtMatcher,
    WhileStmtMatcher);

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_MATCHERS_H_
