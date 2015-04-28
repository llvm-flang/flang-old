//===----- CGIORuntime.h - Interface to IO Runtimes -------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for IO statements code generation.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_IORUNTIME_H
#define FLANG_CODEGEN_IORUNTIME_H

#include "flang/AST/Stmt.h"

namespace flang {
namespace CodeGen {

class CodeGenFunction;
class CodeGenModule;

class CGIORuntime {
protected:
  CodeGenModule &CGM;

public:
  CGIORuntime(CodeGenModule &cgm) : CGM(cgm) {}
  virtual ~CGIORuntime();

  virtual void EmitWriteStmt(CodeGenFunction &CGF, const WriteStmt *S) = 0;
  virtual void EmitPrintStmt(CodeGenFunction &CGF, const PrintStmt *S) = 0;
};

/// Creates an instance of a Libflang IO runtime class.
CGIORuntime *CreateLibflangIORuntime(CodeGenModule &CGM);

}
}  // end namespace flang

#endif
