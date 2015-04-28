//===---- TargetInfo.cpp - Encapsulate target details -----------*- C++ -*-===//
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

#include "TargetInfo.h"
#include "ABIInfo.h"
#include "CGABI.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

TargetCodeGenInfo::~TargetCodeGenInfo() {}

namespace CodeGen {

class DefaultABIInfo : public ABIInfo {
public:
  void computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const;
};

void DefaultABIInfo::computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const {
  if(!T->isComplexType())
    return;

  Info = ABIRetInfo(ABIRetInfo::AggregateValueAsArg);
}

/// ABI Info for x86_32
class X86_32ABIInfo : public ABIInfo {
public:
  void computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const;
};

void X86_32ABIInfo::computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const {
  if(!T->isComplexType())
    return;

  Info = ABIRetInfo(ABIRetInfo::AggregateValueAsArg);
}

/// ABI Info for x86_64
/// NB: works only with x86_64-pc-{linux,darwin}
class X86_64ABIInfo : public ABIInfo  {
  CodeGenModule &CGM;
public:
  X86_64ABIInfo(CodeGenModule &cgm) : CGM(cgm) {}
  void computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const;
};

void X86_64ABIInfo::computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const {
  if(!T->isComplexType())
    return;

  if(Info.getKind() == ABIRetInfo::Value) {
    switch(T->getBuiltinTypeKind()) {
    case BuiltinType::Real4:
      Info = ABIRetInfo(ABIRetInfo::Value, llvm::VectorType::get(CGM.FloatTy, 2));
      break;
    case BuiltinType::Real8:
      break;
    default:
      llvm_unreachable("invalid type kind");
    }
  }
}

//===----------------------------------------------------------------------===//
// Driver code
//===----------------------------------------------------------------------===//

const TargetCodeGenInfo &CodeGenModule::getTargetCodeGenInfo() {
  if (TheTargetCodeGenInfo)
    return *TheTargetCodeGenInfo;

  llvm::Triple Triple(TheModule.getTargetTriple());

  switch (Triple.getArch()) {
  default:
    TheTargetCodeGenInfo = new TargetCodeGenInfo(new DefaultABIInfo());
    break;

  case llvm::Triple::x86:
    TheTargetCodeGenInfo = new TargetCodeGenInfo(new X86_32ABIInfo());
    break;

  case llvm::Triple::x86_64:
    TheTargetCodeGenInfo = new TargetCodeGenInfo(new X86_64ABIInfo(*this));
    break;
  }
  return *TheTargetCodeGenInfo;
}

}
}
