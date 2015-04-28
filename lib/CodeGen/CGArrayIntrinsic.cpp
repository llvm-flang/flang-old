//===--- CGArrayIntrinsics.cpp - Emit LLVM Code for Array intrinsics -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Array intrinsic functions.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGArray.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/AST/StmtVisitor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace flang {
namespace CodeGen {

RValueTy CodeGenFunction::
EmitVectorDimReturningScalarArrayIntrinsic(intrinsic::FunctionKind Func,
                                           Expr *Arr) {
  using namespace intrinsic;
  auto ElementType = Arr->getType()->asArrayType()->getElementType();

  switch(Func) {
  case MAXLOC:
  case MINLOC: {
    auto Result = CreateTempAlloca(CGM.SizeTy, "maxminloc-result");
    auto MaxMinVar = CreateTempAlloca(ConvertTypeForMem(ElementType), "maxminloc-var");
    Builder.CreateStore(llvm::ConstantInt::get(CGM.SizeTy, 0), Result);
    if(Func == MAXLOC)
      Builder.CreateStore(GetConstantScalarMinValue(ElementType), MaxMinVar);
    else
      Builder.CreateStore(GetConstantScalarMaxValue(ElementType), MaxMinVar);

    ArrayOperation OP;
    StandaloneArrayValueSectionGatherer Gatherer(*this, OP);
    Gatherer.EmitExpr(Arr);
    OP.EmitAllScalarValuesAndArraySections(*this, Arr);
    ArrayLoopEmitter Looper(*this);
    Looper.EmitArrayIterationBegin(Gatherer.getResult());
    ArrayOperationEmitter EV(*this, OP, Looper);

    auto ElementValue = EV.Emit(Arr).asScalar();
    auto ResultValue = Builder.CreateLoad(MaxMinVar);
    auto ThenBlock = createBasicBlock("maxminloc-then");
    auto EndBlock = createBasicBlock("maxminloc-end");
    Builder.CreateCondBr(EmitScalarBinaryExpr(Func == MAXLOC? BinaryExpr::GreaterThan : BinaryExpr::LessThan,
                                              ElementValue, ResultValue),
                         ThenBlock, EndBlock);
    EmitBlock(ThenBlock);
    Builder.CreateStore(ElementValue, MaxMinVar);
    Builder.CreateStore(Looper.EmitElementOneDimensionalIndex(Gatherer.getResult()), Result);
    EmitBranch(EndBlock);
    EmitBlock(EndBlock);

    Looper.EmitArrayIterationEnd();
    return EmitScalarToScalarConversion(Builder.CreateLoad(Result),
                                        getContext().IntegerTy);
  }

  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }
  return RValueTy();
}

RValueTy CodeGenFunction::EmitArrayIntrinsic(intrinsic::FunctionKind Func,
                                             ArrayRef<Expr*> Arguments) {
  using namespace intrinsic;

  switch(Func) {
  case MAXLOC:
  case MINLOC:
    if(Arguments.size() == 2 &&
       Arguments[0]->getType()->asArrayType()->getDimensionCount() == 1 &&
       Arguments[1]->getType()->isIntegerType()) {
      // Vector, dim -> return scalar
      return EmitVectorDimReturningScalarArrayIntrinsic(Func,
                                                        Arguments.front());
    }
    llvm_unreachable("FIXME: add codegen for the rest");
    break;

  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }

  return RValueTy();
}

}
} // end namespace flang
