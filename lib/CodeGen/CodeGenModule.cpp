//===--- CodeGenModule.cpp - Emit LLVM Code from ASTs for a Module --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-module state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenModule.h"
#include "CodeGenFunction.h"
#include "CGIORuntime.h"
#include "CGSystemRuntime.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/DeclVisitor.h"
#include "flang/Basic/Diagnostic.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/IR/Mangler.h"

namespace flang {
namespace CodeGen {

CodeGenModule::CodeGenModule(ASTContext &C, const CodeGenOptions &CGO,
                             llvm::Module &M, const llvm::DataLayout &TD,
                             DiagnosticsEngine &diags)
  : Context(C), LangOpts(C.getLangOpts()), CodeGenOpts(CGO), TheModule(M),
    Diags(diags), TheDataLayout(TD), VMContext(M.getContext()), Types(*this),
    TheTargetCodeGenInfo(nullptr) {

  llvm::LLVMContext &LLVMContext = M.getContext();
  VoidTy = llvm::Type::getVoidTy(LLVMContext);
  Int1Ty = llvm::Type::getInt1Ty(LLVMContext);
  Int8Ty = llvm::Type::getInt8Ty(LLVMContext);
  Int16Ty = llvm::Type::getInt16Ty(LLVMContext);
  Int32Ty = llvm::Type::getInt32Ty(LLVMContext);
  Int64Ty = llvm::Type::getInt64Ty(LLVMContext);
  FloatTy = llvm::Type::getFloatTy(LLVMContext);
  DoubleTy = llvm::Type::getDoubleTy(LLVMContext);
  PointerWidthInBits = TD.getPointerSizeInBits(0);
  IntPtrTy = llvm::IntegerType::get(LLVMContext, PointerWidthInBits);
  Int8PtrTy = Int8Ty->getPointerTo(0);
  Int8PtrPtrTy = Int8PtrTy->getPointerTo(0);
  RuntimeCC = llvm::CallingConv::C;

  IORuntime = CreateLibflangIORuntime(*this);
  SystemRuntime = CreateLibflangSystemRuntime(*this);
}

CodeGenModule::~CodeGenModule() {
  if(IORuntime)
    delete IORuntime;
}

void CodeGenModule::Release() {
}

llvm::Value*
CodeGenModule::GetCFunction(StringRef Name,
                            ArrayRef<llvm::Type*> ArgTypes,
                            llvm::Type *ReturnType) {
  if(auto Func = TheModule.getFunction(Name))
    return Func;
  auto FType = llvm::FunctionType::get(ReturnType? ReturnType :
                                       VoidTy, ArgTypes,
                                       false);
  auto Func = llvm::Function::Create(FType, llvm::GlobalValue::ExternalLinkage,
                                     Name, &TheModule);
  Func->setCallingConv(llvm::CallingConv::C);
  return Func;
}

llvm::Value *
CodeGenModule::GetRuntimeFunction(StringRef Name,
                                  ArrayRef<llvm::Type*> ArgTypes,
                                  llvm::Type *ReturnType) {
  llvm::SmallString<32> MangledName("libflang_");
  MangledName.append(Name);
  return GetCFunction(MangledName, ArgTypes, ReturnType);
}

CGFunction
CodeGenModule::GetRuntimeFunction(StringRef Name,
                                  ArrayRef<CGType> ArgTypes,
                                  CGType ReturnType,
                                  FortranABI *ABI) {
  llvm::SmallString<32> MangledName("libflang_");
  MangledName.append(Name);

  auto SearchResult = RuntimeFunctions.find(MangledName);
  if(SearchResult != RuntimeFunctions.end())
    return SearchResult->second;

  auto FunctionInfo = Types.GetFunctionType(ABI? *ABI : RuntimeABI,
                                            ArgTypes,
                                            ReturnType);
  auto Func = llvm::Function::Create(FunctionInfo->getFunctionType(),
                                     llvm::GlobalValue::ExternalLinkage,
                                     llvm::Twine(MangledName), &TheModule);
  Func->setCallingConv(FunctionInfo->getCallingConv());
  auto Result = CGFunction(FunctionInfo, Func);
  RuntimeFunctions[MangledName] = Result;
  return Result;
}

CGFunction CodeGenModule::GetFunction(const FunctionDecl *Function) {
  auto SearchResult = Functions.find(Function);
  if(SearchResult != Functions.end())
    return SearchResult->second;

  auto FunctionInfo = Types.GetFunctionType(Function);

  auto Func = llvm::Function::Create(FunctionInfo->getFunctionType(),
                                     llvm::GlobalValue::ExternalLinkage,
                                     llvm::Twine(Function->getName()) + "_",
                                     &TheModule);

  Func->setCallingConv(FunctionInfo->getCallingConv());
  auto Result = CGFunction(FunctionInfo, Func);
  Functions.insert(std::make_pair(Function, Result));
  return Result;
}

void CodeGenModule::EmitTopLevelDecl(const Decl *Declaration) {
  class Visitor : public ConstDeclVisitor<Visitor> {
  public:
    CodeGenModule *CG;

    Visitor(CodeGenModule *P) : CG(P) {}

    void VisitMainProgramDecl(const MainProgramDecl *D) {
      CG->EmitMainProgramDecl(D);
    }
    void VisitFunctionDecl(const FunctionDecl *D) {
      CG->EmitFunctionDecl(D);
    }
  };
  Visitor DV(this);
  DV.Visit(Declaration);
}

void CodeGenModule::EmitMainProgramDecl(const MainProgramDecl *Program) {
  auto FType = llvm::FunctionType::get(Int32Ty, false);
  auto Linkage = llvm::GlobalValue::ExternalLinkage;
  auto Func = llvm::Function::Create(FType, Linkage, "main", &TheModule);
  Func->setCallingConv(llvm::CallingConv::C);

  CodeGenFunction CGF(*this, Func);
  CGF.EmitMainProgramBody(Program, Program->getBody());
}

void CodeGenModule::EmitFunctionDecl(const FunctionDecl *Function) {
  auto FuncInfo = GetFunction(Function);

  CodeGenFunction CGF(*this, FuncInfo.getFunction());
  CGF.EmitFunctionArguments(Function, FuncInfo.getInfo());
  CGF.EmitFunctionPrologue(Function, FuncInfo.getInfo());
  CGF.EmitFunctionBody(Function, Function->getBody());
  CGF.EmitFunctionEpilogue(Function, FuncInfo.getInfo());
}

llvm::GlobalVariable *CodeGenModule::EmitGlobalVariable(StringRef FuncName, const VarDecl *Var,
                                                        llvm::Constant *Initializer) {
  auto T = getTypes().ConvertTypeForMem(Var->getType());
  return new llvm::GlobalVariable(TheModule, T,
                                  false, llvm::GlobalValue::InternalLinkage,
                                  llvm::Constant::getNullValue(T),
                                  llvm::Twine(FuncName) + Var->getName() + "_");
}

llvm::GlobalVariable *CodeGenModule::EmitGlobalVariable(StringRef FuncName, StringRef VarName,
                                                        llvm::Type *Type, llvm::Constant *Initializer) {
  return new llvm::GlobalVariable(TheModule, Type,
                                  false, llvm::GlobalValue::InternalLinkage, Initializer,
                                  llvm::Twine(FuncName) + VarName + "_");
}

llvm::Value *CodeGenModule::EmitConstantArray(llvm::Constant *Array) {
  // FIXME: fold identical values
  return new llvm::GlobalVariable(TheModule, Array->getType(),
                                  true, llvm::GlobalValue::PrivateLinkage, Array);
}

llvm::Value *CodeGenModule::EmitCommonBlock(const CommonBlockDecl *CB,
                                            llvm::Type *Type,
                                            llvm::Constant *Initializer) {
  llvm::SmallString<32> Name;
  StringRef NameRef;
  if(CB->getIdentifier()) {
    Name.append(CB->getName());
    Name.push_back('_');
    NameRef = Name;
  } else
    NameRef = "__BLNK__"; // FIXME?

  auto Var = TheModule.getGlobalVariable(NameRef);
  if(Var)
    return Var;
  if(!Initializer)
    Initializer = llvm::Constant::getNullValue(Type);
  auto CBVar = new llvm::GlobalVariable(TheModule, Type,
                                        false, llvm::GlobalValue::CommonLinkage,
                                        Initializer, NameRef);
  CBVar->setAlignment(16); // FIXME: proper target dependent alignment value
  return CBVar;
}

}
} // end namespace flang
