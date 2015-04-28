//===- SemaFormat.cpp - FORMAT AST Builder and Semantic Analysis Implementation -===//
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
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/FormatItem.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

static void CheckPositive(DiagnosticsEngine &Diags, IntegerConstantExpr *E) {
  if(!E) return;
  if(E->getValue().isNegative()) {
    Diags.Report(E->getLocation(), diag::err_expected_positive_integer)
     << E->getSourceRange();
  }
}
static void CheckPositiveOrZero(DiagnosticsEngine &Diags, IntegerConstantExpr *E) {
  if(!E) return;
  if(E->getValue().isNegative() || E->getValue().getLimitedValue() == 0) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_gt_0)
      << E->getSourceRange();
  }
}

StmtResult Sema::ActOnFORMAT(ASTContext &C, SourceLocation Loc,
                             FormatItemResult Items,
                             FormatItemResult UnlimitedItems,
                             Expr *StmtLabel,
                             bool IsInline) {
  auto ItemList = cast<FormatItemList>(Items.take());
  auto UnlimitedItemList = UnlimitedItems.isUsable()?
                             cast<FormatItemList>(UnlimitedItems.take()):
                             nullptr;
  if(!IsInline && !StmtLabel)
    Diags.Report(Loc, diag::err_format_without_stmt_label);
  auto Result = FormatStmt::Create(C, Loc, ItemList, UnlimitedItemList,
                                   StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

FormatItemResult Sema::ActOnFORMATIntegerDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                      tok::TokenKind Kind,
                                                      IntegerConstantExpr *RepeatCount,
                                                      IntegerConstantExpr *W,
                                                      IntegerConstantExpr *M) {
  CheckPositive(Diags, RepeatCount);
  CheckPositiveOrZero(Diags, W);
  return IntegerDataEditDesc::Create(C, Loc, Kind, RepeatCount, W, M);
}

FormatItemResult Sema::ActOnFORMATRealDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                   tok::TokenKind Kind,
                                                   IntegerConstantExpr *RepeatCount,
                                                   IntegerConstantExpr *W,
                                                   IntegerConstantExpr *D,
                                                   IntegerConstantExpr *E) {
  CheckPositive(Diags, RepeatCount);
  if(Kind == tok::fs_F || Kind == tok::fs_G)
    CheckPositiveOrZero(Diags, W);
  else CheckPositive(Diags, W);
  CheckPositive(Diags, E);
  // FIXME: add G constraints.
  return RealDataEditDesc::Create(C, Loc, Kind, RepeatCount, W, D, E);
}

FormatItemResult Sema::ActOnFORMATLogicalDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                      tok::TokenKind Kind,
                                                      IntegerConstantExpr *RepeatCount,
                                                      IntegerConstantExpr *W) {
  CheckPositive(Diags, RepeatCount);
  CheckPositive(Diags, W);
  return LogicalDataEditDesc::Create(C, Loc, Kind, RepeatCount, W);
}

FormatItemResult Sema::ActOnFORMATCharacterDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                        tok::TokenKind Kind,
                                                        IntegerConstantExpr *RepeatCount,
                                                        IntegerConstantExpr *W) {
  CheckPositive(Diags, RepeatCount);
  CheckPositive(Diags, W);
  return CharacterDataEditDesc::Create(C, Loc, Kind, RepeatCount, W);
}

FormatItemResult Sema::ActOnFORMATPositionEditDesc(ASTContext &C, SourceLocation Loc,
                                                   tok::TokenKind Kind,
                                                   IntegerConstantExpr *N) {
  CheckPositive(Diags, N);
  return PositionEditDesc::Create(C, Loc, Kind, N);
}

FormatItemResult Sema::ActOnFORMATControlEditDesc(ASTContext &C, SourceLocation Loc,
                                                  tok::TokenKind Kind) {
  return FormatItemResult(true);
}

FormatItemResult Sema::ActOnFORMATCharacterStringDesc(ASTContext &C, SourceLocation Loc,
                                                      ExprResult E) {
  auto Str = cast<CharacterConstantExpr>(E.take());
  return CharacterStringEditDesc::Create(C, Str);
}

FormatItemResult Sema::ActOnFORMATFormatItemList(ASTContext &C, SourceLocation Loc,
                                                 IntegerConstantExpr *RepeatCount,
                                                 ArrayRef<FormatItem*> Items) {
  CheckPositive(Diags, RepeatCount);
  return FormatItemList::Create(C, Loc, RepeatCount, Items);
}

} // end namespace flang
