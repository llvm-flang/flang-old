//===--- AllDiagnostics.h - Aggregate Diagnostic headers --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Includes all the separate Diagnostic headers & some related helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_ALL_DIAGNOSTICS_H
#define LLVM_FLANG_ALL_DIAGNOSTICS_H

#include "flang/Frontend/FrontendDiagnostic.h"
#include "flang/Parse/LexDiagnostic.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/Sema/SemaDiagnostic.h"

namespace flang {
template <size_t SizeOfStr, typename FieldType>
class StringSizerHelper {
  char FIELD_TOO_SMALL[SizeOfStr <= FieldType(~0U) ? 1 : -1];
public:
  enum { Size = SizeOfStr };
};
} // end namespace flang

#define STR_SIZE(str, fieldTy) flang::StringSizerHelper<sizeof(str)-1, \
                                                        fieldTy>::Size 

#endif
