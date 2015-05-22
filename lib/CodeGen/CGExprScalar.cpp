//===--- CGExprScalar.cpp - Emit LLVM Code for Scalar Exprs ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes with scalar LLVM types as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"

namespace flang {
namespace CodeGen {

class ScalarExprEmitter
  : public ConstExprVisitor<ScalarExprEmitter, llvm::Value*> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;
public:

  ScalarExprEmitter(CodeGenFunction &cgf);

  llvm::Value *EmitExpr(const Expr *E);
  llvm::Value *EmitLogicalConditionExpr(const Expr *E);
  llvm::Value *EmitLogicalValueExpr(const Expr *E);
  llvm::Value *VisitIntegerConstantExpr(const IntegerConstantExpr *E);
  llvm::Value *VisitRealConstantExpr(const RealConstantExpr *E);
  llvm::Value *VisitLogicalConstantExpr(const LogicalConstantExpr *E);
  llvm::Value *VisitVarExpr(const VarExpr *E);
  llvm::Value *VisitUnaryExprPlus(const UnaryExpr *E);
  llvm::Value *VisitUnaryExprMinus(const UnaryExpr *E);
  llvm::Value *VisitUnaryExprNot(const UnaryExpr *E);
  llvm::Value *VisitBinaryExpr(const BinaryExpr *E);
  llvm::Value *VisitBinaryExprAnd(const BinaryExpr *E);
  llvm::Value *VisitBinaryExprOr(const BinaryExpr *E);
  llvm::Value *VisitImplicitCastExpr(const ImplicitCastExpr *E);
  llvm::Value *VisitCallExpr(const CallExpr *E);
  llvm::Value *VisitIntrinsicCallExpr(const IntrinsicCallExpr *E);
  llvm::Value *VisitArrayElementExpr(const ArrayElementExpr *E);
  llvm::Value *VisitMemberExpr(const MemberExpr *E);
  llvm::Value *VisitFunctionRefExpr(const FunctionRefExpr *E);
};

ScalarExprEmitter::ScalarExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

llvm::Value *ScalarExprEmitter::EmitExpr(const Expr *E) {
  return Visit(E);
}

llvm::Value *CodeGenFunction::ConvertLogicalValueToInt1(llvm::Value *Val) {
  return Builder.CreateZExtOrTrunc(Val, getModule().Int1Ty);
}

llvm::Value *ScalarExprEmitter::EmitLogicalConditionExpr(const Expr *E) {
  auto Value = Visit(E);
  if(Value->getType() != CGF.getModule().Int1Ty)
    return CGF.ConvertLogicalValueToInt1(Value);
  return Value;
}

llvm::Value *CodeGenFunction::ConvertLogicalValueToLogicalMemoryValue(llvm::Value *Val,
                                                                      QualType T) {
  return Builder.CreateZExtOrTrunc(Val, ConvertType(T));
}

llvm::Value *ScalarExprEmitter::EmitLogicalValueExpr(const Expr *E) {
  auto Value = Visit(E);
  if(Value->getType() == CGF.getModule().Int1Ty)
    return CGF.ConvertLogicalValueToLogicalMemoryValue(Value, E->getType());
  return Value;
}

llvm::Value *CodeGenFunction::EmitIntegerConstantExpr(const IntegerConstantExpr *E) {
  return llvm::ConstantInt::get(Builder.getInt32Ty(), E->getValue().sextOrTrunc(32));
}

llvm::Value *ScalarExprEmitter::VisitIntegerConstantExpr(const IntegerConstantExpr *E) {
  return CGF.EmitIntegerConstantExpr(E);
}

llvm::Value *ScalarExprEmitter::VisitRealConstantExpr(const RealConstantExpr *E) {
  return llvm::ConstantFP::get(VMContext, E->getValue());
}

llvm::Value *ScalarExprEmitter::VisitLogicalConstantExpr(const LogicalConstantExpr *E) {
  return Builder.getInt1(E->isTrue());
}

llvm::Value *ScalarExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto VD = E->getVarDecl();
  if(CGF.IsInlinedArgument(VD))
    return CGF.GetInlinedArgumentValue(VD).asScalar();
  if(VD->isParameter())
    return EmitExpr(VD->getInit());
  auto Ptr = CGF.GetVarPtr(VD);
  return Builder.CreateLoad(Ptr,VD->getName());
}

llvm::Value *ScalarExprEmitter::VisitUnaryExprPlus(const UnaryExpr *E) {
  return EmitExpr(E->getExpression());
}

llvm::Value *CodeGenFunction::EmitScalarUnaryMinus(llvm::Value *Val) {
  return Val->getType()->isIntegerTy()?
           Builder.CreateNeg(Val) :
           Builder.CreateFNeg(Val);
}

llvm::Value *ScalarExprEmitter::VisitUnaryExprMinus(const UnaryExpr *E) {
  return CGF.EmitScalarUnaryMinus(EmitExpr(E->getExpression()));
}

llvm::Value *CodeGenFunction::EmitScalarUnaryNot(llvm::Value *Val) {
  return Builder.CreateXor(Val,
                           llvm::ConstantInt::get(Val->getType(), 1));
}

llvm::Value *ScalarExprEmitter::VisitUnaryExprNot(const UnaryExpr *E) {
  return CGF.EmitScalarUnaryNot(EmitExpr(E->getExpression()));
}

llvm::Value *ScalarExprEmitter::VisitBinaryExpr(const BinaryExpr *E) {
  auto Op = E->getOperator();
  if(Op < BinaryExpr::Plus) {
    // Complex comparison
    if(E->getLHS()->getType()->isComplexType())
      return CGF.EmitComplexRelationalExpr(Op, CGF.EmitComplexExpr(E->getLHS()),
                                           CGF.EmitComplexExpr(E->getRHS()));
    // Character comparison
    else if(E->getLHS()->getType()->isCharacterType())
      return CGF.EmitCharacterRelationalExpr(Op, CGF.EmitCharacterExpr(E->getLHS()),
                                             CGF.EmitCharacterExpr(E->getRHS()));
  }

  auto LHS = EmitExpr(E->getLHS());
  auto RHS = EmitExpr(E->getRHS());
  return CGF.EmitScalarBinaryExpr(Op, LHS, RHS);
}

llvm::Value *CodeGenFunction::EmitScalarBinaryExpr(BinaryExpr::Operator Op,
                                                   llvm::Value *LHS,
                                                   llvm::Value *RHS) {
  llvm::Value *Result;
  bool IsInt = LHS->getType()->isIntegerTy();
  switch(Op) {
  case BinaryExpr::Plus:
    Result = IsInt?  Builder.CreateAdd(LHS, RHS) :
                     Builder.CreateFAdd(LHS, RHS);
    break;
  case BinaryExpr::Minus:
    Result = IsInt?  Builder.CreateSub(LHS, RHS) :
                     Builder.CreateFSub(LHS, RHS);
    break;
  case BinaryExpr::Multiply:
    Result = IsInt?  Builder.CreateMul(LHS, RHS) :
                     Builder.CreateFMul(LHS, RHS);
    break;
  case BinaryExpr::Divide:
    Result = IsInt?  Builder.CreateSDiv(LHS, RHS) :
                     Builder.CreateFDiv(LHS, RHS);
    break;
  case BinaryExpr::Power: {
    if(IsInt)
      return EmitScalarPowIntInt(LHS, RHS);
    auto Intrinsic = llvm::Intrinsic::pow;
    if(RHS->getType()->isIntegerTy()) {
      Intrinsic = llvm::Intrinsic::powi;
      RHS = EmitIntToInt32Conversion(RHS);
    }
    auto Func = GetIntrinsicFunction(Intrinsic,
                                     LHS->getType(),
                                     RHS->getType());
    llvm::Value *PowerArgs[] = {LHS, RHS};
    Result = Builder.CreateCall(Func, PowerArgs);
    break;
  }

  default:
    return EmitScalarRelationalExpr(Op, LHS, RHS);
  }
  return Result;
}

static llvm::CmpInst::Predicate
ConvertRelationalOpToPredicate(BinaryExpr::Operator Op,
                               bool IsInt = false) {
  switch(Op) {
  case BinaryExpr::Eqv:
    return llvm::CmpInst::ICMP_EQ;
    break;
  case BinaryExpr::Neqv:
    return llvm::CmpInst::ICMP_NE;
    break;

  case BinaryExpr::Equal:
    return IsInt? llvm::CmpInst::ICMP_EQ : llvm::CmpInst::FCMP_OEQ;
  case BinaryExpr::NotEqual:
    return IsInt? llvm::CmpInst::ICMP_NE : llvm::CmpInst::FCMP_UNE;
  case BinaryExpr::LessThan:
    return IsInt? llvm::CmpInst::ICMP_SLT : llvm::CmpInst::FCMP_OLT;
  case BinaryExpr::LessThanEqual:
    return IsInt? llvm::CmpInst::ICMP_SLE : llvm::CmpInst::FCMP_OLE;
  case BinaryExpr::GreaterThan:
    return IsInt? llvm::CmpInst::ICMP_SGT : llvm::CmpInst::FCMP_OGT;
  case BinaryExpr::GreaterThanEqual:
    return IsInt? llvm::CmpInst::ICMP_SGE : llvm::CmpInst::FCMP_OGE;
  default:
    llvm_unreachable("unknown comparison op");
  }
}

llvm::Value *CodeGenFunction::EmitScalarRelationalExpr(BinaryExpr::Operator Op, llvm::Value *LHS,
                                                       llvm::Value *RHS) {
  auto IsInt = LHS->getType()->isIntegerTy();
  auto Predicate = ConvertRelationalOpToPredicate(Op, IsInt);
  if(Op == BinaryExpr::Eqv || Op == BinaryExpr::Neqv) {
    // logical comparison, need same types.
    RHS = Builder.CreateZExtOrTrunc(RHS, LHS->getType());
  }

  return IsInt? Builder.CreateICmp(Predicate, LHS, RHS) :
                Builder.CreateFCmp(Predicate, LHS, RHS);
}

llvm::Value *CodeGenFunction::EmitComplexRelationalExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                                        ComplexValueTy RHS) {
  assert(Op == BinaryExpr::Equal || Op == BinaryExpr::NotEqual);

  // x == y => x.re == y.re && x.im == y.im
  // x != y => x.re != y.re || y.im != y.im
  auto Predicate = ConvertRelationalOpToPredicate(Op);

  auto CmpRe = Builder.CreateFCmp(Predicate, LHS.Re, RHS.Re);
  auto CmpIm = Builder.CreateFCmp(Predicate, LHS.Im, RHS.Im);
  if(Op == BinaryExpr::Equal)
    return Builder.CreateAnd(CmpRe, CmpIm);
  else
    return Builder.CreateOr(CmpRe, CmpIm);
}

llvm::Value *
CodeGenFunction::ConvertComparisonResultToRelationalOp(BinaryExpr::Operator Op,
                                                       llvm::Value *Result) {
  return Builder.CreateICmp(ConvertRelationalOpToPredicate(Op, true),
                            Result,
                            Builder.getInt32(0));
}

llvm::Value *ScalarExprEmitter::VisitBinaryExprAnd(const BinaryExpr *E) {
  auto LHSTrueBlock = CGF.createBasicBlock("and-lhs-true");
  auto TrueBlock = CGF.createBasicBlock("and-true");
  auto FalseBlock = CGF.createBasicBlock("and-false");
  auto EndBlock = CGF.createBasicBlock("end-and");

  auto LHS = EmitLogicalConditionExpr(E->getLHS());
  Builder.CreateCondBr(LHS, LHSTrueBlock, FalseBlock);
  CGF.EmitBlock(LHSTrueBlock);
  auto RHS = EmitLogicalConditionExpr(E->getRHS());
  Builder.CreateCondBr(RHS, TrueBlock, FalseBlock);
  CGF.EmitBlock(TrueBlock);
  auto ResultTrue = Builder.getTrue();
  CGF.EmitBranch(EndBlock);
  CGF.EmitBlock(FalseBlock);
  auto ResultFalse = Builder.getFalse();
  CGF.EmitBlock(EndBlock);

  auto Result = Builder.CreatePHI(Builder.getInt1Ty(), 2, "and-result");
  Result->addIncoming(ResultTrue, TrueBlock);
  Result->addIncoming(ResultFalse, FalseBlock);
  return Result;
}

llvm::Value *ScalarExprEmitter::VisitBinaryExprOr(const BinaryExpr *E) {
  auto TrueBlock = CGF.createBasicBlock("or-true");
  auto LHSFalseBlock = CGF.createBasicBlock("or-lhs-false");
  auto FalseBlock = CGF.createBasicBlock("or-false");
  auto EndBlock = CGF.createBasicBlock("end-or");

  auto LHS = EmitLogicalConditionExpr(E->getLHS());
  Builder.CreateCondBr(LHS, TrueBlock, LHSFalseBlock);
  CGF.EmitBlock(LHSFalseBlock);
  auto RHS = EmitLogicalConditionExpr(E->getRHS());
  Builder.CreateCondBr(RHS, TrueBlock, FalseBlock);
  CGF.EmitBlock(FalseBlock);
  auto ResultFalse = Builder.getFalse();
  CGF.EmitBranch(EndBlock);
  CGF.EmitBlock(TrueBlock);
  auto ResultTrue = Builder.getTrue();
  CGF.EmitBlock(EndBlock);

  auto Result = Builder.CreatePHI(Builder.getInt1Ty(), 2, "or-result");
  Result->addIncoming(ResultTrue, TrueBlock);
  Result->addIncoming(ResultFalse, FalseBlock);
  return Result;
}

llvm::Value *CodeGenFunction::EmitIntToInt32Conversion(llvm::Value *Value) {
  return Builder.CreateSExtOrTrunc(Value, CGM.Int32Ty);
}

llvm::Value *CodeGenFunction::EmitSizeIntToIntConversion(llvm::Value *Value) {
  return Builder.CreateZExtOrTrunc(Value, ConvertType(getContext().IntegerTy));
}

llvm::Value *CodeGenFunction::EmitScalarToScalarConversion(llvm::Value *Value,
                                                           QualType Target) {
  auto ValueType = Value->getType();

  if(ValueType->isIntegerTy()) {
    if(Target->isIntegerType()) {
      return Builder.CreateSExtOrTrunc(Value, ConvertType(Target));
    } else {
      assert(Target->isRealType());
      return Builder.CreateSIToFP(Value, ConvertType(Target));
    }
  } else {
    assert(ValueType->isFloatingPointTy());
    if(Target->isRealType()) {
      auto TargetType = ConvertType(Target);
      return Builder.CreateFPCast(Value, TargetType);
    } else {
      assert(Target->isIntegerType());
      return Builder.CreateFPToSI(Value, ConvertType(Target));
    }
  }
  return Value;
}

llvm::Value *ScalarExprEmitter::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  auto Input = E->getExpression();
  if(Input->getType()->isComplexType())
    return CGF.EmitComplexToScalarConversion(CGF.EmitComplexExpr(Input),
                                             E->getType().getSelfOrArrayElementType());
  return CGF.EmitScalarToScalarConversion(EmitExpr(Input),
                                          E->getType().getSelfOrArrayElementType());
}

llvm::Value *ScalarExprEmitter::VisitCallExpr(const CallExpr *E) {
  return CGF.EmitCall(E).asScalar();
}

llvm::Value *ScalarExprEmitter::VisitIntrinsicCallExpr(const IntrinsicCallExpr *E) {
  return CGF.EmitIntrinsicCall(E).asScalar();
}

llvm::Value *ScalarExprEmitter::VisitArrayElementExpr(const ArrayElementExpr *E) {
  return Builder.CreateLoad(CGF.EmitArrayElementPtr(E));
}

llvm::Value *ScalarExprEmitter::VisitMemberExpr(const MemberExpr *E) {
  auto Val = CGF.EmitAggregateExpr(E->getTarget());
  return Builder.CreateLoad(CGF.EmitAggregateMember(Val.getAggregateAddr(), E->getField()),
                            Val.isVolatileQualifier());
}

llvm::Value *ScalarExprEmitter::VisitFunctionRefExpr(const FunctionRefExpr *E) {
  return CGF.EmitFunctionPointer(E->getFunctionDecl());
}

llvm::Value *CodeGenFunction::EmitFunctionPointer(const FunctionDecl *F) {
  return CGM.GetFunction(F).getFunction();
}

llvm::Value *CodeGenFunction::EmitScalarExpr(const Expr *E) {
  ScalarExprEmitter EV(*this);
  return EV.Visit(E);
}

llvm::Value *CodeGenFunction::EmitSizeIntExpr(const Expr *E) {
  auto Value = EmitScalarExpr(E);
  if(Value->getType() != CGM.SizeTy)
    return Builder.CreateSExtOrTrunc(Value, CGM.SizeTy);
  return Value;
}

llvm::Value *CodeGenFunction::EmitLogicalConditionExpr(const Expr *E) {
  ScalarExprEmitter EV(*this);
  return EV.EmitLogicalConditionExpr(E);
}

llvm::Value *CodeGenFunction::EmitLogicalValueExpr(const Expr *E) {
  ScalarExprEmitter EV(*this);
  return EV.EmitLogicalValueExpr(E);
}

llvm::Value *CodeGenFunction::GetConstantOne(QualType T) {
  auto Type = ConvertType(T);
  return Type->isIntegerTy()? llvm::ConstantInt::get(Type, 1) :
                              llvm::ConstantFP::get(Type, 1.0);
}

llvm::Value *CodeGenFunction::GetConstantZero(llvm::Type *T) {
  return T->isIntegerTy()? llvm::ConstantInt::get(T, 0) :
                           llvm::ConstantFP::get(T, 0.0);
}

llvm::Value *CodeGenFunction::GetConstantScalarMaxValue(QualType T) {
  return EmitIntrinsicNumericInquiry(intrinsic::HUGE, T, T);
}

llvm::Value *CodeGenFunction::GetConstantScalarMinValue(QualType T) {
  return EmitIntrinsicNumericInquiry(intrinsic::TINY, T, T);
}

llvm::Value *CodeGenFunction::EmitBitOperation(intrinsic::FunctionKind Op,
                                               llvm::Value *A1, llvm::Value *A2,
                                               llvm::Value *A3) {
  switch(Op) {
  case intrinsic::NOT:
    return Builder.CreateNot(A1);
  case intrinsic::IAND:
    return Builder.CreateAnd(A1, A2);
  case intrinsic::IOR:
    return Builder.CreateOr(A1, A2);
  case intrinsic::IEOR:
    return Builder.CreateXor(A1, A2);
  case intrinsic::BTEST: {
    auto Mask = Builder.CreateShl(llvm::ConstantInt::get(A1->getType(), 1),
                                  Builder.CreateZExtOrTrunc(A2, A1->getType()));
    auto Result = Builder.CreateAnd(A1, Mask);
    return Builder.CreateSelect(Builder.CreateICmpNE(Result, llvm::ConstantInt::get(A1->getType(), 0)),
                                Builder.getTrue(), Builder.getFalse());
  }
  case intrinsic::IBSET: {
    auto Mask = Builder.CreateShl(llvm::ConstantInt::get(A1->getType(), 1),
                                  Builder.CreateZExtOrTrunc(A2, A1->getType()));
    return Builder.CreateOr(A1, Mask);
  }
  case intrinsic::IBCLR:{
    auto Mask = Builder.CreateNot(
                  Builder.CreateShl(llvm::ConstantInt::get(A1->getType(), 1),
                                    Builder.CreateZExtOrTrunc(A2, A1->getType())));
    return Builder.CreateAnd(A1, Mask);
  }
  case intrinsic::IBITS: {
    auto MaskOne = llvm::ConstantInt::get(A1->getType(), 1);
    auto Mask = Builder.CreateSub(Builder.CreateShl(MaskOne,
                                  Builder.CreateZExtOrTrunc(A3, A1->getType())), MaskOne);
    return Builder.CreateAnd(Builder.CreateLShr(A1, Builder.CreateZExtOrTrunc(A2, A1->getType())),
                             Mask);
  }
  case intrinsic::ISHFT: {
    auto A1Ty = A1->getType();
    if(auto Val = dyn_cast<llvm::ConstantInt>(A2)) {
      if(Val->isNegative())
        return Builder.CreateLShr(A1,
                Builder.CreateZExtOrTrunc(Builder.CreateNeg(A2), A1Ty));
      return Builder.CreateShl(A1, Builder.CreateZExtOrTrunc(A2, A1Ty));
    }
    return Builder.CreateSelect(Builder.CreateICmpSGE(A2, llvm::ConstantInt::get(A2->getType(), 0)),
                                Builder.CreateShl(A1, Builder.CreateZExtOrTrunc(A2, A1Ty)),
                                Builder.CreateLShr(A1,
                                  Builder.CreateZExtOrTrunc(Builder.CreateNeg(A2), A1Ty)));
  }
  // FIXME: ishftc
  default:
    llvm_unreachable("Invalid bit operation!");
  }
}

}
} // end namespace flang
