//===--- CodeGenModule.h - Per-Module state for LLVM CodeGen ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-translation-unit state used for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CODEGENMODULE_H
#define FLANG_CODEGEN_CODEGENMODULE_H

#include "CodeGenTypes.h"
#include "flang/AST/Decl.h"
#include "flang/Basic/LangOptions.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Support/SpecialCaseList.h"

namespace llvm {
  class Module;
  class Constant;
  class ConstantInt;
  class ConstantArray;
  class Function;
  class GlobalValue;
  class DataLayout;
  class FunctionType;
  class LLVMContext;
}

namespace flang {
  class ASTContext;
  class Decl;
  class Expr;
  class Stmt;
  class NamedDecl;
  class ValueDecl;
  class VarDecl;
  class LangOptions;
  class DiagnosticsEngine;
  class TargetCodeGenInfo;

namespace CodeGen {

  class CGIORuntime;
  class CGSystemRuntime;

struct CodeGenTypeCache {
  /// void
  llvm::Type *VoidTy;

  /// i1, i8, i16, i32, and i64
  llvm::IntegerType *Int1Ty, *Int8Ty, *Int16Ty, *Int32Ty, *Int64Ty;
  /// float, double
  llvm::Type *FloatTy, *DoubleTy;

  /// int
  llvm::IntegerType *IntTy;

  /// intptr_t, size_t, and ptrdiff_t, which we assume are the same size.
  union {
    llvm::IntegerType *IntPtrTy;
    llvm::IntegerType *SizeTy;
    llvm::IntegerType *PtrDiffTy;
  };

  /// void* in address space 0
  union {
    llvm::PointerType *VoidPtrTy;
    llvm::PointerType *Int8PtrTy;
  };

  /// void** in address space 0
  union {
    llvm::PointerType *VoidPtrPtrTy;
    llvm::PointerType *Int8PtrPtrTy;
  };

  /// The width of a pointer into the generic address space.
  unsigned char PointerWidthInBits;

  /// The size and alignment of a pointer into the generic address
  /// space.
  union {
    unsigned char PointerAlignInBytes;
    unsigned char PointerSizeInBytes;
    unsigned char SizeSizeInBytes;     // sizeof(size_t)
  };

  llvm::CallingConv::ID RuntimeCC;
  llvm::CallingConv::ID getRuntimeCC() const {
    return RuntimeCC;
  }
};

/// CodeGenModule - This class organizes the cross-function state that is used
/// while generating LLVM code.
class CodeGenModule : public CodeGenTypeCache {
  CodeGenModule(const CodeGenModule &) = delete;
  void operator=(const CodeGenModule &) = delete;

  ASTContext &Context;
  const LangOptions &LangOpts;
  const CodeGenOptions &CodeGenOpts;
  llvm::Module &TheModule;
  DiagnosticsEngine &Diags;
  const llvm::DataLayout &TheDataLayout;
  //const TargetInfo &Target;
  llvm::LLVMContext &VMContext;

  CodeGenTypes Types;
  LibflangABI RuntimeABI;
  const TargetCodeGenInfo *TheTargetCodeGenInfo;
  CGIORuntime *IORuntime;
  CGSystemRuntime *SystemRuntime;

  /// RuntimeFunctions - contains all the runtime functions
  /// used in this module.
  llvm::StringMap<CGFunction> RuntimeFunctions;

  llvm::DenseMap<const FunctionDecl*, CGFunction> Functions;



public:
  CodeGenModule(ASTContext &C, const CodeGenOptions &CodeGenOpts,
                llvm::Module &M, const llvm::DataLayout &TD,
                DiagnosticsEngine &Diags);

  ~CodeGenModule();

  ASTContext &getContext() const { return Context; }

  llvm::Module &getModule() const { return TheModule; }

  llvm::LLVMContext &getLLVMContext() const { return VMContext; }

  const llvm::DataLayout &getDataLayout() const {
    return TheDataLayout;
  }

  CodeGenTypes &getTypes() { return Types; }

  bool hasIORuntime() const {
    return IORuntime != nullptr;
  }

  /// getIORuntime() - Return a reference to the configured
  /// IO runtime.
  CGIORuntime &getIORuntime() {
    return *IORuntime;
  }

  bool hasSystemRuntime() const {
    return SystemRuntime != nullptr;
  }

  /// getSystemRuntime() - Return a reference to the configured
  /// system runtime.
  CGSystemRuntime &getSystemRuntime() {
    return *SystemRuntime;
  }

  /// getTargetCodeGenInfo - Retun a reference to the configured
  /// target code gen information.
  const TargetCodeGenInfo &getTargetCodeGenInfo();

  /// Release - Finalize LLVM code generation.
  void Release();

  void EmitTopLevelDecl(const Decl *Declaration);

  void EmitMainProgramDecl(const MainProgramDecl *Program);

  void EmitFunctionDecl(const FunctionDecl *Function);

  llvm::GlobalVariable *EmitGlobalVariable(StringRef FuncName, const VarDecl *Var,
                                           llvm::Constant *Initializer = nullptr);

  llvm::GlobalVariable *EmitGlobalVariable(StringRef FuncName, StringRef VarName,
                                           llvm::Type *Type, llvm::Constant *Initializer);

  llvm::Value *EmitConstantArray(llvm::Constant *Array);

  llvm::Value *EmitCommonBlock(const CommonBlockDecl *CB,
                               llvm::Type *Type,
                               llvm::Constant *Initializer = nullptr);

  llvm::Value *GetCFunction(StringRef Name,
                            ArrayRef<llvm::Type*> ArgTypes,
                            llvm::Type *ReturnType = nullptr);

  llvm::Value *GetRuntimeFunction(StringRef Name,
                                  ArrayRef<llvm::Type*> ArgTypes,
                                  llvm::Type *ReturnType = nullptr);

  CGFunction GetRuntimeFunction(StringRef Name,
                                ArrayRef<CGType> ArgTypes,
                                CGType ReturnType = CGType(),
                                FortranABI *ABI = nullptr);

  CGFunction GetRuntimeFunction1(StringRef Name,
                                 CGType ArgType,
                                 CGType ReturnType = CGType(),
                                 FortranABI *ABI = nullptr) {
    return GetRuntimeFunction(Name, ArgType,
                              ReturnType, ABI);
  }

  CGFunction GetRuntimeFunction2(StringRef Name,
                                 CGType A1, CGType A2,
                                 CGType ReturnType = CGType(),
                                 FortranABI *ABI = nullptr) {
    CGType ArgTypes[] = { A1, A2 };
    return GetRuntimeFunction(Name, llvm::makeArrayRef(ArgTypes, 2),
                              ReturnType, ABI);
  }

  CGFunction GetRuntimeFunction3(StringRef Name,
                                 CGType A1, CGType A2, CGType A3,
                                 CGType ReturnType = CGType(),
                                 FortranABI *ABI = nullptr) {
    CGType ArgTypes[] = { A1, A2, A3 };
    return GetRuntimeFunction(Name, llvm::makeArrayRef(ArgTypes, 3),
                              ReturnType, ABI);
  }

  CGFunction GetRuntimeFunction4(StringRef Name,
                                 CGType A1, CGType A2, CGType A3,
                                 CGType A4, CGType ReturnType = CGType(),
                                 FortranABI *ABI = nullptr) {
    CGType ArgTypes[] = { A1, A2, A3, A4 };
    return GetRuntimeFunction(Name, llvm::makeArrayRef(ArgTypes, 4),
                              ReturnType, ABI);
  }

  CGFunction GetRuntimeFunction5(StringRef Name,
                                 CGType A1, CGType A2, CGType A3,
                                 CGType A4, CGType A5, CGType ReturnType = CGType(),
                                 FortranABI *ABI = nullptr) {
    CGType ArgTypes[] = { A1, A2, A3, A4, A5 };
    return GetRuntimeFunction(Name, llvm::makeArrayRef(ArgTypes, 5),
                              ReturnType, ABI);
  }

  CGFunction GetFunction(const FunctionDecl *Function);

};

}  // end namespace CodeGen
}  // end namespace flang

#endif
