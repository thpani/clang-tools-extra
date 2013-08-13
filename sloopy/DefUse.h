#pragma once

#include "clang/AST/StmtVisitor.h"

using namespace clang;

namespace sloopy {

class DefUseHelper : public ConstStmtVisitor<DefUseHelper> {
  private:
    enum UseKind {
      Use, Def, UseDef
    };
    bool AnalysisRun;
    UseKind current_use;
    std::set<const VarDecl*> Uses, Defs, DefsAndUses;
    std::stack<const Stmt*> Edits;
    std::map<const VarDecl*, std::set<const Stmt*>> DefStmts;
    const Stmt *Stmt;

    void RunAnalysis() {
      if (AnalysisRun) return;
      Edits.push(Stmt);
      Visit(Stmt);
      Edits.pop();
      AnalysisRun = true;
    }

  public:
    DefUseHelper(const class Stmt *Stmt) : AnalysisRun(false), current_use(Use), Stmt(Stmt) {}

    bool isDef(const VarDecl *VD) {
      RunAnalysis();
      return std::find(Defs.begin(), Defs.end(), VD) != Defs.end();
    }
    unsigned countDefs(const VarDecl *VD) {
      RunAnalysis();
      return DefStmts[VD].size();
    }
    bool isDefExclDecl(const VarDecl *VD) {
      auto I = DefStmts.find(VD);
      if (I != DefStmts.end()) {
        for (const class Stmt *S : I->second) {
          if (!isa<DeclStmt>(S)) {
            return true;
          }
        }
      }
      return false;
    }
    bool isUse(const VarDecl *VD) {
      RunAnalysis();
      return std::find(Uses.begin(), Uses.end(), VD) != Uses.end();
    }
    std::set<const VarDecl*> getDefs() {
      RunAnalysis();
      return Defs;
    }
    std::set<const VarDecl*> getUses() {
      RunAnalysis();
      return Uses;
    }
    std::set<const VarDecl*> getDefsAndUses() {
      RunAnalysis();
      if (DefsAndUses.size() != Uses.size() + Defs.size()) {
        for (auto VD : Defs)
          DefsAndUses.insert(VD);
        for (auto VD : Uses)
          DefsAndUses.insert(VD);
      }
      return DefsAndUses;
    }
    std::set<const class Stmt*> getDefiningStmts(const VarDecl *VD) {
      if (DefStmts.find(VD) == DefStmts.end()) {
        return std::set<const class Stmt*>();
      }
      return DefStmts.find(VD)->second;
    }

    void HandleDeclRefExpr(const DeclRefExpr *DR);
    void HandleDeclStmt(const DeclStmt *DS);
    void HandleBinaryOperator(const BinaryOperator* B);
    void HandleConditionalOperator(const ConditionalOperator* C);
    void HandleCallExpr(const CallExpr* C);
    void HandleUnaryOperator(const UnaryOperator* U);
    void HandleArraySubscriptExpr(const ArraySubscriptExpr* AS);

    void VisitDeclRefExpr(const DeclRefExpr *DR) {
      HandleDeclRefExpr(DR);
    }
    void VisitDeclStmt(const DeclStmt *DS) {
      HandleDeclStmt(DS);
    }
    void VisitBinaryOperator(const BinaryOperator* B) {
      HandleBinaryOperator(B);
    }
    void VisitConditionalOperator(const ConditionalOperator* C) {
      HandleConditionalOperator(C);
    }
    void VisitCallExpr(const CallExpr* C) {
      HandleCallExpr(C);
    }
    void VisitUnaryOperator(const UnaryOperator* U) {
      HandleUnaryOperator(U);
    }
    void VisitArraySubscriptExpr(const ArraySubscriptExpr* AS) {
      HandleArraySubscriptExpr(AS);
    }
    void VisitStmt(const class Stmt* S) {
      for (Stmt::const_child_iterator I = S->child_begin(),
                                      E = S->child_end();
                                      I != E; ++I) {
        if (*I) {
          Visit(*I);
        }
      }
    }
};

} // end namespace sloopy

void sloopy::DefUseHelper::HandleDeclRefExpr(const DeclRefExpr *DR) {
  if (const VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl())) {
    switch (current_use) {
      case Use:
        Uses.insert(VD);
        break;
      case Def:
        Defs.insert(VD);
        assert(Edits.size()>0);
        DefStmts[VD].insert(Edits.top());
        break;
      case UseDef:
        Uses.insert(VD);
        Defs.insert(VD);
        assert(Edits.size()>0);
        DefStmts[VD].insert(Edits.top());
        break;
    }
  }
}

void sloopy::DefUseHelper::HandleDeclStmt(const DeclStmt *DS) {
  for (DeclStmt::const_decl_iterator I = DS->decl_begin(),
                               E = DS->decl_end();
                               I != E; I++) {
    if (const VarDecl* VD = dyn_cast<VarDecl> (*I)) {
      Defs.insert(VD);
      assert(Edits.size()>0);
      DefStmts[VD].insert(Edits.top());
    }
  }
}

void sloopy::DefUseHelper::HandleBinaryOperator(const BinaryOperator* B) {
  UseKind backup = current_use; // backup the usage
  if (B->isAssignmentOp()) {
    current_use = Use;
    Visit(B->getRHS());
    current_use = B->isCompoundAssignmentOp() ? UseDef : Def;
    if (current_use != Use) Edits.push(B);
    Visit(B->getLHS());
    Edits.pop();
  } else {
    if (current_use != Use) {
      Edits.push(B);
      Visit(B->getLHS());
      Edits.pop();
    } else {
      Visit(B->getLHS());
    }
    current_use = Use;
    Visit(B->getRHS());
  }
  current_use = backup; // write back the usage to the current usage
}

void sloopy::DefUseHelper::HandleConditionalOperator(const ConditionalOperator* C) {}

void sloopy::DefUseHelper::HandleCallExpr(const CallExpr* C) {
  UseKind backup = current_use; // backup the usage
  for (CallExpr::const_arg_iterator I = C->arg_begin(),
                                    E = C->arg_end();
                                    I != E; ++I) {
    /* if ((*I)->getType()->isPointerType() || (*I)->getType()->isReferenceType()) */
    /*   current_use = Def; */
    if (current_use != Use) {
      Edits.push(C);
      Visit(*I);
      Edits.pop();
    } else {
      Visit(*I);
    }
    current_use = backup;
  }
}

void sloopy::DefUseHelper::HandleUnaryOperator(const UnaryOperator* U) {
  UseKind backup = current_use; // backup the usage
  switch (U->getOpcode()) {
  case UO_PostInc:
  case UO_PostDec:
  case UO_PreInc:
  case UO_PreDec:
    current_use = UseDef;
    break;
  case UO_Plus:
  case UO_Minus:
  case UO_Not:
  case UO_LNot:
    current_use = Use;
    break;
  case UO_AddrOf:
  case UO_Deref:
    // use the current_use
    break;
  default:
    // DEBUG("Operator " << UnaryOperator::getOpcodeStr(U->getOpcode()) <<
    // " not supported in def-use analysis");
    break;
  }
  if (current_use != Use) {
    Edits.push(U);
    Visit(U->getSubExpr());
    Edits.pop();
  } else {
    Visit(U->getSubExpr());
  }
  current_use = backup; // write back the usage to the current usage
}

void sloopy::DefUseHelper::HandleArraySubscriptExpr(const ArraySubscriptExpr* AS) {
  UseKind backup = current_use; // backup the usage
  if (current_use != Use) {
    Edits.push(AS);
    Visit(AS->getBase());
    Edits.pop();
  } else {
    Visit(AS->getBase());
  }
  current_use = Use;
  Visit(AS->getIdx());
  current_use = backup; // write back the usage to the current usage
}
