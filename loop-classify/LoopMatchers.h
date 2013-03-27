const StatementMatcher DoStmtMatcher = doStmt().bind(LoopName);
const StatementMatcher ForStmtMatcher = forStmt().bind(LoopName);
const StatementMatcher ForRangeStmtMatcher = forRangeStmt().bind(LoopName);
const StatementMatcher WhileStmtMatcher = whileStmt().bind(LoopName);

const StatementMatcher AnyLoopMatcher = anyOf(
    DoStmtMatcher,
    ForStmtMatcher,
    ForRangeStmtMatcher,
    WhileStmtMatcher);
