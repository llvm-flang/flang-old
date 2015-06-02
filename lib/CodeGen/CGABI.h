//===----- CGABI.h - ABI types-----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_ABI_H
#define FLANG_CODEGEN_ABI_H

#include "ABIInfo.h"

namespace flang {
namespace CodeGen {

class FortranABI {
public:
  virtual ~FortranABI() {}
  virtual ABIArgInfo GetArgABI(QualType ArgType);
  virtual ABIRetInfo GetRetABI(QualType RetType);
};

class LibflangABI : public FortranABI {
public:
  virtual ~LibflangABI() {}
  ABIArgInfo GetArgABI(QualType ArgType);
  ABIRetInfo GetRetABI(QualType RetType);
};

class LibflangTransferABI : public LibflangABI {
public:
  virtual ~LibflangTransferABI() {}
  ABIArgInfo GetArgABI(QualType ArgType);
};

}
}  // end namespace flang

#endif
