//===----- CGCall.h - Encapsulate calling convention details ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGCALL_H
#define FLANG_CODEGEN_CGCALL_H

#include "ABIInfo.h"
#include "CGValue.h"
#include "flang/AST/Type.h"
#include "llvm/IR/Module.h"

namespace flang   {
namespace CodeGen {

class CallArgList {
  SmallVector<llvm::Value*, 16> Values;
  SmallVector<llvm::Value*, 4>  AdditionalValues;
  RValueTy ReturnValue;
public:

  void add(llvm::Value *Arg) {
    Values.push_back(Arg);
  }

  llvm::Value *getLast() const {
    return Values.back();
  }

  void setLast(llvm::Value *Arg) {
    Values.back() = Arg;
  }

  void addAditional(llvm::Value *Arg) {
    AdditionalValues.push_back(Arg);
  }

  void addReturnValueArg(RValueTy Value) {
    ReturnValue = Value;
  }

  RValueTy getReturnValueArg() const {
    return ReturnValue;
  }

  ArrayRef<llvm::Value*> createValues() {
    for(auto I : AdditionalValues)
      Values.push_back(I);
    return Values;
  }

  int getOffset() const {
    return Values.size();
  }
};

class CGFunctionInfo {
public:
  struct ArgInfo {
    ABIArgInfo ABIInfo;

    ArgInfo() : ABIInfo(ABIArgInfo::Reference) {}
  };

  struct RetInfo {
    enum ValueKind {
      ScalarValue,
      ComplexValue
    };

    ABIRetInfo ABIInfo;
    ArgInfo    ReturnArgInfo;
    QualType   Type;

    RetInfo() : ABIInfo(ABIRetInfo::Nothing) {}
  };
private:
  llvm::FunctionType *Type;
  llvm::CallingConv::ID CC;
  unsigned NumArgs;
  ArgInfo *Args;
  RetInfo ReturnInfo;

  CGFunctionInfo() {}
public:
  static CGFunctionInfo *Create(ASTContext &C,
                                llvm::CallingConv::ID CC,
                                llvm::FunctionType *Type,
                                ArrayRef<ArgInfo> Arguments,
                                RetInfo Returns);

  llvm::FunctionType *getFunctionType() const {
    return Type;
  }
  llvm::CallingConv::ID getCallingConv() const {
    return CC;
  }
  ArrayRef<ArgInfo> getArguments() const {
    return ArrayRef<ArgInfo>(Args, size_t(NumArgs));
  }
  RetInfo getReturnInfo() const {
    return ReturnInfo;
  }
};

class CGFunction {
  const CGFunctionInfo *FuncInfo;
  llvm::Function *Function;
public:
  CGFunction() {}
  CGFunction(const CGFunctionInfo *Info,
             llvm::Function *Func)
    : FuncInfo(Info), Function(Func) {}

  const CGFunctionInfo *getInfo() const {
    return FuncInfo;
  }
  llvm::Function *getFunction() const {
    return Function;
  }
};

}
} // end namespace flang

#endif
