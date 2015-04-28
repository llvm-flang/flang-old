//===--- LexDiagnostic.h - Diagnostics for lexer --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_LEXDIAGNOSTIC_H
#define LLVM_FLANG_LEXDIAGNOSTIC_H

#include "flang/Basic/Diagnostic.h"

namespace flang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define LEXSTART
#include "flang/Basic/DiagnosticLexKinds.inc"
#undef DIAG
      NUM_BUILTIN_LEX_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace flang

#endif
