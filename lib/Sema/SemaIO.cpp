//===- SemaIO.cpp - IO AST Builder and Semantic Analysis Implementation --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/Sema/SemaInternal.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/FormatSpec.h"
#include "flang/AST/IOSpec.h"
#include "flang/Basic/Diagnostic.h"

namespace flang {

StarFormatSpec *Sema::ActOnStarFormatSpec(ASTContext &C, SourceLocation Loc) {
  return StarFormatSpec::Create(C, Loc);
}

static void CheckStmtLabelIsFormat(DiagnosticsEngine &Diags, Stmt *S, Expr *Label) {
  if(!isa<FormatStmt>(S)) {
    Diags.Report(Label->getLocation(), diag::err_fmt_spec_stmt_label_not_format)
      << Label->getSourceRange();
    Diags.Report(S->getStmtLabel()->getLocation(), diag::note_stmt_label_declared_at)
      << S->getStmtLabel()->getSourceRange();
  }
}

void StmtLabelResolver::VisitLabelFormatSpec(LabelFormatSpec *FS) {
  CheckStmtLabelIsFormat(Diags, StmtLabelDecl, Info.StmtLabel);
  FS->setLabel(StmtLabelReference(StmtLabelDecl));
}

LabelFormatSpec *Sema::ActOnLabelFormatSpec(ASTContext &C, SourceLocation Loc,
                                            ExprResult Label) {
  if(isa<IntegerConstantExpr>(Label.get())) {
    auto Decl = getCurrentStmtLabelScope()->Resolve(Label.get());
    if(!Decl) {
      auto Result = LabelFormatSpec::Create(C, Loc, StmtLabelReference());
      getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Label.get(), Result));
      return Result;
    } else {
      CheckStmtLabelIsFormat(Diags, Decl, Label.get());
      return LabelFormatSpec::Create(C, Loc, StmtLabelReference(Decl));
    }
  }

  // FIXME: TODO.
  return LabelFormatSpec::Create(C, Loc, StmtLabelReference());
}

FormatSpec *Sema::ActOnExpressionFormatSpec(ASTContext &C, SourceLocation Loc,
                                            Expr *E) {
  auto Type = E->getType();
  if(Type->isCharacterType())
    return CharacterExpFormatSpec::Create(C, Loc, E);
  if(auto Var = dyn_cast<VarExpr>(E)) {
    if(Type->isIntegerType())
      return VarLabelFormatSpec::Create(C, Loc, Var);
  }
  Diags.Report(Loc, diag::err_typecheck_expected_format_spec)
    << Type << E->getSourceRange();
  // Return an empty character literal spec when an error occurs.
  return CharacterExpFormatSpec::Create(C, Loc,
                                        CharacterConstantExpr::Create(Context, Loc, "", C.CharacterTy));
}

ExternalStarUnitSpec *Sema::ActOnStarUnitSpec(ASTContext &C, SourceLocation Loc,
                                              bool IsLabeled) {
  return ExternalStarUnitSpec::Create(C, Loc, IsLabeled);
}

UnitSpec *Sema::ActOnUnitSpec(ASTContext &C, ExprResult Value, SourceLocation Loc,
                              bool IsLabeled) {
  // FIXME: TODO
  return nullptr;
}

StmtResult Sema::ActOnPrintStmt(ASTContext &C, SourceLocation Loc, FormatSpec *FS,
                                ArrayRef<ExprResult> OutputItemList,
                                Expr *StmtLabel) {
  SmallVector<Expr *, 8> OutputList;
  for(auto I : OutputItemList) OutputList.push_back(I.take());

  auto Result = PrintStmt::Create(C, Loc, FS, OutputList, StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnWriteStmt(ASTContext &C, SourceLocation Loc,
                                UnitSpec *US, FormatSpec *FS,
                                ArrayRef<ExprResult> OutputItemList,
                                Expr *StmtLabel) {
  // FIXME: TODO
  SmallVector<Expr *, 8> OutputList;
  for(auto I : OutputItemList) OutputList.push_back(I.take());

  auto Result = WriteStmt::Create(C, Loc, US, FS, OutputList, StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

} // end namespace flang
