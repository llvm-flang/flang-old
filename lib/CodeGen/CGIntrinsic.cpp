//===--- CGIntrinsic.cpp - Emit LLVM Code for Intrinsic calls ------------===//
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

#include <limits>
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGSystemRuntime.h"
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

RValueTy CodeGenFunction::EmitIntrinsicCall(const IntrinsicCallExpr *E) {
  using namespace intrinsic;

  auto Func = getGenericFunctionKind(E->getIntrinsicFunction());
  auto Group = getFunctionGroup(Func);
  auto Args = E->getArguments();

  switch(Group) {
  case GROUP_CONVERSION:
    if(Func == INT ||
       Func == REAL) {
      if(Args[0]->getType()->isComplexType())
        return EmitComplexToScalarConversion(EmitComplexExpr(Args[0]),
                                             E->getType());
      else
        return EmitScalarToScalarConversion(EmitScalarExpr(Args[0]),
                                            E->getType());
    } else if(Func == CMPLX) {
      if(Args[0]->getType()->isComplexType())
        return EmitComplexToComplexConversion(EmitComplexExpr(Args[0]),
                                              E->getType());
      else {
        if(Args.size() >= 2) {
          auto ElementType = getContext().getComplexTypeElementType(E->getType());
          return ComplexValueTy(EmitScalarToScalarConversion(EmitScalarExpr(Args[0]), ElementType),
                                EmitScalarToScalarConversion(EmitScalarExpr(Args[1]), ElementType));
        }
        else return EmitScalarToComplexConversion(EmitScalarExpr(Args[0]),
                                                  E->getType());
      }
    } else if(Func == ICHAR) {
      auto Value = EmitCharacterDereference(EmitCharacterExpr(Args[0]));
      return Builder.CreateZExtOrTrunc(Value, ConvertType(getContext().IntegerTy));
    } else if(Func == CHAR) {
      auto Temp = CreateTempAlloca(CGM.Int8Ty, "char");
      auto Value = CharacterValueTy(Temp,
                                    llvm::ConstantInt::get(CGM.SizeTy, 1));
      Builder.CreateStore(Builder.CreateSExtOrTrunc(EmitScalarExpr(Args[0]), CGM.Int8Ty),
                          Value.Ptr);
      return Value;
    } else llvm_unreachable("invalid conversion intrinsic");
    break;

  case GROUP_TRUNCATION:
    return EmitIntrinsicCallScalarTruncation(Func, EmitScalarExpr(Args[0]),
                                             E->getType());

  case GROUP_COMPLEX:
    return EmitIntrinsicCallComplex(Func, EmitComplexExpr(Args[0]));

  case GROUP_MATHS:
    if(Func == MAX || Func == MIN)
      return EmitIntrinsicMinMax(Func, Args);
    if(Args[0]->getType()->isComplexType())
      return EmitIntrinsicCallComplexMath(Func, EmitComplexExpr(Args[0]));
    return EmitIntrinsicCallScalarMath(Func, EmitScalarExpr(Args[0]),
                                       Args.size() == 2?
                                        EmitScalarExpr(Args[1]) : nullptr);
  case GROUP_CHARACTER:
    if(Args.size() == 1)
      return EmitIntrinsicCallCharacter(Func, EmitCharacterExpr(Args[0]));
    else
      return EmitIntrinsicCallCharacter(Func, EmitCharacterExpr(Args[0]),
                                        EmitCharacterExpr(Args[1]));

  case GROUP_ARRAY:
    return EmitArrayIntrinsic(Func, Args);

  case GROUP_NUMERIC_INQUIRY:
    return EmitIntrinsicNumericInquiry(Func, Args[0]->getType(), E->getType());

  case GROUP_SYSTEM:
    return EmitSystemIntrinsic(Func, Args);

  case GROUP_INQUIRY: {
    int64_t Val;
    if(E->EvaluateAsInt(Val, getContext()))
      return llvm::ConstantInt::get(ConvertType(E->getType()), Val);
    return EmitInquiryIntrinsic(Func, Args);
  }

  case GROUP_BITOPS:
    return EmitBitOperation(Func, EmitScalarExpr(Args[0]),
             Args.size() > 1? EmitScalarExpr(Args[1]) : nullptr,
             Args.size() > 2? EmitScalarExpr(Args[2]) : nullptr);

  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }

  return RValueTy();
}

llvm::Value *CodeGenFunction::EmitIntrinsicCallScalarTruncation(intrinsic::FunctionKind Func,
                                                                llvm::Value *Value,
                                                                QualType ResultType) {
  llvm::Value *FuncDecl = nullptr;
  auto ValueType = Value->getType();
  switch(Func) {
  case intrinsic::AINT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::trunc, ValueType);
    break;
  case intrinsic::ANINT:
  case intrinsic::NINT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::rint, ValueType);
    break;
  case intrinsic::CEILING:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::ceil, ValueType);
    break;
  case intrinsic::FLOOR:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::floor, ValueType);
    break;
  }

  auto Result = Builder.CreateCall(FuncDecl, Value);
  if(ResultType->isIntegerType())
    return EmitScalarToScalarConversion(Result, ResultType);
  return Result;
}

#define MANGLE_MATH_FUNCTION(Str, Type) \
  ((Type)->isFloatTy() ? Str "f" : Str)

llvm::Value* CodeGenFunction::EmitIntrinsicCallScalarMath(intrinsic::FunctionKind Func,
                                                          llvm::Value *A1, llvm::Value *A2) {
  using namespace intrinsic;

  llvm::Value *FuncDecl = nullptr;
  auto ValueType = A1->getType();
  switch(Func) {
  case ABS:
    if(ValueType->isIntegerTy()) {
      auto Condition = Builder.CreateICmpSGE(A1, llvm::ConstantInt::get(ValueType, 0));
      return Builder.CreateSelect(Condition, A1, Builder.CreateNeg(A1));
    }
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::fabs, ValueType);
    break;

  case MOD:
    if(ValueType->isIntegerTy())
      return Builder.CreateSRem(A1, A2);
    else
      return Builder.CreateFRem(A1, A2);
    break;

  // |a1|  if a2 >= 0
  // -|a1| if a2 < 0
  case SIGN: {
    auto A1Abs = EmitIntrinsicCallScalarMath(ABS, A1);
    auto Cond = EmitScalarRelationalExpr(BinaryExpr::GreaterThanEqual,
                                         A2, GetConstantZero(A2->getType()));
    return Builder.CreateSelect(Cond, A1Abs, EmitScalarUnaryMinus(A1Abs));
    break;
  }

  //a1-a2 if a1>a2
  //  0   if a1<=a2
  case DIM: {
    auto Cond = EmitScalarRelationalExpr(BinaryExpr::GreaterThan,
                                         A1, A2);
    return Builder.CreateSelect(Cond,
                                EmitScalarBinaryExpr(BinaryExpr::Minus,
                                                     A1, A2),
                                GetConstantZero(A1->getType()));
    break;
  }

  case DPROD: {
    auto TargetType = getContext().DoublePrecisionTy;
    return Builder.CreateFMul(EmitScalarToScalarConversion(A1, TargetType),
                              EmitScalarToScalarConversion(A2, TargetType));
  }

  case SQRT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::sqrt, ValueType);
    break;
  case EXP:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::exp, ValueType);
    break;
  case LOG:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::log, ValueType);
    break;
  case LOG10:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::log10, ValueType);
    break;
  case SIN:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::sin, ValueType);
    break;
  case COS:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::cos, ValueType);
    break;
  case TAN:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("tan", ValueType),
                                ValueType, ValueType);
    break;
  case ASIN:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("asin", ValueType),
                                ValueType, ValueType);
    break;
  case ACOS:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("acos", ValueType),
                                ValueType, ValueType);
    break;
  case ATAN:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("atan", ValueType),
                                ValueType, ValueType);
    break;
  case ATAN2: {
    llvm::Type *Args[] = {ValueType, ValueType};
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("atan2", ValueType),
                                llvm::makeArrayRef(Args, 2),
                                ValueType);
    break;
  }
  case SINH:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("sinh", ValueType),
                                ValueType, ValueType);
    break;
  case COSH:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("cosh", ValueType),
                                ValueType, ValueType);
    break;
  case TANH:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("tanh", ValueType),
                                ValueType, ValueType);
    break;
  default:
    llvm_unreachable("invalid scalar math intrinsic");
  }
  if(A2)
    return Builder.CreateCall2(FuncDecl, A1, A2);
  return Builder.CreateCall(FuncDecl, A1);
}

llvm::Value *CodeGenFunction::EmitIntrinsicMinMax(intrinsic::FunctionKind Func,
                                                  ArrayRef<Expr*> Arguments) {
  SmallVector<llvm::Value*, 8> Args(Arguments.size());
  for(size_t I = 0; I < Arguments.size(); ++I)
    Args[I] = EmitScalarExpr(Arguments[I]);
  return EmitIntrinsicScalarMinMax(Func, Args);
}

llvm::Value *CodeGenFunction::EmitIntrinsicScalarMinMax(intrinsic::FunctionKind Func,
                                                        ArrayRef<llvm::Value*> Args) {
  auto Value = Args[0];
  auto Op = Func == intrinsic::MAX? BinaryExpr::GreaterThanEqual :
                                    BinaryExpr::LessThanEqual;
  for(size_t I = 1; I < Args.size(); ++I)
    Value = Builder.CreateSelect(EmitScalarRelationalExpr(Op,
                                 Value, Args[I]), Value, Args[I]);
  return Value;
}

// Lets pretend ** is an intrinsic
llvm::Value *CodeGenFunction::EmitScalarPowIntInt(llvm::Value *LHS, llvm::Value *RHS) {
  auto T = cast<llvm::IntegerType>(LHS->getType());
  StringRef FuncName;
  switch(T->getBitWidth()) {
  case 8:
    FuncName = "pow_i1_i1"; break;
  case 16:
    FuncName = "pow_i2_i2"; break;
  case 32:
    FuncName = "pow_i4_i4"; break;
  case 64:
    FuncName = "pow_i8_i8"; break;
  default:
    llvm_unreachable("unsupported integer type");
  }
  auto Func = CGM.GetRuntimeFunction2(FuncName, T, T, T);
  CallArgList Args;
  Args.add(LHS);
  Args.add(RHS);
  return EmitCall(Func.getFunction(), Func.getInfo(), Args).asScalar();
}

ComplexValueTy CodeGenFunction::EmitComplexPowi(ComplexValueTy LHS, llvm::Value *RHS) {
  auto ElementType = LHS.Re->getType();
  auto ValueType = getTypes().GetComplexType(ElementType);
  auto ResultType =  llvm::PointerType::get(ValueType, 0);
  auto Func = CGM.GetRuntimeFunction4(MANGLE_MATH_FUNCTION("cpowi", ElementType),
                                      ElementType, ElementType, RHS->getType(), ResultType);
  CallArgList Args;
  Args.add(LHS.Re);Args.add(LHS.Im);Args.add(RHS);
  auto Result = CreateTempAlloca(ValueType,"libflang_complex_result");
  Args.add(Result);
  EmitCall(Func.getFunction(), Func.getInfo(), Args);
  return EmitComplexLoad(Result);
}

ComplexValueTy CodeGenFunction::EmitComplexPow(ComplexValueTy LHS, ComplexValueTy RHS) {
  auto ElementType = LHS.Re->getType();
  auto ValueType = getTypes().GetComplexType(ElementType);
  auto ResultType =  llvm::PointerType::get(ValueType, 0);
  auto Func = CGM.GetRuntimeFunction5(MANGLE_MATH_FUNCTION("cpow", ElementType),
                                      ElementType, ElementType, ElementType, ElementType, ResultType);
  CallArgList Args;
  Args.add(LHS.Re);Args.add(LHS.Im);
  Args.add(RHS.Re);Args.add(RHS.Im);
  auto Result = CreateTempAlloca(ValueType,"libflang_complex_result");
  Args.add(Result);
  EmitCall(Func.getFunction(), Func.getInfo(), Args);
  return EmitComplexLoad(Result);
}

RValueTy CodeGenFunction::EmitIntrinsicCallComplexMath(intrinsic::FunctionKind Function,
                                                       ComplexValueTy Value) {
  auto ElementType = Value.Re->getType();
  auto ValueType = getTypes().GetComplexType(ElementType);
  auto ResultType =  llvm::PointerType::get(ValueType, 0);
  CGFunction Func;
  CGType Arg1Types[] = { ElementType, ElementType, ResultType };
  ArrayRef<CGType> Arg1(Arg1Types, 3);

  switch(Function) {
  case intrinsic::ABS:
    Func = CGM.GetRuntimeFunction2(MANGLE_MATH_FUNCTION("cabs", ElementType),
                                   ElementType, ElementType, ElementType);
    break;
  case intrinsic::SQRT:
    Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("csqrt", ElementType),
                                  Arg1);
    break;
  case intrinsic::EXP:
    Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("cexp", ElementType),
                                  Arg1);
    break;
  case intrinsic::LOG:
    Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("clog", ElementType),
                                  Arg1);
    break;
  case intrinsic::SIN:
    Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("csin", ElementType),
                                  Arg1);
    break;
  case intrinsic::COS:
    Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("ccos", ElementType),
                                  Arg1);
    break;
  case intrinsic::TAN:
    Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("ctan", ElementType),
                                  Arg1);
    break;
  default:
    llvm_unreachable("invalid complex math intrinsic");
  }

  CallArgList Args;
  Args.add(Value.Re);Args.add(Value.Im);
  if(Function != intrinsic::ABS) {
    auto Result = CreateTempAlloca(ValueType, "libflang_complex_result");
    Args.add(Result);
    EmitCall(Func.getFunction(), Func.getInfo(), Args);
    return EmitComplexLoad(Result);
  }

  return EmitCall(Func.getFunction(), Func.getInfo(), Args);
}

llvm::Value *CodeGenFunction::EmitIntrinsicNumericInquiry(intrinsic::FunctionKind Func,
                                                          QualType ArgType, QualType Result) {
  using namespace intrinsic;
  using namespace std;

  auto RetT = ConvertType(Result);
  auto T = ArgType;
  auto TKind = T->getBuiltinTypeKind();
  int IntResult;

#define HANDLE_INT(Result, func) \
    switch(TKind) {  \
    case BuiltinType::Int1: \
      Result = numeric_limits<int8_t>::func; break; \
    case BuiltinType::Int2: \
      Result = numeric_limits<int16_t>::func; break; \
    case BuiltinType::Int4: \
      Result = numeric_limits<int32_t>::func; break; \
    case BuiltinType::Int8: \
      Result = numeric_limits<int64_t>::func; break; \
    default: \
      llvm_unreachable("invalid type kind"); \
      break; \
    }

#define HANDLE_REAL(Result, func) \
    switch(TKind) {  \
    case BuiltinType::Real4: \
      Result = numeric_limits<float>::func; break; \
    case BuiltinType::Real8: \
      Result = numeric_limits<double>::func; break; \
    default: \
      llvm_unreachable("invalid type kind"); \
      break; \
    }

  // FIXME: the float numeric limit is being implicitly converted into a double here..
#define HANDLE_REAL_RET_REAL(func) \
    switch(TKind) {  \
    case BuiltinType::Real4: \
      return llvm::ConstantFP::get(RetT, numeric_limits<float>::func()); \
      break; \
    case BuiltinType::Real8: \
      return llvm::ConstantFP::get(RetT, numeric_limits<double>::func()); \
      break; \
    default: \
      llvm_unreachable("invalid type kind"); \
      break; \
    }

  if(T->isIntegerType()) {
    switch(Func) {
    case RADIX:
      HANDLE_INT(IntResult, radix);
      break;
    case DIGITS:
      HANDLE_INT(IntResult, digits);
      break;
    case RANGE:
      HANDLE_INT(IntResult, digits10);
      break;
    case HUGE: {
      int64_t i64;
      HANDLE_INT(i64, max());
      return llvm::ConstantInt::get(RetT, i64, true);
      break;
    }
    case TINY: {
      int64_t i64;
      HANDLE_INT(i64, min());
      return llvm::ConstantInt::get(RetT, i64, true);
      break;
    }
    default:
      llvm_unreachable("Invalid integer inquiry intrinsic");
    }
  } else {
    switch(Func) {
    case RADIX:
      HANDLE_REAL(IntResult, radix);
      break;
    case DIGITS:
      HANDLE_REAL(IntResult, digits);
      break;
    case MINEXPONENT:
      HANDLE_REAL(IntResult, min_exponent);
      break;
    case MAXEXPONENT:
      HANDLE_REAL(IntResult, max_exponent);
      break;
    case PRECISION:
      HANDLE_REAL(IntResult, digits10);
      break;
    case RANGE:
      HANDLE_REAL(IntResult, min_exponent10);
      IntResult = abs(IntResult);
      break;
    case HUGE:
      HANDLE_REAL_RET_REAL(max);
      break;
    case TINY:
      HANDLE_REAL_RET_REAL(min);
      break;
    case EPSILON:
      HANDLE_REAL_RET_REAL(epsilon);
      break;
    }
  }

#undef HANDLE_INT
#undef HANDLE_REAL

  return llvm::ConstantInt::get(RetT, IntResult, true);
}

RValueTy CodeGenFunction::EmitSystemIntrinsic(intrinsic::FunctionKind Func,
                                              ArrayRef<Expr*> Arguments) {
  using namespace intrinsic;

  switch(Func) {
  case ETIME:
    return CGM.getSystemRuntime().EmitETIME(*this, Arguments);

  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }

  return RValueTy();
}

llvm::Value *CodeGenFunction::EmitInquiryIntrinsic(intrinsic::FunctionKind Func,
                                                   ArrayRef<Expr*> Arguments) {
  using namespace intrinsic;

  switch(Func) {
  case SELECTED_INT_KIND: {
    auto Func = CGM.GetRuntimeFunction1("selected_int_kind", CGM.Int32Ty, CGM.Int32Ty);
    CallArgList Args;
    Args.add(Builder.CreateSExtOrTrunc(EmitScalarExpr(Arguments[0]), CGM.Int32Ty));
    return EmitCall(Func.getFunction(), Func.getInfo(), Args).asScalar();
  }
  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }

  return nullptr;
}

}
} // end namespace flang
