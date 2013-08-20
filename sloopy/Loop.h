#pragma once

#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/GraphWriter.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/StmtVisitor.h"

using namespace clang;

namespace sloopy {

class MergedLoopDescriptor {
  public:
  const CFGBlock *Header;
  std::set<const CFGBlock*> Tails;
  std::set<const CFGBlock*> Body;
  std::vector<const MergedLoopDescriptor*> NestedLoops, ProperlyNestedLoops, NestingLoops;
  MergedLoopDescriptor(
    const CFGBlock *Header,
    std::set<const CFGBlock*> Tails,
    std::set<const CFGBlock*> Body) : Header(Header), Tails(Tails), Body(Body) {}
  void addNestedLoop(MergedLoopDescriptor *D) {
    NestedLoops.push_back(D);
    D->NestingLoops.push_back(this);
    ProperlyNestedLoops.push_back(D);
  }
  void addTriviallyNestedLoop(MergedLoopDescriptor *D) {
    NestedLoops.push_back(D);
    D->NestingLoops.push_back(this);
  }
  bool operator< (const MergedLoopDescriptor &Other) const {
    return (this->Header < Other.Header);
  }
};
struct LoopDescriptor {
  const CFGBlock *Header, *Tail;
  std::set<const CFGBlock*> Body;
};
struct SlicingCriterion {
  const std::set<const VarDecl*> Vars;
  const std::set<const CFGBlock*> Locations;
};

class NaturalLoopBlock;

class NaturalLoop {
  private:
    NaturalLoopBlock *Entry, *Exit;
    std::list<NaturalLoopBlock*> Blocks;
    std::set<const VarDecl*> ControlVars;
    std::set<const CFGBlock*> Tails;
    const Stmt *LoopStmt;
    std::string Identifier;
    const NaturalLoop *Unsliced;
  public:
    ~NaturalLoop();
    void build(
        const CFGBlock *Header,
        const std::set<const CFGBlock*> Tails,
        const std::set<const CFGBlock*> Blocks,
        const std::set<const VarDecl*> ControlVars,
        const std::set<const Stmt*> *TrackedStmts = NULL,
        const std::set<const CFGBlock*> *TrackedBlocks = NULL,
        const NaturalLoop *Unsliced = NULL);
    void dump() const;
    void view(const LangOptions &LO = LangOptions()) const;
    void write(const LangOptions &LO = LangOptions()) const;

    typedef std::list<NaturalLoopBlock*>::iterator                      iterator;
    typedef std::list<NaturalLoopBlock*>::const_iterator                const_iterator;
    typedef std::list<NaturalLoopBlock*>::reverse_iterator              reverse_iterator;
    typedef std::list<NaturalLoopBlock*>::const_reverse_iterator        const_reverse_iterator;

    iterator                   begin()             { return Blocks.begin();   }
    iterator                   end()               { return Blocks.end();     }
    const_iterator             begin()       const { return Blocks.begin();   }
    const_iterator             end()         const { return Blocks.end();     }

    /// \brief Provides a custom implementation of the iterator class to have the
    /// same interface as Function::iterator.
    class graph_iterator {
    public:
      typedef const NaturalLoopBlock          value_type;
      typedef value_type&                     reference;
      typedef value_type*                     pointer;
      typedef std::list<NaturalLoopBlock*>::iterator ImplTy;

      graph_iterator(const ImplTy &i) : I(i) {}

      bool operator==(const graph_iterator &X) const { return I == X.I; }
      bool operator!=(const graph_iterator &X) const { return I != X.I; }

      reference operator*()    const { return **I; }
      pointer operator->()     const { return  *I; }
      operator NaturalLoopBlock* ()          { return  *I; }

      graph_iterator &operator++() { ++I; return *this; }
      graph_iterator &operator--() { --I; return *this; }

    private:
      ImplTy I;
    };

    class const_graph_iterator {
    public:
      typedef const NaturalLoopBlock                  value_type;
      typedef value_type&                     reference;
      typedef value_type*                     pointer;
      typedef std::list<NaturalLoopBlock*>::const_iterator ImplTy;

      const_graph_iterator(const ImplTy &i) : I(i) {}

      bool operator==(const const_graph_iterator &X) const { return I == X.I; }
      bool operator!=(const const_graph_iterator &X) const { return I != X.I; }

      reference operator*() const { return **I; }
      pointer operator->()  const { return  *I; }
      operator NaturalLoopBlock* () const { return  *I; }

      const_graph_iterator &operator++() { ++I; return *this; }
      const_graph_iterator &operator--() { --I; return *this; }

    private:
      ImplTy I;
    };

    NaturalLoopBlock &                getEntry()             { return *Entry; }
    const NaturalLoopBlock &          getEntry()    const    { return *Entry; }
    NaturalLoopBlock &                getExit()              { return *Exit; }
    const NaturalLoopBlock &          getExit()     const    { return *Exit; }
    graph_iterator nodes_begin() { return graph_iterator(Blocks.begin()); }
    graph_iterator nodes_end() { return graph_iterator(Blocks.end()); }
    const_graph_iterator nodes_begin() const {
      return const_graph_iterator(Blocks.begin());
    }
    const_graph_iterator nodes_end() const {
      return const_graph_iterator(Blocks.end());
    }
    unsigned size() const { return Blocks.size(); }

    const Stmt *getLoopStmt() const { return LoopStmt; }
    
    const NaturalLoop *getUnsliced() const {
      return Unsliced ? Unsliced : this;
    }

    std::string getLoopStmtMarker() const {
      switch (LoopStmt->getStmtClass()) {
        case Stmt::ForStmtClass:
          return "FOR";
        case Stmt::WhileStmtClass:
          return "WHILE";
        case Stmt::DoStmtClass:
          return "DO";
        case Stmt::GotoStmtClass:
          return "GOTO";
        case Stmt::LabelStmtClass:
          return "UnstructuredFlow";
        default:
          llvm_unreachable("Unknown loop stmt.");
      }
    }

    std::pair<const PresumedLoc, const std::vector<PresumedLoc>> getLoopStmtLocation(const SourceManager* SM) const {
      const Stmt *S = LoopStmt;
      if (const GotoStmt *GS = dyn_cast<GotoStmt>(S)) {
        S = GS->getLabel()->getStmt();
      }
      SourceLocation SLHead = S->getSourceRange().getBegin();
      PresumedLoc PLHead = SM->getPresumedLoc(SLHead);

      std::vector<PresumedLoc> PLBacks;
      for (const CFGBlock *Tail : Tails) {
        const Stmt *S;
        if (Tail->getTerminator()) {
          S = Tail->getTerminator().getStmt();
        } else {
          S = Tail->getLoopTarget();
        }
        if (!S) {
          // block before labeled block that is the header
          for (CFGBlock::const_reverse_iterator I = Tail->rbegin(),
                                                E = Tail->rend();
                                                I != E; I++) {
            auto CStmt = I->getAs<CFGStmt>();
            if (CStmt) {
              S = CStmt->getStmt();
              break;
            }
          }
          if (!S) {
            S = Tail->getLabel();
          }
        }
        assert(S);
        SourceLocation SLBack = S->getSourceRange().getEnd();
        PresumedLoc PLBack = SM->getPresumedLoc(SLBack);
        PLBacks.push_back(PLBack);
      }

      return std::make_pair(PLHead, PLBacks);
    }

    std::set<const VarDecl*> getControlVars() const {
      return ControlVars;
    }
};

class NaturalLoopTerminator {
  const Stmt *S;

  public:
  NaturalLoopTerminator(const Stmt *S) : S(S) {}

  const Stmt *getStmt() const { return S; }

  operator const Stmt *() const { return getStmt(); }

  const Stmt *operator->() const { return getStmt(); }

  const Stmt &operator*() const { return *getStmt(); }

  LLVM_EXPLICIT operator bool() const { return getStmt(); }
};

class NaturalLoopBlock {
  private:
    friend class NaturalLoop;
    
    const unsigned BlockID;
    const Stmt *Label;
    const NaturalLoopTerminator Terminator;

    std::list<const Stmt*> Stmts;
    std::set<NaturalLoopBlock *> Succs, Preds;
  public:
    NaturalLoopBlock(unsigned BlockID, const Stmt *LabelStmt = NULL, const Stmt *TerminatorStmt = NULL) :
      BlockID(BlockID), Label(LabelStmt), Terminator(TerminatorStmt) {}

    unsigned getBlockID() const { return BlockID; }
    const NaturalLoopTerminator getTerminator() const { return Terminator; }

    typedef std::list<const Stmt*>::iterator                      iterator;
    typedef std::list<const Stmt*>::const_iterator                const_iterator;
    typedef std::list<const Stmt*>::reverse_iterator              reverse_iterator;
    typedef std::list<const Stmt*>::const_reverse_iterator        const_reverse_iterator;

    const Stmt* front() const { return Stmts.front();   }
    const Stmt* back()  const { return Stmts.back();    }

    iterator                   begin()             { return Stmts.begin();   }
    iterator                   end()               { return Stmts.end();     }
    const_iterator             begin()       const { return Stmts.begin();   }
    const_iterator             end()         const { return Stmts.end();     }

    typedef std::set<NaturalLoopBlock *>::iterator             succ_iterator;
    typedef std::set<NaturalLoopBlock *>::const_iterator const_succ_iterator;

    succ_iterator                succ_begin()        { return Succs.begin();   }
    succ_iterator                succ_end()          { return Succs.end();     }
    const_succ_iterator          succ_begin()  const { return Succs.begin();   }
    const_succ_iterator          succ_end()    const { return Succs.end();     }

    unsigned                     succ_size()   const { return Succs.size();    }
    bool                         succ_empty()  const { return Succs.empty();   }

    typedef std::set<NaturalLoopBlock *>::iterator             pred_iterator;
    typedef std::set<NaturalLoopBlock *>::const_iterator const_pred_iterator;

    pred_iterator                pred_begin()        { return Preds.begin();   }
    pred_iterator                pred_end()          { return Preds.end();     }
    const_pred_iterator          pred_begin()  const { return Preds.begin();   }
    const_pred_iterator          pred_end()    const { return Preds.end();     }

    unsigned                     pred_size()   const { return Preds.size();    }
    bool                         pred_empty()  const { return Preds.empty();   }

    const Stmt *getLabel() const {
      return Label;
    }
    const Expr *getTerminatorCondition() const {
      Stmt *Terminator = const_cast<Stmt*>(this->Terminator.getStmt());
      if (!Terminator)
        return NULL;

      Expr *E = NULL;

      switch (Terminator->getStmtClass()) {
        default:
          break;

        case Stmt::CXXForRangeStmtClass:
          E = cast<CXXForRangeStmt>(Terminator)->getCond();
          break;

        case Stmt::ForStmtClass:
          E = cast<ForStmt>(Terminator)->getCond();
          break;

        case Stmt::WhileStmtClass:
          E = cast<WhileStmt>(Terminator)->getCond();
          break;

        case Stmt::DoStmtClass:
          E = cast<DoStmt>(Terminator)->getCond();
          break;

        case Stmt::IfStmtClass:
          E = cast<IfStmt>(Terminator)->getCond();
          break;

        case Stmt::ChooseExprClass:
          E = cast<ChooseExpr>(Terminator)->getCond();
          break;

        case Stmt::IndirectGotoStmtClass:
          E = cast<IndirectGotoStmt>(Terminator)->getTarget();
          break;

        case Stmt::SwitchStmtClass:
          E = cast<SwitchStmt>(Terminator)->getCond();
          break;

        case Stmt::BinaryConditionalOperatorClass:
          E = cast<BinaryConditionalOperator>(Terminator)->getCond();
          break;

        case Stmt::ConditionalOperatorClass:
          E = cast<ConditionalOperator>(Terminator)->getCond();
          break;

        case Stmt::BinaryOperatorClass: // '&&' and '||'
          E = cast<BinaryOperator>(Terminator)->getLHS();
          break;

/*         case Stmt::ObjCForCollectionStmtClass: */
/*           return Terminator; */
      }

      return E ? E->IgnoreParens() : NULL;
    }
};

inline void WriteAsOperand(raw_ostream &OS, const NaturalLoopBlock *BB, bool t) {
  OS << "BB#" << BB->getBlockID();
}

NaturalLoop::~NaturalLoop() {
  for (auto B : *this) {
    delete B;
  }
}

void NaturalLoop::dump() const {
  llvm::errs() << "Natural Loop\n";
  llvm::errs() << "============\n";
  for (auto Block : Blocks) {
    llvm::errs() << "Block " << Block->getBlockID() << "\n";
    llvm::errs() << "\tstatements:\n";
    for (auto Stmt : Block->Stmts) {
      llvm::errs() << "\t";
      Stmt->printPretty(llvm::errs(), NULL, PrintingPolicy(LangOptions()));
      llvm::errs() << "\n";
    }
    llvm::errs() << "\tsuccessors:\n";
    for (auto Succ : Block->Succs) {
      if (not Succ) continue;
      llvm::errs() << "\t" << Succ->getBlockID() << "\n";
    }
    llvm::errs() << "\tpredecessors:\n";
    for (auto Pred : Block->Preds) {
      llvm::errs() << "\t" << Pred->getBlockID() << "\n";
    }
  }
}

void NaturalLoop::build(
    const CFGBlock *Header,
    const std::set<const CFGBlock*> Tails,
    const std::set<const CFGBlock*> CFGBlocks,
    const std::set<const VarDecl*> ControlVars,
    const std::set<const Stmt*> *TrackedStmts, 
    const std::set<const CFGBlock*> *TrackedBlocks,
    const NaturalLoop *Unsliced) {


  assert(TrackedStmts != NULL || TrackedBlocks == NULL);
  assert(TrackedStmts == NULL || TrackedBlocks != NULL);
  assert(Tails.size() > 0);

  Entry = new NaturalLoopBlock(-1);
  Exit = new NaturalLoopBlock(0);
  this->Tails = Tails;
  this->ControlVars = ControlVars;
  this->Unsliced = Unsliced;

  const CFGBlock *Tail = *Tails.begin();
  // FOR, WHILE, DO-WHILE
  LoopStmt = Tail->getLoopTarget();
  if (!LoopStmt) {
    for (const CFGBlock *Tail : Tails) {
      if (const GotoStmt *GS = dyn_cast_or_null<GotoStmt>(Tail->getTerminator().getStmt())) {
        LoopStmt = GS;
      } else {
        LoopStmt = Header->getLabel();
        break;
      }
    }
  }
  assert(LoopStmt && "No loop stmt!");

  std::map<const CFGBlock*, NaturalLoopBlock*> Map;

  // construct blocks with stmts
  for (auto Current : CFGBlocks) {
    const Stmt *S =
      TrackedBlocks != NULL && TrackedBlocks->count(Current) == 0 ?
      NULL : Current->getTerminator();
    NaturalLoopBlock *CBlock = new NaturalLoopBlock(Current->getBlockID(), Current->getLabel(), S);
    for (auto Element : *Current) {
      auto CStmt = Element.getAs<CFGStmt>();
      assert(CStmt);
      const Stmt *S = CStmt->getStmt();
      if (TrackedStmts != NULL && TrackedStmts->count(S) == 0) continue;
      CBlock->Stmts.push_back(S);
    }
    assert(Map.find(Current) == Map.end());
    Map[Current] = CBlock;
    Blocks.push_back(CBlock);
  }
  // add successors/predecessors to blocks
  for (auto Current : CFGBlocks) {
    for (CFGBlock::const_succ_iterator I = Current->succ_begin(),
                                       E = Current->succ_end();
                                       I != E; I++) {
      const CFGBlock *Succ = *I;
      if (Succ) {
        NaturalLoopBlock *CSuccBlock;
        if (Map.find(Succ) == Map.end()) {
          CSuccBlock = Exit;
        }
        else {
          CSuccBlock = Map[Succ];
        }
        Map[Current]->Succs.insert(CSuccBlock);
        CSuccBlock->Preds.insert(Map[Current]);
      } else {
        Map[Current]->Succs.insert(NULL);
      }
    }
  }
  Entry->Succs.insert(Map[Header]);
  Map[Header]->Preds.insert(Entry);
  Blocks.push_back(Exit);
  Blocks.push_back(Entry);

  // reduce
  if (TrackedStmts != NULL) {
    llvm::DominatorTreeBase<NaturalLoopBlock> PDT(true);
    PDT.recalculate(*const_cast<NaturalLoop*>(this));
    for (NaturalLoop::iterator BI = Blocks.begin(),
                               BE = Blocks.end();
                               BI != BE; ) {
      NaturalLoopBlock *Current = *BI;
      /* std::cerr << Current->getBlockID() << "\n"; */
      if (Current == Entry || Current == Exit ||
          Current == Map[Header] // don't reduce the header or we will end up with no loop at all
          ) {
        BI++;
        continue;
      }
      if (Current->begin() == Current->end() && Current->getTerminator().getStmt() == NULL) {
        // no stmts in this block
        for (NaturalLoopBlock::const_pred_iterator I = Current->pred_begin(),
                                                   E = Current->pred_end();
                                                   I != E; I++) {
          NaturalLoopBlock *Pred = *I;

          NaturalLoopBlock *PostDom = Current;
          do {
            PostDom = PDT.getNode(PostDom)->getIDom()->getBlock();
          } while (std::find(Blocks.begin(), Blocks.end(), PostDom) == Blocks.end());

          // redirect pointer Pred->Current to Pred->PostDom
          for (NaturalLoopBlock::const_succ_iterator SI = Pred->Succs.begin(),
                                                 SE = Pred->Succs.end();
                                                 SI != SE;) {
            if (*SI == Current) {
              SI = Pred->Succs.erase(SI);
            } else {
              SI++;
            }
          }
          Pred->Succs.insert(PostDom);
          PostDom->Preds.insert(Pred);
          /* std::cerr << "Redirecting (" << Pred->getBlockID() << "," << Current->getBlockID() << ") to " << PostDom->getBlockID() << "\n"; */
        }
        // for all successors: remove me as pred, add postdom as pred
        for (NaturalLoopBlock::const_succ_iterator I = Current->succ_begin(),
                                                   E = Current->succ_end();
                                                   I != E; I++) {
          NaturalLoopBlock *Succ = *I;
          for (NaturalLoopBlock::const_pred_iterator PI = Succ->Preds.begin(),
                                                 PE = Succ->Preds.end();
                                                 PI != PE;) {
            if (*PI == Current) {
              PI = Succ->Preds.erase(PI);
            } else {
              PI++;
            }
          }
        }
        BI = Blocks.erase(BI);
        /* BI = Blocks.erase(std::remove(Blocks.begin(), Blocks.end(), Current), Blocks.end()); */
        /* std::cerr << "Deleting " << Current->getBlockID() << "\n"; */
        delete Current;
      }
      else { BI++; }
    }
  }

  return;
}

typedef std::pair<const NaturalLoop*, const NaturalLoop*> NaturalLoopPair;

} // namespace sloopy

using namespace sloopy;

//===----------------------------------------------------------------------===//
// CFG pretty printing (CFG.cpp)
//===----------------------------------------------------------------------===//

namespace {

class StmtPrinterHelper : public PrinterHelper  {
  typedef llvm::DenseMap<const Stmt*,std::pair<unsigned,unsigned> > StmtMapTy;
  typedef llvm::DenseMap<const Decl*,std::pair<unsigned,unsigned> > DeclMapTy;
  StmtMapTy StmtMap;
  DeclMapTy DeclMap;
  signed currentBlock;
  unsigned currStmt;
  const LangOptions &LangOpts;
public:

  StmtPrinterHelper(const NaturalLoop* cfg, const LangOptions &LO)
    : currentBlock(0), currStmt(0), LangOpts(LO)
  {
    for (NaturalLoop::const_iterator I = cfg->begin(), E = cfg->end(); I != E; ++I ) {
      unsigned j = 1;
      for (NaturalLoopBlock::const_iterator BI = (*I)->begin(), BEnd = (*I)->end() ;
           BI != BEnd; ++BI, ++j ) {        
        if (true) {   // thpani
          const Stmt *stmt= *BI;    // thpani
          std::pair<unsigned, unsigned> P((*I)->getBlockID(), j);
          StmtMap[stmt] = P;

          switch (stmt->getStmtClass()) {
            case Stmt::DeclStmtClass:
                DeclMap[cast<DeclStmt>(stmt)->getSingleDecl()] = P;
                break;
            case Stmt::IfStmtClass: {
              const VarDecl *var = cast<IfStmt>(stmt)->getConditionVariable();
              if (var)
                DeclMap[var] = P;
              break;
            }
            case Stmt::ForStmtClass: {
              const VarDecl *var = cast<ForStmt>(stmt)->getConditionVariable();
              if (var)
                DeclMap[var] = P;
              break;
            }
            case Stmt::WhileStmtClass: {
              const VarDecl *var =
                cast<WhileStmt>(stmt)->getConditionVariable();
              if (var)
                DeclMap[var] = P;
              break;
            }
            case Stmt::SwitchStmtClass: {
              const VarDecl *var =
                cast<SwitchStmt>(stmt)->getConditionVariable();
              if (var)
                DeclMap[var] = P;
              break;
            }
            case Stmt::CXXCatchStmtClass: {
              const VarDecl *var =
                cast<CXXCatchStmt>(stmt)->getExceptionDecl();
              if (var)
                DeclMap[var] = P;
              break;
            }
            default:
              break;
          }
        }
      }
    }
  }
  

  virtual ~StmtPrinterHelper() {}

  const LangOptions &getLangOpts() const { return LangOpts; }
  void setBlockID(signed i) { currentBlock = i; }
  void setStmtID(unsigned i) { currStmt = i; }

  virtual bool handledStmt(Stmt *S, raw_ostream &OS) {
    StmtMapTy::iterator I = StmtMap.find(S);

    if (I == StmtMap.end())
      return false;

    if (currentBlock >= 0 && I->second.first == (unsigned) currentBlock
                          && I->second.second == currStmt) {
      return false;
    }

    OS << "[B" << I->second.first << "." << I->second.second << "]";
    return true;
  }

  bool handleDecl(const Decl *D, raw_ostream &OS) {
    DeclMapTy::iterator I = DeclMap.find(D);

    if (I == DeclMap.end())
      return false;

    if (currentBlock >= 0 && I->second.first == (unsigned) currentBlock
                          && I->second.second == currStmt) {
      return false;
    }

    OS << "[B" << I->second.first << "." << I->second.second << "]";
    return true;
  }
};

} // end anonymous namespace

namespace {
class CFGBlockTerminatorPrint
  : public StmtVisitor<CFGBlockTerminatorPrint,void> {

  raw_ostream &OS;
  StmtPrinterHelper* Helper;
  PrintingPolicy Policy;
public:
  CFGBlockTerminatorPrint(raw_ostream &os, StmtPrinterHelper* helper,
                          const PrintingPolicy &Policy)
    : OS(os), Helper(helper), Policy(Policy) {}

  void VisitIfStmt(IfStmt *I) {
    OS << "if ";
    I->getCond()->printPretty(OS,Helper,Policy);
  }

  // Default case.
  void VisitStmt(Stmt *Terminator) {
    Terminator->printPretty(OS, Helper, Policy);
  }

  void VisitDeclStmt(DeclStmt *DS) {
    VarDecl *VD = cast<VarDecl>(DS->getSingleDecl());
    OS << "static init " << VD->getName();
  }

  void VisitForStmt(ForStmt *F) {
    OS << "for (" ;
    if (F->getInit())
      OS << "...";
    OS << "; ";
    if (Stmt *C = F->getCond())
      C->printPretty(OS, Helper, Policy);
    OS << "; ";
    if (F->getInc())
      OS << "...";
    OS << ")";
  }

  void VisitWhileStmt(WhileStmt *W) {
    OS << "while " ;
    if (Stmt *C = W->getCond())
      C->printPretty(OS, Helper, Policy);
  }

  void VisitDoStmt(DoStmt *D) {
    OS << "do ... while ";
    if (Stmt *C = D->getCond())
      C->printPretty(OS, Helper, Policy);
  }

  void VisitSwitchStmt(SwitchStmt *Terminator) {
    OS << "switch ";
    Terminator->getCond()->printPretty(OS, Helper, Policy);
  }

  void VisitCXXTryStmt(CXXTryStmt *CS) {
    OS << "try ...";
  }

  void VisitAbstractConditionalOperator(AbstractConditionalOperator* C) {
    C->getCond()->printPretty(OS, Helper, Policy);
    OS << " ? ... : ...";
  }

  void VisitChooseExpr(ChooseExpr *C) {
    OS << "__builtin_choose_expr( ";
    C->getCond()->printPretty(OS, Helper, Policy);
    OS << " )";
  }

  void VisitIndirectGotoStmt(IndirectGotoStmt *I) {
    OS << "goto *";
    I->getTarget()->printPretty(OS, Helper, Policy);
  }

  void VisitBinaryOperator(BinaryOperator* B) {
    if (!B->isLogicalOp()) {
      VisitExpr(B);
      return;
    }

    B->getLHS()->printPretty(OS, Helper, Policy);

    switch (B->getOpcode()) {
      case BO_LOr:
        OS << " || ...";
        return;
      case BO_LAnd:
        OS << " && ...";
        return;
      default:
        llvm_unreachable("Invalid logical operator.");
    }
  }

  void VisitExpr(Expr *E) {
    E->printPretty(OS, Helper, Policy);
  }
};
} // end anonymous namespace


static StmtPrinterHelper* GraphHelper;

void NaturalLoop::view(const LangOptions &LO) const {
  const NaturalLoop *L = this;
  StmtPrinterHelper H(this, LO);
  GraphHelper = &H;
  llvm::ViewGraph(L, "NaturalLoop");
  GraphHelper = NULL;
}

void NaturalLoop::write(const LangOptions &LO) const {
  const NaturalLoop *G = this;
  StmtPrinterHelper H(this, LO);
  GraphHelper = &H;

  std::string ErrMsg;
  char Pathname[32];
  strcpy(Pathname, "./benchdot/NL_XXXXXX.dot");
  int TempFD;
  if ((TempFD = mkstemps(Pathname, 4)) == -1) {
    llvm::errs() << Pathname << ": can't make unique filename\n";
    return;
  }
  close(TempFD);
  
  llvm::errs() << "Writing '" << Pathname << "'... ";

  std::string ErrorInfo;
  llvm::raw_fd_ostream O(Pathname, ErrorInfo);

  if (ErrorInfo.empty()) {
    llvm::WriteGraph(O, G);
    llvm::errs() << " done. \n";
  } else {
    llvm::errs() << "error opening file '" << Pathname << "' for writing!\n";
  }

  GraphHelper = NULL;
}

static void print_elem(raw_ostream &OS, StmtPrinterHelper* Helper,
                       const Stmt * S) {
  if (Helper) {
    // special printing for statement-expressions.
    if (const StmtExpr *SE = dyn_cast<StmtExpr>(S)) {
      const CompoundStmt *Sub = SE->getSubStmt();

      if (Sub->children()) {
        OS << "({ ... ; ";
        Helper->handledStmt(*SE->getSubStmt()->body_rbegin(),OS);
        OS << " })\n";
        return;
      }
    }
    // special printing for comma expressions.
    if (const BinaryOperator* B = dyn_cast<BinaryOperator>(S)) {
      if (B->getOpcode() == BO_Comma) {
        OS << "... , ";
        Helper->handledStmt(B->getRHS(),OS);
        OS << '\n';
        return;
      }
    }
  }
  S->printPretty(OS, Helper, PrintingPolicy(Helper->getLangOpts()));

  if (isa<CXXOperatorCallExpr>(S)) {
    OS << " (OperatorCall)";
  }
  else if (isa<CXXBindTemporaryExpr>(S)) {
    OS << " (BindTemporary)";
  }
  else if (const CXXConstructExpr *CCE = dyn_cast<CXXConstructExpr>(S)) {
    OS << " (CXXConstructExpr, " << CCE->getType().getAsString() << ")";
  }
  else if (const CastExpr *CE = dyn_cast<CastExpr>(S)) {
    OS << " (" << CE->getStmtClassName() << ", "
        << CE->getCastKindName()
        << ", " << CE->getType().getAsString()
        << ")";
  }

  // Expressions need a newline.
  if (isa<Expr>(S))
    OS << '\n';
}

static void print_block(raw_ostream &OS, const NaturalLoop* cfg,
                        const NaturalLoopBlock &B,
                        StmtPrinterHelper* Helper, bool print_edges,
                        bool ShowColors) {

  if (Helper)
    Helper->setBlockID(B.getBlockID());

  // Print the header.
  if (ShowColors)
    OS.changeColor(raw_ostream::YELLOW, true);
  
  OS << "\n [B" << B.getBlockID();

  if (&B == &cfg->getEntry())
    OS << " (ENTRY)]\n";
  else if (&B == &cfg->getExit())
    OS << " (EXIT)]\n";
  /* else if (&B == cfg->getIndirectGotoBlock()) */
  /*   OS << " (INDIRECT GOTO DISPATCH)]\n"; */
  else
    OS << "]\n";
  
  if (ShowColors)
    OS.resetColor();

  // Print the label of this block.
  if (Stmt *Label = const_cast<Stmt*>(B.getLabel())) {

    if (print_edges)
      OS << "  ";

    if (LabelStmt *L = dyn_cast<LabelStmt>(Label))
      OS << L->getName();
    else if (CaseStmt *C = dyn_cast<CaseStmt>(Label)) {
      OS << "case ";
      C->getLHS()->printPretty(OS, Helper,
                               PrintingPolicy(Helper->getLangOpts()));
      if (C->getRHS()) {
        OS << " ... ";
        C->getRHS()->printPretty(OS, Helper,
                                 PrintingPolicy(Helper->getLangOpts()));
      }
    } else if (isa<DefaultStmt>(Label))
      OS << "default";
    else if (CXXCatchStmt *CS = dyn_cast<CXXCatchStmt>(Label)) {
      OS << "catch (";
      if (CS->getExceptionDecl())
        CS->getExceptionDecl()->print(OS, PrintingPolicy(Helper->getLangOpts()),
                                      0);
      else
        OS << "...";
      OS << ")";

    } else
      llvm_unreachable("Invalid label statement in CFGBlock.");

    OS << ":\n";
  }

  // Iterate through the statements in the block and print them.
  unsigned j = 1;

  for (NaturalLoopBlock::const_iterator I = B.begin(), E = B.end() ;
       I != E ; ++I, ++j ) {

    // Print the statement # in the basic block and the statement itself.
    if (print_edges)
      OS << " ";

    OS << llvm::format("%3d", j) << ": ";

    if (Helper)
      Helper->setStmtID(j);

    print_elem(OS, Helper, *I);
  }

  // Print the terminator of this block.
  if (B.getTerminator()) {
    if (ShowColors)
      OS.changeColor(raw_ostream::GREEN);

    OS << "   T: ";

    if (Helper) Helper->setBlockID(-1);

    PrintingPolicy PP(Helper ? Helper->getLangOpts() : LangOptions());
    CFGBlockTerminatorPrint TPrinter(OS, Helper, PP);
    TPrinter.Visit(const_cast<Stmt*>(B.getTerminator().getStmt()));
    OS << '\n';
    
    if (ShowColors)
      OS.resetColor();
  }

  if (print_edges) {
    // Print the predecessors of this block.
    if (!B.pred_empty()) {
      const raw_ostream::Colors Color = raw_ostream::BLUE;
      if (ShowColors)
        OS.changeColor(Color);
      OS << "   Preds " ;
      if (ShowColors)
        OS.resetColor();
      OS << '(' << B.pred_size() << "):";
      unsigned i = 0;

      if (ShowColors)
        OS.changeColor(Color);
      
      for (NaturalLoopBlock::const_pred_iterator I = B.pred_begin(), E = B.pred_end();
           I != E; ++I, ++i) {

        if (i % 10 == 8)
          OS << "\n     ";

        OS << " B" << (*I)->getBlockID();
      }
      
      if (ShowColors)
        OS.resetColor();

      OS << '\n';
    }

    // Print the successors of this block.
    if (!B.succ_empty()) {
      const raw_ostream::Colors Color = raw_ostream::MAGENTA;
      if (ShowColors)
        OS.changeColor(Color);
      OS << "   Succs ";
      if (ShowColors)
        OS.resetColor();
      OS << '(' << B.succ_size() << "):";
      unsigned i = 0;

      if (ShowColors)
        OS.changeColor(Color);

      for (NaturalLoopBlock::const_succ_iterator I = B.succ_begin(), E = B.succ_end();
           I != E; ++I, ++i) {

        if (i % 10 == 8)
          OS << "\n    ";

        if (*I)
          OS << " B" << (*I)->getBlockID();
        else
          OS  << " NULL";
      }
      
      if (ShowColors)
        OS.resetColor();
      OS << '\n';
    }
  }
}

//===----------------------------------------------------------------------===//
// GraphTraits specializations for NaturalLoop basic block graphs
//===----------------------------------------------------------------------===//

namespace llvm {

// Traits for: NaturalLoopBlock

template <> struct GraphTraits< ::sloopy::NaturalLoopBlock *> {
  typedef ::sloopy::NaturalLoopBlock NodeType;
  typedef ::sloopy::NaturalLoopBlock::succ_iterator ChildIteratorType;

  static NodeType* getEntryNode(::sloopy::NaturalLoopBlock *BB)
  { return BB; }

  static inline ChildIteratorType child_begin(NodeType* N)
  { return N->succ_begin(); }

  static inline ChildIteratorType child_end(NodeType* N)
  { return N->succ_end(); }
};

template <> struct GraphTraits< const ::sloopy::NaturalLoopBlock *> {
  typedef const ::sloopy::NaturalLoopBlock NodeType;
  typedef ::sloopy::NaturalLoopBlock::const_succ_iterator ChildIteratorType;

  static NodeType* getEntryNode(const sloopy::NaturalLoopBlock *BB)
  { return BB; }

  static inline ChildIteratorType child_begin(NodeType* N)
  { return N->succ_begin(); }

  static inline ChildIteratorType child_end(NodeType* N)
  { return N->succ_end(); }
};

template <> struct GraphTraits<Inverse< ::sloopy::NaturalLoopBlock*> > {
  typedef ::sloopy::NaturalLoopBlock NodeType;
  typedef ::sloopy::NaturalLoopBlock::pred_iterator ChildIteratorType;

  static NodeType *getEntryNode(Inverse< ::sloopy::NaturalLoopBlock*> G)
  { return G.Graph; }

  static inline ChildIteratorType child_begin(NodeType* N)
  { return N->pred_begin(); }

  static inline ChildIteratorType child_end(NodeType* N)
  { return N->pred_end(); }
};

template <> struct GraphTraits<Inverse<const ::sloopy::NaturalLoopBlock*> > {
  typedef const ::sloopy::NaturalLoopBlock NodeType;
  typedef ::sloopy::NaturalLoopBlock::const_pred_iterator ChildIteratorType;

  static NodeType *getEntryNode(Inverse<const ::sloopy::NaturalLoopBlock*> G)
  { return G.Graph; }

  static inline ChildIteratorType child_begin(NodeType* N)
  { return N->pred_begin(); }

  static inline ChildIteratorType child_end(NodeType* N)
  { return N->pred_end(); }
};

// Traits for: NaturalLoop

template <> struct GraphTraits< ::sloopy::NaturalLoop* >
    : public GraphTraits< ::sloopy::NaturalLoopBlock *>  {

  typedef ::sloopy::NaturalLoop::graph_iterator nodes_iterator;

  static NodeType     *getEntryNode(::sloopy::NaturalLoop* F) { return &F->getEntry(); }
  static nodes_iterator nodes_begin(::sloopy::NaturalLoop* F) { return F->nodes_begin();}
  static nodes_iterator   nodes_end(::sloopy::NaturalLoop* F) { return F->nodes_end(); }
  static unsigned              size(::sloopy::NaturalLoop* F) { return F->size(); }
};

template <> struct GraphTraits<const ::sloopy::NaturalLoop* >
    : public GraphTraits<const ::sloopy::NaturalLoopBlock *>  {

  typedef ::sloopy::NaturalLoop::const_graph_iterator nodes_iterator;

  static NodeType *getEntryNode( const ::sloopy::NaturalLoop* F) { return &F->getEntry(); }
  static nodes_iterator nodes_begin( const ::sloopy::NaturalLoop* F) { return F->nodes_begin(); }
  static nodes_iterator nodes_end( const ::sloopy::NaturalLoop* F) { return F->nodes_end(); }
  static unsigned size(const ::sloopy::NaturalLoop* F) { return F->size(); }
};

template <> struct GraphTraits<Inverse< ::sloopy::NaturalLoop*> >
  : public GraphTraits<Inverse< ::sloopy::NaturalLoopBlock*> > {

  typedef ::sloopy::NaturalLoop::graph_iterator nodes_iterator;

  static NodeType *getEntryNode( ::sloopy::NaturalLoop* F) { return &F->getExit(); }
  static nodes_iterator nodes_begin( ::sloopy::NaturalLoop* F) {return F->nodes_begin();}
  static nodes_iterator nodes_end( ::sloopy::NaturalLoop* F) { return F->nodes_end(); }
};

template <> struct GraphTraits<Inverse<const ::sloopy::NaturalLoop*> >
  : public GraphTraits<Inverse<const ::sloopy::NaturalLoopBlock*> > {

  typedef ::sloopy::NaturalLoop::const_graph_iterator nodes_iterator;

  static NodeType *getEntryNode(const ::sloopy::NaturalLoop* F) { return &F->getExit(); }
  static nodes_iterator nodes_begin(const ::sloopy::NaturalLoop* F) {
    return F->nodes_begin();
  }
  static nodes_iterator nodes_end(const ::sloopy::NaturalLoop* F) {
    return F->nodes_end();
  }
};

template<>
struct DOTGraphTraits<const sloopy::NaturalLoop*> : public DefaultDOTGraphTraits {

  DOTGraphTraits (bool isSimple=false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getNodeLabel(const NaturalLoopBlock *Node, const NaturalLoop* Graph) {
    std::string OutSStr;
    llvm::raw_string_ostream Out(OutSStr);
    print_block(Out, Graph, *Node, GraphHelper, false, false);
    std::string& OutStr = Out.str();

    if (OutStr[0] == '\n') OutStr.erase(OutStr.begin());

    // Process string output to make it nicer...
    for (unsigned i = 0; i != OutStr.length(); ++i)
      if (OutStr[i] == '\n') {                            // Left justify
        OutStr[i] = '\\';
        OutStr.insert(OutStr.begin()+i+1, 'l');
      }

    return OutStr;
  }
};

} // end llvm namespace
