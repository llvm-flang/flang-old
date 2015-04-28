//===----- CGSystemRuntime.h - Interface to System specific Runtimes ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for system specific runtime calls.
//
//===----------------------------------------------------------------------===//

#include "CGSystemRuntime.h"
#include "CodeGenFunction.h"

namespace flang {
namespace CodeGen {

CGSystemRuntime::~CGSystemRuntime() {
}

llvm::Value *CGSystemRuntime::EmitMalloc(CodeGenFunction &CGF, llvm::Type *T, llvm::Value *Size) {
  return CGF.getBuilder().CreateBitCast(EmitMalloc(CGF, Size), T);
}

}
} // end namespace flang

