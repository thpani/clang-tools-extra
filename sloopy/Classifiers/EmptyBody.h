class EmptyBodyClassifier : public LoopClassifier {
  public:
    void run(const MatchFinder::MatchResult &Result) {
      const Stmt *LS = Result.Nodes.getNodeAs<Stmt>(LoopName);

      const Stmt *Body = getLoopBody(LS);

      if (isa<NullStmt>(Body) ||
          (isa<CompoundStmt>(Body) && cast<CompoundStmt>(Body)->size() == 0)) {
        classify(Result, getStmtMarker(LS)+"EMPTYBODY");
      }
    }
};
