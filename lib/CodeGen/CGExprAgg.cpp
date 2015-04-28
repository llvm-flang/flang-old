//===--- CGExprAgg.cpp - Emit LLVM Code from Aggregate Expressions --------===//
//
// The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Aggregate Expr nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"

namespace flang {
namespace CodeGen {

class AggregateExprEmitter
  : public ConstExprVisitor<AggregateExprEmitter, RValueTy> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
public:

  AggregateExprEmitter(CodeGenFunction &cgf);

  RValueTy EmitExpr(const Expr *E);
  RValueTy VisitVarExpr(const VarExpr *E);
  RValueTy VisitArrayElementExpr(const ArrayElementExpr *E);
  RValueTy VisitMemberExpr(const MemberExpr *E);
  RValueTy VisitCallExpr(const CallExpr *E);
  RValueTy VisitTypeConstructorExpr(const TypeConstructorExpr *E);
};

AggregateExprEmitter::AggregateExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()) {}

RValueTy AggregateExprEmitter::EmitExpr(const Expr *E) {
  return Visit(E);
}

RValueTy AggregateExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto VD = E->getVarDecl();
  if(CGF.IsInlinedArgument(VD))
    return CGF.GetInlinedArgumentValue(VD);
  else if(VD->isParameter())
    return EmitExpr(VD->getInit());
  else if(VD->isFunctionResult())
    return RValueTy::getAggregate(CGF.GetRetVarPtr());
  return RValueTy::getAggregate(CGF.GetVarPtr(VD));
}

RValueTy AggregateExprEmitter::VisitArrayElementExpr(const ArrayElementExpr *E) {
  return RValueTy::getAggregate(CGF.EmitArrayElementPtr(E));
}

RValueTy AggregateExprEmitter::VisitMemberExpr(const MemberExpr *E) {
  auto Val = EmitExpr(E->getTarget());
  return RValueTy::getAggregate(CGF.EmitAggregateMember(Val.getAggregateAddr(), E->getField()),
                                Val.isVolatileQualifier());
}

RValueTy AggregateExprEmitter::VisitCallExpr(const CallExpr *E) {
  return CGF.EmitCall(E);
}

RValueTy AggregateExprEmitter::VisitTypeConstructorExpr(const TypeConstructorExpr *E) {
  // create a temporary object.
  auto TempStruct = CGF.CreateTempAlloca(CGF.ConvertTypeForMem(E->getType()), "type-constructor");
  auto Values = E->getArguments();
  auto Fields = E->getType().getSelfOrArrayElementType()->asRecordType()->getElements();
  for(unsigned I = 0; I < Values.size(); ++I) {
    CGF.EmitStore(CGF.EmitRValue(Values[I]),
                  LValueTy(Builder.CreateStructGEP(CGF.ConvertTypeForMem(E->getType()),
                                                   TempStruct,I)),
                  Fields[I]->getType());
  }
  return RValueTy::getAggregate(TempStruct);
}

RValueTy CodeGenFunction::EmitAggregateExpr(const Expr *E) {
  AggregateExprEmitter EV(*this);
  return EV.EmitExpr(E);
}

void CodeGenFunction::EmitAggregateAssignment(const Expr *LHS, const Expr *RHS) {
  auto Val = EmitAggregateExpr(RHS);
  auto Dest = EmitLValue(LHS);
  Builder.CreateStore(Builder.CreateLoad(Val.getAggregateAddr(), Val.isVolatileQualifier()),
                      Dest.getPointer(), Dest.isVolatileQualifier());
}

llvm::Value *CodeGenFunction::EmitAggregateMember(llvm::Value *Agg, const FieldDecl *Field) {
  return Builder.CreateStructGEP(Agg->getType(), Agg, Field->getIndex());
}

void CodeGenFunction::EmitAggregateReturn(const CGFunctionInfo::RetInfo &Info, llvm::Value *Ptr) {
  if(Info.Type->isComplexType()) {
    if(Info.ABIInfo.hasAggregateReturnType()) {
      Builder.CreateRet(Builder.CreateLoad(
        Builder.CreateBitCast(Ptr, llvm::PointerType::get(Info.ABIInfo.getAggregateReturnType(), 0))));
      return;
    }
    Builder.CreateRet(CreateComplexAggregate(
                        EmitComplexLoad(Ptr)));
  } else {
    Builder.CreateRet(Builder.CreateLoad(Ptr));
  }
}

}
}
