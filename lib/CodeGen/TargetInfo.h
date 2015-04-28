//===---- TargetInfo.h - Encapsulate target details -------------*- C++ -*-===//
//
// The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These classes wrap the information about a call or function
// definition used to handle ABI compliancy.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_TARGETINFO_H
#define FLANG_CODEGEN_TARGETINFO_H

#include "flang/AST/Type.h"
#include "flang/Basic/LLVM.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallString.h"

namespace llvm {
  class GlobalValue;
  class Type;
  class Value;
}

namespace flang {
  class ABIInfo;
  class Decl;

  namespace CodeGen {
    class CallArgList;
    class CodeGenModule;
    class CodeGenFunction;
    class CGFunctionInfo;
  }

  /// TargetCodeGenInfo - This class organizes various target-specific
  /// codegeneration issues, like target-specific attributes, builtins and so
  /// on.
  class TargetCodeGenInfo {
    ABIInfo *Info;
  public:
    TargetCodeGenInfo(ABIInfo *info = nullptr):Info(info) { }
    virtual ~TargetCodeGenInfo();

    /// getABIInfo() - Returns ABI info helper for the target.
    const ABIInfo& getABIInfo() const { return *Info; }
  };
}

#endif // FLANG_CODEGEN_TARGETINFO_H
