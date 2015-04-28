//===--- CodeGenFunction.cpp - Emit LLVM Code from ASTs for a Function ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-function state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGSystemRuntime.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Expr.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Operator.h"

namespace flang {
namespace CodeGen {

CodeGenFunction::CodeGenFunction(CodeGenModule &cgm, llvm::Function *Fn)
  : CGM(cgm), /*, Target(cgm.getTarget()),*/
    Builder(cgm.getModule().getContext()),
    UnreachableBlock(nullptr), CurFn(Fn), IsMainProgram(false),
    ReturnValuePtr(nullptr), AllocaInsertPt(nullptr),
    AssignedGotoVarPtr(nullptr), AssignedGotoDispatchBlock(nullptr),
    CurLoopScope(nullptr), CurInlinedStmtFunc(nullptr) {
  HasSavedVariables = false;
}

CodeGenFunction::~CodeGenFunction() {
}

void CodeGenFunction::EmitMainProgramBody(const DeclContext *DC, const Stmt *S) {
  EmitBlock(createBasicBlock("program_entry"));
  CGM.getSystemRuntime().EmitInit(*this);
  IsMainProgram = true;

  ReturnBlock = createBasicBlock("program_exit");
  EmitFunctionBody(DC, S);

  EmitBlock(ReturnBlock);
  EmitCleanup();
  auto ReturnValue = Builder.getInt32(0);
  Builder.CreateRet(ReturnValue);
  if(AssignedGotoDispatchBlock)
    EmitAssignedGotoDispatcher();
}

void CodeGenFunction::EmitFunctionArguments(const FunctionDecl *Func,
                                            const CGFunctionInfo *Info) {
  ArgsList = Func->getArguments();
  ArgsInfo = Info->getArguments();

  size_t I = 0;
  auto Arg = CurFn->arg_begin();

  for(; I < ArgsList.size(); ++Arg, ++I) {
    auto ArgDecl = ArgsList[I];
    auto Info = GetArgInfo(ArgDecl);

    auto ABI = Info.ABIInfo.getKind();
    if(ABI == ABIArgInfo::ExpandCharacterPutLengthToAdditionalArgsAsInt) {
      ExpandedArg AArg;
      AArg.Decl = ArgDecl;
      AArg.A1 = Arg;
      ExpandedArgs.push_back(AArg);
    }
    else
      LocalVariables.insert(std::make_pair(ArgDecl, Arg));

    Arg->setName(ArgDecl->getName());
    if(ArgDecl->getType()->isArrayType() ||
       ABI == ABIArgInfo::Reference) {
      llvm::AttrBuilder Attributes;
      Attributes.addAttribute(llvm::Attribute::NoAlias);
      Arg->addAttr(llvm::AttributeSet::get(CGM.getLLVMContext(), 0, Attributes));
    }
  }

  // Extra argument for the returned data.
  auto RetABIKind = Info->getReturnInfo().ABIInfo.getKind();
  if(RetABIKind == ABIRetInfo::CharacterValueAsArg ||
     RetABIKind == ABIRetInfo::AggregateValueAsArg) {
    Arg->setName(Func->getName());
    ReturnValuePtr = Arg;
    ++Arg;
  }

  // Additional arguments.
  for(I = 0; I < ExpandedArgs.size(); ++Arg, ++I) {
    Arg->setName(llvm::Twine(ExpandedArgs[I].Decl->getName()) + ".length");
    ExpandedArgs[I].A2 = Arg;
  }
}

void CodeGenFunction::EmitFunctionPrologue(const FunctionDecl *Func,
                                           const CGFunctionInfo *Info) {
  EmitBlock(createBasicBlock("entry"));

  // Extract argument values when necessary
  for(auto Arg : ArgsList) {
    if(Arg->getType()->isCharacterType())
      GetCharacterArg(Arg);
  }

  // Create return value and lbock
  auto RetABI = Info->getReturnInfo().ABIInfo.getKind();
  if(RetABI == ABIRetInfo::Value) {
    ReturnValuePtr = Builder.CreateAlloca(ConvertType(Func->getType()),
                                          nullptr, Func->getName());
  }
  ReturnBlock = createBasicBlock("return");
}

void CodeGenFunction::EmitFunctionBody(const DeclContext *DC, const Stmt *S) {
  EmitFunctionDecls(DC);
  auto BodyBB = createBasicBlock("body");
  AllocaInsertPt = Builder.CreateBr(BodyBB);
  EmitBlock(BodyBB);
  if(HasSavedVariables)
    EmitFirstInvocationBlock(DC, S);
  EmitVarInitializers(DC);
  if(S)
    EmitStmt(S);
}

void CodeGenFunction::EmitFirstInvocationBlock(const DeclContext *DC,
                                               const Stmt *S) {
  auto GlobalFirstInvocationFlag = CGM.EmitGlobalVariable(CurFn->getName(), "FIRST_INVOCATION",
                                                          CGM.Int1Ty, Builder.getTrue());
  auto FirstInvocationBB = createBasicBlock("first-invocation");
  auto EndBB = createBasicBlock("first-invocation-end");
  Builder.CreateCondBr(Builder.CreateLoad(GlobalFirstInvocationFlag), FirstInvocationBB,
                       EndBB);
  EmitBlock(FirstInvocationBB);
  EmitSavedVarInitializers(DC);
  Builder.CreateStore(Builder.getFalse(), GlobalFirstInvocationFlag);
  EmitBlock(EndBB);
}

void CodeGenFunction::EmitCleanup() {
  for(auto I : TempHeapAllocations)
    CGM.getSystemRuntime().EmitFree(*this, I);
}

void CodeGenFunction::EmitFunctionEpilogue(const FunctionDecl *Func,
                                           const CGFunctionInfo *Info) {  
  EmitBlock(ReturnBlock);
  EmitCleanup();
  // return
  if(auto RetVar = GetRetVarPtr()) {
    auto ReturnInfo = Info->getReturnInfo();

    if(ReturnInfo.ABIInfo.getKind() == ABIRetInfo::Value) {
      if(ReturnInfo.Type->isComplexType() ||
         ReturnInfo.Type->isRecordType()) {
        EmitAggregateReturn(ReturnInfo, RetVar);
      } else
        Builder.CreateRet(Builder.CreateLoad(RetVar));
    }
    else Builder.CreateRetVoid();
  } else
    Builder.CreateRetVoid();
  if(AssignedGotoDispatchBlock)
    EmitAssignedGotoDispatcher();
}

llvm::Value *CodeGenFunction::GetVarPtr(const VarDecl *D) {
  if(D->isFunctionResult())
    return ReturnValuePtr;
  return LocalVariables[D];
}

llvm::Value *CodeGenFunction::GetRetVarPtr() {
  return ReturnValuePtr;
}

CGFunctionInfo::ArgInfo CodeGenFunction::GetArgInfo(const VarDecl *Arg) const {
  for(size_t I = 0; I < ArgsList.size(); ++I) {
    if(ArgsList[I] == Arg) return ArgsInfo[I];
  }
  assert(false && "Invalid argument");
  return CGFunctionInfo::ArgInfo();
}

const VarDecl *CodeGenFunction::GetExternalFunctionArgument(const FunctionDecl *Func) {
  for(auto Arg : ArgsList) {
    if(auto FType = Arg->getType()->asFunctionType()) {
      if(FType->getPrototype() == Func)
        return Arg;
    }
  }
  llvm_unreachable("invalid external function argument");
  return nullptr;
}

llvm::Value *CodeGenFunction::CreateTempHeapAlloca(llvm::Value *Size) {
  auto P = CGM.getSystemRuntime().EmitMalloc(*this, Size->getType() != CGM.SizeTy?
                                             Builder.CreateZExtOrTrunc(Size, CGM.SizeTy) : Size);
  TempHeapAllocations.push_back(P);
  return P;
}

llvm::Value *CodeGenFunction::CreateTempHeapAlloca(llvm::Value *Size, llvm::Type *PtrType) {
  auto P = CreateTempHeapAlloca(Size);
  return P->getType() != PtrType? Builder.CreateBitCast(P, PtrType) : P;
}

llvm::Value *CodeGenFunction::GetIntrinsicFunction(int FuncID,
                                                   ArrayRef<llvm::Type*> ArgTypes) const {
  return llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                         (llvm::Intrinsic::ID)FuncID,
                                         ArgTypes);
}

llvm::Value *CodeGenFunction::GetIntrinsicFunction(int FuncID,
                                                   llvm::Type *T1) const {
  return llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                         (llvm::Intrinsic::ID)FuncID,
                                         T1);
}

llvm::Value *CodeGenFunction::GetIntrinsicFunction(int FuncID,
                                                   llvm::Type *T1, llvm::Type *T2) const {
  llvm::Type *ArgTypes[2] = {T1, T2};
  return llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                         (llvm::Intrinsic::ID)FuncID,
                                         ArrayRef<llvm::Type*>(ArgTypes,2));
}

llvm::Type *CodeGenFunction::ConvertTypeForMem(QualType T) const {
  return CGM.getTypes().ConvertTypeForMem(T);
}

llvm::Type *CodeGenFunction::ConvertType(QualType T) const {
  return CGM.getTypes().ConvertType(T);
}

} // end namespace CodeGen
} // end namespace flang
