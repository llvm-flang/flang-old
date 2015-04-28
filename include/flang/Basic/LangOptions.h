//===--- LangOptions.h - Fortran Language Family Language Opts --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the LangOptions interface.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_LANGOPTIONS_H__
#define FLANG_LANGOPTIONS_H__

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <string>

namespace flang {

/// LangOptions - This class keeps track of the various options that can be
/// enabled, which controls the dialect of Fortran that is accepted.
class LangOptions : public llvm::RefCountedBase<LangOptions> {
public:
  unsigned Fortran77         : 1; // Fortran 77
  unsigned Fortran90         : 1; // Fortran 90
  unsigned Fortran95         : 1; // Fortran 95
  unsigned Fortran2000       : 1; // Fortran 2000
  unsigned Fortran2003       : 1; // Fortran 2003
  unsigned Fortran2008       : 1; // Fortran 2008

  unsigned FixedForm         : 1; // Fixed-form style
  unsigned FreeForm          : 1; // Free-form style

  unsigned ReturnComments    : 1; // Return comments as lexical tokens

  unsigned SpellChecking     : 1; // Whether to perform spell-checking for error
                                  // recovery.

  unsigned DefaultReal8      : 1; // Sets the default real type to be 8 bytes wide
  unsigned DefaultDouble8    : 1; // Sets the default double precision type to be 8 bytes wide
  unsigned DefaultInt8       : 1; // Sets the default integer type to be 8 bytes wide
  unsigned TabWidth;              // The tab character is treated as N spaces.

  LangOptions() {
    Fortran77 = 0;
    Fortran90 = Fortran95 = Fortran2000 = Fortran2003 = 1;
    FixedForm = 0;
    FreeForm = 1;
    ReturnComments = 0;
    SpellChecking = 1;
    DefaultReal8 = DefaultDouble8 = DefaultInt8 = 0;
    TabWidth = 6;
  }
};

}  // end namespace flang

#endif
