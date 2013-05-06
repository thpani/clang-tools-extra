class BranchingClassifier : public LoopClassifier {
  public:
    void run(const MatchFinder::MatchResult &Result) {
      const Stmt *LS = Result.Nodes.getNodeAs<Stmt>(LoopName);

      const Stmt *Body = getLoopBody(LS);

      BranchVisitor Finder;
      // depth, nodes
      std::pair<int,int> BranchingInfo = Finder.findUsages(Body);

      std::stringstream sstm;
      sstm << "-Depth-" << BranchingInfo.first << "-Nodes-" << BranchingInfo.second;

      classify(Result, getStmtMarker(LS)+"Branch"+sstm.str());
    }
  private:
  class BranchVisitor : public RecursiveASTVisitor<BranchVisitor> {
    public:
      std::pair<int,int> findUsages(const Stmt *Body) {
        CurrentDepth = 0;
        MaxDepth = 0;
        NumNodes = 0;

        TraverseStmt(const_cast<Stmt *>(Body));
        return std::pair<int,int>(MaxDepth, NumNodes);
      }

      bool TraverseStmt(Stmt *StmtNode) {
        if (StmtNode == NULL) return true;
        bool ret;
        if (dyn_cast<IfStmt>(StmtNode)) {
          NumNodes++;
          CurrentDepth++;
          if (CurrentDepth > MaxDepth) MaxDepth = CurrentDepth;
          ret = RecursiveASTVisitor<BranchVisitor>::TraverseStmt(StmtNode);
          CurrentDepth--;
        }
        else {
          ret = RecursiveASTVisitor<BranchVisitor>::TraverseStmt(StmtNode);
        }
        return ret;
      }
    private:
      int CurrentDepth, MaxDepth, NumNodes;
  };
};
