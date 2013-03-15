static const StatementMatcher IntegerRefComparisonMatcher =
    expr(ignoringParenImpCasts(declRefExpr(to(
        varDecl(hasType(isInteger())).bind(ConditionVarName)))));

static const StatementMatcher IntegerLitComparisonMatcher =
    expr(ignoringParenImpCasts(hasType(isInteger()))).bind(ConditionBoundName);

static const StatementMatcher IncrementVarMatcher =
    declRefExpr(to(varDecl(hasType(isInteger())).bind(IncrementVarName)));

StatementMatcher AnyLoopMatcher = anyOf(
    doStmt().bind(LoopName),
    forStmt().bind(LoopName),
    forRangeStmt().bind(LoopName),
    whileStmt().bind(LoopName));

StatementMatcher DoStmtMatcher = doStmt().bind(LoopName);
StatementMatcher ForStmtMatcher = forStmt().bind(LoopName);
StatementMatcher ForRangeStmtMatcher = forRangeStmt().bind(LoopName);
StatementMatcher WhileStmtMatcher = whileStmt().bind(LoopName);

StatementMatcher FortranForLoopMatcher = forStmt(
    hasLoopInit(declStmt().bind(InitDeclStmt)),
    hasCondition(binaryOperator(hasLHS(IntegerRefComparisonMatcher),
                                hasRHS(IntegerLitComparisonMatcher)).bind(ConditionOperatorName)),
    hasIncrement(unaryOperator(hasUnaryOperand(IncrementVarMatcher)).bind(IncrementOperatorName)))
    .bind(LoopName);
