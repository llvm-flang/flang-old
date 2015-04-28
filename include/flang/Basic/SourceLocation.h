//===--- SourceLocation.h - Compact identifier for Source Files -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the SourceLocation class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_SOURCELOCATION_H
#define LLVM_FLANG_SOURCELOCATION_H

#include "llvm/Support/SMLoc.h"

namespace flang {

typedef llvm::SMLoc   SourceLocation;
typedef llvm::SMRange SourceRange;

} // end flang namespace

#endif
