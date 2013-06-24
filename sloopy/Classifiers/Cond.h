class ConditionClassifier : public LoopClassifier {
  public:
  virtual void run(const MatchFinder::MatchResult &Result) {
    const Stmt *LS = Result.Nodes.getNodeAs<clang::Stmt>(LoopName);
    const Expr *Cond = getLoopCond(LS);
    Marker=getStmtMarker(LS);
    classifyCond(Result, Cond);
  }
  protected:
  void classifyCond(const MatchFinder::MatchResult &Result, const Expr *Cond) {
    std::string suffix;

    /* for(<init>;;<incr>) */
    if (Cond == NULL) {
      classify(Result, Marker+"Cond-NULL");
      return;
    }

    /* while(<INTEGER LITERAL>) */
    if (isIntegerConstant(Cond, Result.Context)) {
      if (getIntegerConstant(Cond, Result.Context).getBoolValue()) {
        classify(Result, Marker+"Cond-TRUE");
      }
      else {
        classify(Result, Marker+"Cond-FALSE");
        if (Result.Nodes.getNodeAs<clang::DoStmt>(LoopName)) {
          classify(Result, Marker+"MacroPseudoBlock");
        }
        /* Result.Nodes.getNodeAs<clang::Stmt>(LoopName)->dumpPretty(*Result.Context); // C source */
      }
      return;
    }

    /* while (<UNARY>) */
    if (const UnaryOperator *ConditionOp = dyn_cast<UnaryOperator>(Cond)) {
      if (ConditionOp->isIncrementDecrementOp()) {
        classify(Result, Marker+"Cond-Unary-IncDec");
        return;
      }
      classify(Result, Marker+"Cond-Unary-Other");
      return;
    }

    /* while(<BINARY>) */
    if (const BinaryOperator *ConditionOp = dyn_cast<BinaryOperator>(Cond)) {
      if (ConditionOp->isEqualityOp()) {
        classify(Result, Marker+"Cond-Binary-Equality");
        return;
      }
      if (ConditionOp->isRelationalOp()) {
        classify(Result, Marker+"Cond-Binary-Rel");
        return;
      }
      if (ConditionOp->isAssignmentOp()) {
        classify(Result, Marker+"Cond-Binary-Assign");
        return;
      }
      if (ConditionOp->isLogicalOp()) {
        classify(Result, Marker+"Cond-Binary-Logic");
        return;
      }
      std::stringstream sstm;
      sstm << ConditionOp->getOpcode();
      classify(Result, Marker+"Cond-Binary-Other-"+sstm.str());
      return;
    }

    /* while ((cast)...) {} */
    if (/*const CastExpr *Cast =*/ dyn_cast<CastExpr>(Cond)) {
      classify(Result, Marker+"Cond-Cast");
      return;
    }

    const std::string Cls(Cond->getStmtClassName());
    classify(Result, Marker+"Cond-Other-"+Cls);
    return;
  }
};
