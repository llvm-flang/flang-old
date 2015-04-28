//===--- CGExpr.cpp - Emit LLVM Code from Expressions ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"

namespace flang {
namespace CodeGen {

/// CreateTempAlloca - This creates a alloca and inserts it into the entry
/// block.
llvm::AllocaInst *CodeGenFunction::CreateTempAlloca(llvm::Type *Ty,
                                                    const llvm::Twine &Name) {
  return new llvm::AllocaInst(Ty, 0, Name, AllocaInsertPt);
}

RValueTy CodeGenFunction::EmitRValue(const Expr *E) {
  auto EType = E->getType();
  if(EType->isComplexType())
    return EmitComplexExpr(E);
  else if(EType->isCharacterType())
    return EmitCharacterExpr(E);
  else if(EType->isLogicalType())
    return EmitLogicalValueExpr(E);
  else if(EType->isRecordType())
    return EmitAggregateExpr(E);
  else
    return EmitScalarExpr(E);
}

RValueTy CodeGenFunction::EmitLoad(llvm::Value *Ptr, QualType T, bool IsVolatile) {
  if(T->isComplexType())
    return EmitComplexLoad(Ptr, IsVolatile);
  else
    return Builder.CreateLoad(Ptr, IsVolatile);
}

void CodeGenFunction::EmitStore(RValueTy Val, LValueTy Dest, QualType T) {
  auto Ptr = Dest.getPointer();
  auto IsVolatile = Dest.isVolatileQualifier();
  if(Val.isScalar()) {
    if(Val.asScalar()->getType() == CGM.Int1Ty)
      Val = ConvertLogicalValueToLogicalMemoryValue(Val.asScalar(),
                                                    T->isArrayType()? T->asArrayType()->getElementType() : T);
    Builder.CreateStore(Val.asScalar(), Ptr, IsVolatile);
  } else if(Val.isComplex())
    EmitComplexStore(Val.asComplex(), Ptr, IsVolatile);
  else if(Val.isAggregate()) {
    Builder.CreateStore(Builder.CreateLoad(Val.getAggregateAddr(), Val.isVolatileQualifier()),
                        Ptr, IsVolatile);
  }
}

void CodeGenFunction::EmitStoreCharSameLength(RValueTy Val, LValueTy Dest, QualType T) {
  if(!Val.isCharacter())
    return EmitStore(Val, Dest, T);
  auto CharVal = Val.asCharacter();
  Builder.CreateMemCpy(Dest.getPointer(), CharVal.Ptr, CharVal.Len, 1, Dest.isVolatileQualifier());
}

RValueTy CodeGenFunction::EmitBinaryExpr(BinaryExpr::Operator Op, RValueTy LHS, RValueTy RHS) {
  if(LHS.isScalar())
    return EmitScalarBinaryExpr(Op, LHS.asScalar(), RHS.asScalar());
  else if(LHS.isComplex()) {
    if(Op == BinaryExpr::Plus || Op == BinaryExpr::Minus || Op == BinaryExpr::Multiply ||
       Op == BinaryExpr::Divide || Op == BinaryExpr::Power)
     return EmitComplexBinaryExpr(Op, LHS.asComplex(), RHS.asComplex());
    return EmitComplexRelationalExpr(Op, LHS.asComplex(), RHS.asComplex());
  } else // FIXME: character concat
    return EmitCharacterRelationalExpr(Op, LHS.asCharacter(), RHS.asCharacter());
}

RValueTy CodeGenFunction::EmitUnaryExpr(UnaryExpr::Operator Op, RValueTy Val) {
  switch(Op) {
  case UnaryExpr::Plus:
    return Val;
  case UnaryExpr::Minus:
    if(Val.isScalar())
      return EmitScalarUnaryMinus(Val.asScalar());
    return EmitComplexUnaryMinus(Val.asComplex());
  case UnaryExpr::Not:
    return EmitScalarUnaryNot(Val.asScalar());
  }
  return RValueTy();
}

RValueTy CodeGenFunction::EmitImplicitConversion(RValueTy Val, QualType T) {
  if(Val.isScalar()) {
    if(T->isComplexType())
      return EmitScalarToComplexConversion(Val.asScalar(), T);
    return EmitScalarToScalarConversion(Val.asScalar(), T);
  }
  assert(Val.isComplex());
  if(T->isComplexType())
    return EmitComplexToComplexConversion(Val.asComplex(), T);
  return EmitComplexToScalarConversion(Val.asComplex(), T);
}

llvm::Constant *CodeGenFunction::EmitConstantExpr(const Expr *E) {
  auto T = E->getType();
  if(T->isComplexType())
    return CreateComplexConstant(EmitComplexExpr(E));
  else if(T->isCharacterType()) //FIXME
    ;//;return CreateCharacterConstant(EmitCharacterExpr(E));
  else if(T->isLogicalType())
    return cast<llvm::Constant>(EmitLogicalValueExpr(E));
  else if(T->isArrayType())
    return EmitConstantArrayExpr(dyn_cast<ArrayConstructorExpr>(E));
  else
    return cast<llvm::Constant>(EmitScalarExpr(E));
}

class LValueExprEmitter
  : public ConstExprVisitor<LValueExprEmitter, LValueTy> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;
public:

  LValueExprEmitter(CodeGenFunction &cgf);

  LValueTy VisitVarExpr(const VarExpr *E);
  LValueTy VisitArrayElementExpr(const ArrayElementExpr *E);
  LValueTy VisitMemberExpr(const MemberExpr *E);
};

LValueExprEmitter::LValueExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

LValueTy LValueExprEmitter::VisitVarExpr(const VarExpr *E) {
  return LValueTy(CGF.GetVarPtr(E->getVarDecl()));
}

LValueTy LValueExprEmitter::VisitArrayElementExpr(const ArrayElementExpr *E) {
  return CGF.EmitArrayElementPtr(E->getTarget(), E->getSubscripts());
}

LValueTy LValueExprEmitter::VisitMemberExpr(const MemberExpr *E) {
  return CGF.EmitAggregateMember(Visit(E->getTarget()).getPointer(),
                                 E->getField());
}

LValueTy CodeGenFunction::EmitLValue(const Expr *E) {
  LValueExprEmitter EV(*this);
  return EV.Visit(E);
}

}
} // end namespace flang
