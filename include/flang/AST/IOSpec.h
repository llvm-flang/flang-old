//===--- IOSpec.h - Fortran IO Specifiers -------00000---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the various IO specifier classes, such as Unit, etc
//  which are used by IO statements.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_IOSPEC_H__
#define FLANG_AST_IOSPEC_H__

#include "flang/Basic/SourceLocation.h"
#include "flang/AST/Expr.h"
#include "flang/Basic/LLVM.h"

namespace flang {

class ASTContext;

class UnitSpec {
public:
  enum UnitType { US_ExternalInt, US_ExternalStar, US_Internal };
private:
  UnitType ID;
  SourceLocation Loc;
  /// is UNIT = used?
  bool Labeled;
protected:
  UnitSpec(UnitType id, SourceLocation L,
           bool IsLabeled)
    : ID(id), Loc(L), Labeled(IsLabeled) {}
  friend class ASTContext;
public:
  SourceLocation getLocation() const { return Loc; }

  UnitType getUnitSpecID() const { return ID; }

  bool IsLabeled() const { return Labeled; }

  static bool classof(const UnitSpec *) { return true; }
};

class ExternalStarUnitSpec : public UnitSpec {
  ExternalStarUnitSpec(SourceLocation Loc, bool IsLabeled);
public:
  static ExternalStarUnitSpec *Create(ASTContext &Context, SourceLocation Loc,
                                      bool IsLabeled);

  static bool classof(const UnitSpec *F) {
    return F->getUnitSpecID() == US_ExternalStar;
  }
};

class ExternalIntegerUnitSpec : public UnitSpec {
  Expr *E;
  ExternalIntegerUnitSpec(SourceLocation Loc, Expr *Value, bool IsLabeled);
public:
  static ExternalIntegerUnitSpec *Create(ASTContext &C, SourceLocation Loc,
                                         Expr *Value, bool IsLabeled);

  Expr *getValue() const { return E; }

  static bool classof(const UnitSpec *F) {
    return F->getUnitSpecID() == US_ExternalInt;
  }
};

class InternalUnitSpec : public UnitSpec {
  Expr *E;
  InternalUnitSpec(SourceLocation Loc, Expr *Value, bool IsLabeled);
public:
  static InternalUnitSpec *Create(ASTContext &C, SourceLocation Loc,
                                  Expr *Value, bool IsLabeled);

  Expr *getValue() const { return E; }

  static bool classof(const UnitSpec *F) {
    return F->getUnitSpecID() == US_Internal;
  }
};

} // end namespace flang

#endif
