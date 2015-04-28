//===--- FormatSpec.h - Fortran Format Specifier ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the format specifier class, used by the PRINT statement,
//  et al.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_FORMATSPEC_H__
#define FLANG_AST_FORMATSPEC_H__

#include "flang/Basic/SourceLocation.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/LLVM.h"

// FIXME: add dumping using ASTDumper

namespace flang {

class ASTContext;

/// Format Spec - this class represents a format specifier.
class FormatSpec {
protected:
  enum FormatType { FS_CharExpr, FS_Label, FS_Star, FS_VarLabel };
private:
  FormatType ID;
  SourceLocation Loc;
protected:
  FormatSpec(FormatType id, SourceLocation L)
    : ID(id), Loc(L) {}
  friend class ASTContext;
public:
  SourceLocation getLocation() const { return Loc; }

  FormatType getFormatSpecID() const { return ID; }
  static bool classof(const FormatSpec *) { return true; }
};

/// StarFormatSpec - represents a '*' format specifier.
class StarFormatSpec : public FormatSpec {
  StarFormatSpec(SourceLocation Loc);
public:
  static StarFormatSpec *Create(ASTContext &C, SourceLocation Loc);

  static bool classof(const StarFormatSpec *) { return true; }
  static bool classof(const FormatSpec *F) {
    return F->getFormatSpecID() == FS_Star;
  }
};

/// CharacterExprFormatSpec - represents a character expression
/// format specifier.
class CharacterExpFormatSpec : public FormatSpec {
  Expr *Fmt;
  CharacterExpFormatSpec(SourceLocation Loc, Expr *Fmt);
public:
  static CharacterExpFormatSpec *Create(ASTContext &C, SourceLocation Loc,
                                       Expr *Fmt);

  Expr *getFormat() const { return Fmt; }

  static bool classof(const CharacterExpFormatSpec *) { return true; }
  static bool classof(const FormatSpec *F) {
    return F->getFormatSpecID() == FS_CharExpr;
  }
};

/// LabelFormatSpec - represents a statement label format specifier.
class LabelFormatSpec : public FormatSpec {
  StmtLabelReference StmtLabel;
  LabelFormatSpec(SourceLocation Loc, StmtLabelReference Label);
public:
  static LabelFormatSpec *Create(ASTContext &C, SourceLocation Loc,
                                 StmtLabelReference Label);

  void setLabel(StmtLabelReference Label);
  StmtLabelReference getLabel() const { return StmtLabel; }

  static bool classof(const LabelFormatSpec *) { return true; }
  static bool classof(const FormatSpec *F) {
    return F->getFormatSpecID() == FS_Label;
  }
};

/// VarLabelFormatSpec - represents an integer variable format specifier,
/// where the variable holds the value of the statement label which
/// points to the relevant format statement.
class VarLabelFormatSpec : public FormatSpec {
  VarExpr *Var;
  VarLabelFormatSpec(SourceLocation Loc, VarExpr *VarRef);
public:
  static VarLabelFormatSpec *Create(ASTContext &C, SourceLocation Loc,
                                    VarExpr *Var);

  VarExpr *getVar() const { return Var; }

  static bool classof(const VarLabelFormatSpec *) { return true; }
  static bool classof(const FormatSpec *F) {
    return F->getFormatSpecID() == FS_VarLabel;
  }
};

} // end namespace flang

#endif
