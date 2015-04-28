//===-- DeclSpec.cpp - Fortran Declaration Type Specifier Interface ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran declaration type specifier interface.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/DeclSpec.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Type.h"
#include "llvm/ADT/ArrayRef.h"

namespace flang {

DeclSpec::~DeclSpec() {}

const char *DeclSpec::getSpecifierName(DeclSpec::TST I) {
  switch (I) {
  case TST_unspecified:     return "unspecified";
  case TST_integer:         return "INTEGER";
  case TST_real:            return "REAL";
  case TST_complex:         return "COMPLEX";
  case TST_character:       return "CHARACTER";
  case TST_logical:         return "LOGICAL";
  case TST_struct:          return "TYPE";
  }
  llvm_unreachable("Unknown typespec!");
}

const char *DeclSpec::getSpecifierName(DeclSpec::AS A) {
  switch (A) {
  case AS_unspecified:  return "unspecified";
  case AS_allocatable:  return "allocate";
  case AS_asynchronous: return "asynchronous";
  case AS_codimension:  return "codimension";
  case AS_contiguous:   return "contiguous";
  case AS_dimension:    return "dimension";
  case AS_external:     return "external";
  case AS_intrinsic:    return "intrinsic";
  case AS_optional:     return "optinonal";
  case AS_parameter:    return "parameter";
  case AS_pointer:      return "pointer";
  case AS_protected:    return "protected";
  case AS_save:         return "save";
  case AS_target:       return "target";
  case AS_value:        return "value";
  case AS_volatile:     return "volatile";
  }
  llvm_unreachable("Unknown typespec!");
}

const char *DeclSpec::getSpecifierName(DeclSpec::IS I) {
  switch (I) {
  case IS_unspecified: return "unspecified";
  case IS_in:          return "in";
  case IS_out:         return "out";
  case IS_inout:       return "inout";
  }
  llvm_unreachable("Unknown typespec!");
}

const char *DeclSpec::getSpecifierName(DeclSpec::AC I) {
  switch (I) {
  case AC_unspecified: return "unspecified";
  case AC_public:      return "public";
  case AC_private:     return "private";
  }
  llvm_unreachable("Unknown typespec!");
}

void DeclSpec::setDimensions(ArrayRef<ArraySpec *> Dims) {
  assert(hasAttributeSpec(AS_dimension) &&
         "Adding dimensions to a non-array declspec!");
  Dimensions.reserve(Dims.size());
  for (ArrayRef<ArraySpec*>::iterator
         I = Dims.begin(), E = Dims.end(); I != E; ++I)
    Dimensions.push_back(*I);
}

} //namespace flang
