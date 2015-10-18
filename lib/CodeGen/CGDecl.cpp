//===--- CGDecl.cpp - Emit LLVM Code for declarations ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Decl nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/Decl.h"
#include "flang/AST/DeclVisitor.h"
#include "flang/AST/Expr.h"
#include "flang/AST/StorageSet.h"

namespace flang {
namespace CodeGen {

class FuncDeclEmitter : public ConstDeclVisitor<FuncDeclEmitter> {
  CodeGenFunction &CGF;
public:
  FuncDeclEmitter(CodeGenFunction &cgf) : CGF(cgf) {}

  void VisitVarDecl(const VarDecl *D) {
    CGF.EmitVarDecl(D);
  }
};

void CodeGenFunction::EmitFunctionDecls(const DeclContext *DC) {
  FuncDeclEmitter DV(*this);
  DV.Visit(DC);
}

void CodeGenFunction::EmitVarDecl(const VarDecl *D) {
  if(D->isParameter() ||
     D->isArgument()  ||
     D->isFunctionResult()) return;

  if(D->hasStorageSet()) {
    if(auto E = dyn_cast<EquivalenceSet>(D->getStorageSet())) {
      auto Set = EmitEquivalenceSet(E);
      auto Ptr = EmitEquivalenceSetObject(Set, D);
      LocalVariables.insert(std::make_pair(D, Ptr));
    } else if(auto CB = dyn_cast<CommonBlockSet>(D->getStorageSet()))
      EmitCommonBlock(CB);
    else
      llvm_unreachable("invalid storage set");
    return;
  }

  llvm::Value *Ptr;
  auto Type = D->getType();
  if(Type.hasAttributeSpec(Qualifiers::AS_save) && !IsMainProgram) {
    Ptr = CGM.EmitGlobalVariable(CurFn->getName(), D);
    HasSavedVariables = true;
  } else {
    if(Type->isArrayType())
      Ptr = CreateArrayAlloca(Type, D->getName());
    else Ptr = Builder.CreateAlloca(ConvertTypeForMem(Type),
                                    nullptr, D->getName());
  }
  LocalVariables.insert(std::make_pair(D, Ptr));
}

class VarInitEmitter : public ConstDeclVisitor<VarInitEmitter> {
  CodeGenFunction &CGF;
  bool VisitSaveQualified;
public:
 VarInitEmitter(CodeGenFunction &cgf, bool VisitSave = false)
    : CGF(cgf), VisitSaveQualified(VisitSave) {}

  void VisitVarDecl(const VarDecl *D) {
    if(D->isParameter() || D->isArgument() ||
       D->isFunctionResult())
      return;
    bool HasSave = D->getType().hasAttributeSpec(Qualifiers::AS_save);
    if(HasSave != VisitSaveQualified)
      return;
    if(D->hasInit())
      CGF.EmitVarInitializer(D);
  }
};

void CodeGenFunction::EmitVarInitializers(const DeclContext *DC) {
  VarInitEmitter DV(*this, false);
  DV.Visit(DC);
}

void CodeGenFunction::EmitSavedVarInitializers(const DeclContext *DC) {
  VarInitEmitter DV(*this, true);
  DV.Visit(DC);
}

void CodeGenFunction::EmitVarInitializer(const VarDecl *D) {
  assert(D->hasInit());

  auto T = D->getType();
  if(T->isArrayType()) {
    auto Dest = Builder.CreateConstInBoundsGEP2_32(ConvertTypeForMem(T),
                                                   GetVarPtr(D), 0, 0);
    auto Init = cast<ArrayConstructorExpr>(D->getInit())->getItems();
    for(size_t I = 0; I < Init.size(); ++I) {
      auto Val = EmitRValue(Init[I]);
      EmitStoreCharSameLength(Val, Builder.CreateConstInBoundsGEP1_64(Dest, I), T.getSelfOrArrayElementType());
    }
    return;
  }
  auto Val = EmitRValue(D->getInit());
  EmitStoreCharSameLength(Val, GetVarPtr(D), D->getType());
}

// FIXME: support substrings.
std::pair<int64_t, int64_t> CodeGenFunction::GetObjectBounds(const VarDecl *Var, const Expr *E) {
  auto Size = CGM.getDataLayout().getTypeStoreSize(ConvertTypeForMem(Var->getType()));
  uint64_t Offset = 0;
  if(auto Arr = dyn_cast<ArrayElementExpr>(E)) {
    Arr->EvaluateOffset(getContext(), Offset);
    Offset = Offset * CGM.getDataLayout().getTypeStoreSize(ConvertTypeForMem(E->getType()));
  } else
    Offset = 0;

  return std::make_pair(-int64_t(Offset), -int64_t(Offset) + int64_t(Size));
}

CodeGenFunction::EquivSet CodeGenFunction::EmitEquivalenceSet(const EquivalenceSet *S) {
  auto Result = EquivSets.find(S);
  if(Result != EquivSets.end())
    return Result->second;

  // Find the bounds of the set
  int64_t LowestBound = 0;
  int64_t HighestBound = 0;
  for(auto I : S->getObjects()) {
    auto Bounds  = GetObjectBounds(I.Var, I.E);
    LowestBound  = std::min(Bounds.first, LowestBound);
    HighestBound = std::max(Bounds.second, HighestBound);
    LocalVariablesInEquivSets.insert(std::make_pair(I.Var, Bounds.first));
  }
  EquivSet Set;
  // FIXME: more accurate alignment?
  Set.Ptr= Builder.Insert(new llvm::AllocaInst(CGM.Int8Ty,
                          llvm::ConstantInt::get(CGM.SizeTy, HighestBound - LowestBound),
                          CGM.getDataLayout().getTypeStoreSize(CGM.DoubleTy)),
                          "equivalence-set");
  Set.LowestBound = LowestBound;
  EquivSets.insert(std::make_pair(S, Set));
  return Set;
}

llvm::Value *CodeGenFunction::EmitEquivalenceSetObject(EquivSet Set, const VarDecl *Var) {
  // Compute the pointer to the object.
  auto ObjLowBound = LocalVariablesInEquivSets.find(Var)->second;
  auto Ptr = Builder.CreateConstInBoundsGEP1_64(Set.Ptr,
                                                uint64_t(ObjLowBound - Set.LowestBound));
  auto T = Var->getType();
  return Builder.CreatePointerCast(Ptr, llvm::PointerType::get(ConvertTypeForMem(T), 0));
}

void CodeGenFunction::EmitCommonBlock(const CommonBlockSet *S) {
  auto Result = CommonBlocks.find(S);
  if(Result != CommonBlocks.end())
    return;

  SmallVector<llvm::Type*, 32> Items;
  for(auto Obj : S->getObjects()) {
    if(Obj.Var)
      Items.push_back(ConvertTypeForMem(Obj.Var->getType()));
  }
  auto CBType = llvm::StructType::get(CGM.getLLVMContext(), Items);
  auto Ptr = Builder.CreateBitCast(CGM.EmitCommonBlock(S->getDecl(), CBType),
                                   llvm::PointerType::get(CBType, 0));
  CommonBlocks.insert(std::make_pair(S, Ptr));
  unsigned Idx = 0;
  for(auto Obj : S->getObjects()) {
    if(Obj.Var) {
      LocalVariables.insert(std::make_pair(Obj.Var,
        Builder.CreateStructGEP(nullptr,
                                Ptr, Idx)));
    }
    ++Idx;
  }
}

}
}
