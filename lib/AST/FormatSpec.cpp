//===--- FormatSpec.cpp - Fortran FormatSpecifier -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/FormatSpec.h"
#include "flang/AST/ASTContext.h"

namespace flang {

StarFormatSpec::StarFormatSpec(SourceLocation Loc)
  : FormatSpec(FormatSpec::FS_Star, Loc) {}

StarFormatSpec *StarFormatSpec::Create(ASTContext &C, SourceLocation Loc) {
  return new (C) StarFormatSpec(Loc);
}

CharacterExpFormatSpec::CharacterExpFormatSpec(SourceLocation Loc, Expr *F)
  : FormatSpec(FormatSpec::FS_CharExpr, Loc), Fmt(F) {}

CharacterExpFormatSpec *CharacterExpFormatSpec::Create(ASTContext &C, SourceLocation Loc,
                                                   Expr *Fmt) {
  return new (C) CharacterExpFormatSpec(Loc, Fmt);
}

LabelFormatSpec::LabelFormatSpec(SourceLocation Loc, StmtLabelReference Label)
  : FormatSpec(FormatSpec::FS_Label, Loc), StmtLabel(Label) {}

LabelFormatSpec *LabelFormatSpec::Create(ASTContext &C, SourceLocation Loc,
                                         StmtLabelReference Label) {
  return new (C) LabelFormatSpec(Loc, Label);
}

void LabelFormatSpec::setLabel(StmtLabelReference Label) {
  assert(!StmtLabel.Statement);
  assert(Label.Statement);
  StmtLabel = Label;
}

VarLabelFormatSpec::VarLabelFormatSpec(SourceLocation Loc, VarExpr *VarRef)
  : FormatSpec(FormatSpec::FS_VarLabel, Loc), Var(VarRef) {}

VarLabelFormatSpec *VarLabelFormatSpec::Create(ASTContext &C, SourceLocation Loc,
                                               VarExpr *Var) {
  return new (C) VarLabelFormatSpec(Loc, Var);
}

} //namespace flang
