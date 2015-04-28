//===--- IOSpec.cpp - Fortran IO Specifier -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/IOSpec.h"
#include "flang/AST/ASTContext.h"

namespace flang {

ExternalStarUnitSpec::ExternalStarUnitSpec(SourceLocation Loc, bool IsLabeled)
  : UnitSpec(US_ExternalStar, Loc, IsLabeled) {}

ExternalStarUnitSpec *ExternalStarUnitSpec::Create(ASTContext &C, SourceLocation Loc,
                                                   bool IsLabeled) {
  return new(C) ExternalStarUnitSpec(Loc, IsLabeled);
}

ExternalIntegerUnitSpec::ExternalIntegerUnitSpec(SourceLocation Loc,
                                                 Expr *Value, bool IsLabeled)
  : UnitSpec(US_ExternalInt, Loc, IsLabeled), E(Value) {}

ExternalIntegerUnitSpec *ExternalIntegerUnitSpec::Create(ASTContext &C, SourceLocation Loc,
                                                         Expr *Value, bool IsLabeled) {
  return new(C) ExternalIntegerUnitSpec(Loc, Value, IsLabeled);
}

InternalUnitSpec::InternalUnitSpec(SourceLocation Loc, Expr *Value, bool IsLabeled)
  : UnitSpec(US_Internal, Loc, IsLabeled), E(Value) {}

InternalUnitSpec *InternalUnitSpec::Create(ASTContext &C, SourceLocation Loc,
                                           Expr *Value, bool IsLabeled) {
  return new(C) InternalUnitSpec(Loc, Value, IsLabeled);
}

} // end namespace flang
