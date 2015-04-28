//===--- SemaIntrinsic.cpp - Intrinsic call Semantic Checking -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

using namespace intrinsic;

bool Sema::CheckIntrinsicCallArgumentCount(intrinsic::FunctionKind Function,
                                           ArrayRef<Expr*> Args,
                                           SourceLocation Loc) {
  unsigned ArgCountDiag = 0;
  int ExpectedCount = 0;
  const char *ExpectedString = nullptr;

  switch(getFunctionArgumentCount(Function)) {
  case ArgumentCount1:
    ExpectedCount = 1;
    if(Args.size() < 1)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 1)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount2:
    ExpectedCount = 2;
    if(Args.size() < 2)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 2)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount3:
    ExpectedCount = 3;
    if(Args.size() < 3)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 3)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount1or2:
    ExpectedString = "1 or 2";
    if(Args.size() < 1)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 2)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount2orMore:
    ExpectedCount = 2;
    if(Args.size() < 2)
      ArgCountDiag = diag::err_typecheck_call_too_few_args_at_least;
    break;
  default:
    llvm_unreachable("invalid arg count");
  }
  if(ArgCountDiag) {
    auto Reporter = Diags.Report(Loc, ArgCountDiag)
                      << /*intrinsic function=*/ 0;
    if(ExpectedString)
      Reporter << ExpectedString;
    else
      Reporter << ExpectedCount;
    Reporter << unsigned(Args.size());
    return true;
  }
  return false;
}

static QualType ApplyKind(Sema &S, QualType T, const Expr *E) {

}

// FIXME: add support for kind parameter
bool Sema::CheckIntrinsicConversionFunc(intrinsic::FunctionKind Function,
                                        ArrayRef<Expr*> Args,
                                        QualType &ReturnType) {
  const Expr *Item = Args[0];
  const Expr *Kind = nullptr;

  if(Function == INT || Function == REAL) {
    if(Args.size() >= 2)
      Kind = Args[1];
  } else if(Function == CMPLX) {
    if(Args.size() >= 2) {
      if(Item->getType().getSelfOrArrayElementType()->isComplexType())
        Kind = Args[1];
      else if(Args.size() == 3) {
        Kind = Args[2];
      }
    }
  }
  if(Kind) {
    if(CheckIntegerArgument(Kind, false, "kind"))
      Kind = nullptr;
  }

  switch(Function) {
  case INT: case IFIX: case IDINT:
    if(Function == IFIX) CheckStrictlyRealArgument(Item, true);
    else if(Function == IDINT) CheckDoublePrecisionRealArgument(Item, true);
    else CheckIntegerOrRealOrComplexArgument(Item, true);
    ReturnType = GetUnaryReturnType(Item, Kind? ApplyTypeKind(Context.IntegerTy, Kind) :
                                                Context.IntegerTy);
    break;

  case REAL: case FLOAT: case SNGL: {
    if(Function == FLOAT) CheckIntegerArgument(Item , true);
    else if(Function == SNGL) CheckDoublePrecisionRealArgument(Item , true);
    else CheckIntegerOrRealOrComplexArgument(Item , true);

    auto ItemType = Item->getType().getSelfOrArrayElementType();
    if(!Kind && ItemType->isComplexType()) {
      ReturnType = GetUnaryReturnType(Item, Context.getComplexTypeElementType(ItemType));
      break;
    }
    ReturnType = GetUnaryReturnType(Item, Kind? ApplyTypeKind(Context.RealTy, Kind) :
                                                Context.RealTy);
    break;
  }
  case DBLE:
    CheckIntegerOrRealOrComplexArgument(Item , true);
    ReturnType = GetUnaryReturnType(Item , Context.DoublePrecisionTy);
    break;

  case CMPLX:
  case DCMPLX:
    CheckIntegerOrRealOrComplexArgument(Item , true);
    if(Args.size() > 1) {
      if(!Item->getType().getSelfOrArrayElementType()->isComplexType()) {
        CheckIntegerOrRealArgument(Args[1], true);
        CheckArrayArgumentsDimensionCompability(Item, Args[1], "x", "y");
      }
    }
    ReturnType = GetUnaryReturnType(Item, Kind? ApplyTypeKind(Context.ComplexTy, Kind) :
                                     (Function == CMPLX? Context.ComplexTy :
                                                         Context.DoubleComplexTy));
    break;

    // FIXME: array and kind support
  case ICHAR:
    CheckCharacterArgument(Item);
    ReturnType = Context.IntegerTy;
    break;

  case CHAR:
    CheckIntegerArgument(Item);
    ReturnType = Context.CharacterTy;
    break;
  }
  return false;
}

bool Sema::CheckIntrinsicTruncationFunc(intrinsic::FunctionKind Function,
                                        ArrayRef<Expr*> Args,
                                        QualType &ReturnType) {
  auto Arg = Args[0];
  auto GenericFunction = getGenericFunctionKind(Function);

  if(GenericFunction != Function)
    CheckDoublePrecisionRealArgument(Arg, true);
  else CheckRealArgument(Arg, true);

  switch(GenericFunction) {
  case AINT:
  case ANINT:
    ReturnType = Arg->getType();
    break;
  case NINT:
  case CEILING:
  case FLOOR:
    ReturnType = GetUnaryReturnType(Arg, Context.IntegerTy);
    break;
  }
  return false;
}

bool Sema::CheckIntrinsicComplexFunc(intrinsic::FunctionKind Function,
                                     ArrayRef<Expr*> Args,
                                     QualType &ReturnType) {
  auto Arg = Args[0];
  auto GenericFunction = getGenericFunctionKind(Function);

  if(GenericFunction != Function) {
    if(CheckDoubleComplexArgument(Arg, true)) return true;
  } else {
    if(CheckComplexArgument(Arg, true)) return true;
  }

  switch(GenericFunction) {
  case AIMAG:
    ReturnType = GetUnaryReturnType(Arg,
                   Context.getComplexTypeElementType(Arg->getType().getSelfOrArrayElementType()));
    break;
  case CONJG:
    ReturnType = Arg->getType();
    break;
  }
  return false;
}

static QualType TypeWithKind(ASTContext &C, QualType T, QualType TKind) {
  return C.getQualTypeOtherKind(T, TKind);
}

bool Sema::CheckIntrinsicMathsFunc(intrinsic::FunctionKind Function,
                                   ArrayRef<Expr*> Args,
                                   QualType &ReturnType) {
  auto FirstArg = Args[0];
  auto SecondArg = Args.size() > 1? Args[1] : nullptr;
  auto GenericFunction = getGenericFunctionKind(Function);

  switch(GenericFunction) {
  case ABS:
    if(GenericFunction != Function) {
      switch(Function) {
      case IABS: CheckIntegerArgument(FirstArg, true); break;
      case DABS: CheckDoublePrecisionRealArgument(FirstArg, true); break;
      case CABS: CheckComplexArgument(FirstArg, true); break;
      case CDABS:
        CheckDoubleComplexArgument(FirstArg, true);
        ReturnType = GetUnaryReturnType(FirstArg, Context.DoublePrecisionTy);
        return false;
      }
    }
    else CheckIntegerOrRealOrComplexArgument(FirstArg, true);
    if(FirstArg->getType()->isComplexType()) {
      ReturnType = GetUnaryReturnType(FirstArg, TypeWithKind(Context, Context.RealTy,
                                      FirstArg->getType()));
    } else
      ReturnType = FirstArg->getType();
    break;

  // 2 integer/real/complex
  case MOD:
  case SIGN:
  case DIM:
  case ATAN2:
    if(GenericFunction != Function) {
      switch(Function) {
      case ISIGN: case IDIM:
        CheckIntegerArgument(FirstArg, true);
        break;
      case AMOD:
        CheckRealArgument(FirstArg, true);
        break;
      case DMOD: case DSIGN: case DDIM:
      case DATAN2:
        CheckDoublePrecisionRealArgument(FirstArg, true);
        break;
      }
    }
    else {
      if(GenericFunction == ATAN2)
        CheckRealArgument(FirstArg, true);
     else
        CheckIntegerOrRealArgument(FirstArg, true);
    }
    CheckArgumentsTypeCompability(FirstArg, SecondArg, "x", "y", true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "x", "y");
    ReturnType = FirstArg->getType(); // FIXME: Binary type
    break;

  case DPROD:
    CheckStrictlyRealArgument(FirstArg, true);
    CheckStrictlyRealArgument(SecondArg, true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "x", "y");
    ReturnType = GetBinaryReturnType(FirstArg, SecondArg, Context.DoublePrecisionTy);
    break;

  case MAX:
  case MIN:
    if(GenericFunction != Function) {
      //FIXME
      bool Failed = false;
      for(size_t I = 0; I < Args.size(); ++I) {
        switch(Function) {
        case MAX0: case MIN0: case AMIN0:
          if(CheckIntegerArgument(Args[I]))
            Failed = true;
          break;
        case AMAX1: case AMIN1: case MIN1:
          if(CheckStrictlyRealArgument(Args[I]))
            Failed = true;
          break;
        case DMAX1: case DMIN1:
          if(CheckDoublePrecisionRealArgument(Args[I]))
            Failed = true;
          break;
        }
      }
      if(!Failed)
        CheckExpressionListSameTypeKind(Args);
      //FIXME AMIN0 returns -> Real
      //FIXME MIN1  returns -> Integer ???
    } else {
      bool Failed = false;
      for(size_t I = 0; I < Args.size(); ++I) {
        if(CheckIntegerOrRealArgument(Args[I]))
          Failed = true;
      }
      if(!Failed)
        CheckExpressionListSameTypeKind(Args);
    }
    ReturnType = FirstArg->getType();
    break;

  // 1 real/double/complex
  case SQRT:
  case EXP:
  case LOG:
  case SIN:
  case COS:
  case TAN:
    if(GenericFunction != Function) {
      switch(Function) {
      case ALOG:
        if(CheckStrictlyRealArgument(FirstArg, true))
          return true;
        break;
      case DSQRT: case DEXP: case DLOG:
      case DSIN: case DCOS: case DTAN:
        if(CheckDoublePrecisionRealArgument(FirstArg, true))
          return true;
        break;
      case CSQRT: case CEXP: case CLOG:
      case CSIN:  case CCOS: case CTAN:
        if(CheckComplexArgument(FirstArg, true))
          return true;
        break;
      }
    }
    else if(CheckRealOrComplexArgument(FirstArg, true))
      return true;
    ReturnType = FirstArg->getType();
    break;

  // 1 real/double
  case LOG10:
  case ASIN:
  case ACOS:
  case ATAN:
  case SINH:
  case COSH:
  case TANH:
    if(GenericFunction != Function) {
      switch(Function) {
      case ALOG10:
        if(CheckStrictlyRealArgument(FirstArg, true))
          return true;
        break;
      default:
        if(CheckDoublePrecisionRealArgument(FirstArg, true))
          return true;
      }
    }
    else if(CheckRealArgument(FirstArg, true))
      return true;
    ReturnType = FirstArg->getType();
    break;

  }
  return false;
}

bool Sema::CheckIntrinsicCharacterFunc(intrinsic::FunctionKind Function,
                                       ArrayRef<Expr*> Args,
                                       QualType &ReturnType) {
  auto FirstArg = Args[0];
  auto SecondArg = Args.size() > 1? Args[1] : nullptr;

  CheckCharacterArgument(FirstArg);
  if(SecondArg) {
    if(!CheckCharacterArgument(SecondArg))
      CheckExpressionListSameTypeKind(Args);
  }

  switch(Function) {
  case LEN:
  case LEN_TRIM:
  case INDEX:
    ReturnType = Context.IntegerTy;
    break;

  case LGE: case LGT: case LLE: case LLT:
    ReturnType = Context.LogicalTy;
    break;
  }
  return false;
}

bool Sema::CheckIntrinsicArrayFunc(intrinsic::FunctionKind Function,
                                   ArrayRef<Expr*> Args,
                                   QualType &ReturnType) {
  auto FirstArg = Args[0];
  auto SecondArg = Args.size() > 1? Args[1] : nullptr;
  auto ThirdArg = Args.size() > 2? Args[2] : nullptr;
  size_t ArrayDimCount = 0;

  switch(Function) {
  case MAXLOC:
  case MINLOC:
    // FIXME: Optional DIM (when array has more than one dim)
    // FIXME: Third argument mask

    if(!CheckIntegerOrRealArrayArgument(FirstArg, "array")) {
      ArrayDimCount = FirstArg->getType()->asArrayType()->getDimensionCount();
      ReturnType = GetSingleDimArrayType(Context.IntegerTy, ArrayDimCount);
    } else ReturnType = Context.IntegerTy;
    if(SecondArg) {

      if(SecondArg->getType()->isIntegerType()) {
        if(ArrayDimCount == 1) {
          ReturnType = Context.IntegerTy;
        } else {
          // Result: array with a dimension
          assert(false && "FIXME");
        }
      }

      else if(IsLogicalArray(SecondArg))
        CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg,
                                                "array", "mask");
      else {
        if(ArrayDimCount == 1) {
          ReturnType = Context.IntegerTy;
        }
        CheckIntegerArgumentOrLogicalArrayArgument(SecondArg, "dim", "mask");
      }
    }

    break;
  }

  return false;
}

bool Sema::CheckIntrinsicNumericInquiryFunc(intrinsic::FunctionKind Function,
                                            ArrayRef<Expr*> Args,
                                            QualType &ReturnType) {
  auto Arg = Args[0];
  ReturnType = Context.IntegerTy;

  switch(Function) {
  case RADIX:
  case DIGITS:
    CheckIntegerOrRealArgument(Arg);
    break;
  case MINEXPONENT:
  case MAXEXPONENT:
    CheckRealArgument(Arg);
    break;
  case PRECISION:
    CheckRealOrComplexArgument(Arg);
    break;
  case RANGE:
    CheckIntegerOrRealOrComplexArgument(Arg);
    break;
  case intrinsic::HUGE:
  case TINY:
    if(!CheckIntegerOrRealArgument(Arg))
      ReturnType = Arg->getType();
    break;
  case EPSILON:
    if(!CheckRealArgument(Arg))
      ReturnType = Arg->getType();
    break;
  }

  return false;
}

bool Sema::CheckIntrinsicSystemFunc(intrinsic::FunctionKind Function,
                                    ArrayRef<Expr*> Args,
                                    QualType &ReturnType) {
  auto FirstArg = Args[0];

  switch(Function) {
  case ETIME:
    if(!CheckStrictlyRealArrayArgument(FirstArg, "tarray"))
      CheckArrayArgumentDimensionCompability(FirstArg,
                                             GetSingleDimArrayType(Context.RealTy, 2)->asArrayType(),
                                             "tarray");
    ReturnType = Context.RealTy;
    break;
  }

  return false;
}

bool Sema::CheckIntrinsicInquiryFunc(intrinsic::FunctionKind Function,
                                     ArrayRef<Expr*> Args,
                                     QualType &ReturnType) {
  switch(Function) {
  case KIND:
    CheckBuiltinTypeArgument(Args[0], true);
    ReturnType = Context.IntegerTy;
    break;
  case SELECTED_INT_KIND:
    CheckIntegerArgument(Args[0]);
    ReturnType = Context.IntegerTy;
    break;
  case SELECTED_REAL_KIND:
    CheckIntegerArgument(Args[0]);
    if(Args.size() > 1)
      CheckIntegerArgument(Args[1]);
    ReturnType = Context.IntegerTy;
    break;
  case BIT_SIZE:
    if(CheckIntegerArgument(Args[0], true))
      ReturnType = Context.IntegerTy;
    else {
      auto Kind = Args[0]->getType().getSelfOrArrayElementType();
      ReturnType = Context.getQualTypeOtherKind(Context.IntegerTy, Kind);
    }
    break;
  }
  return false;
}

// FIXME: apply constant arguments constraints (e.g. 0 .. BIT_SIZE range)
bool Sema::CheckIntrinsicBitFunc(intrinsic::FunctionKind Function,
                                 ArrayRef<Expr*> Args,
                                 QualType &ReturnType) {
  if(Function == NOT) {
    CheckIntegerArgument(Args[0], true);
    ReturnType = Args[0]->getType();
    return false;
  }

  auto FirstArg = Args[0];
  auto SecondArg = Args[1];
  if(CheckIntegerArgument(FirstArg,true)) {
    ReturnType = Context.IntegerTy;
    return true;
  }

  switch(Function) {
  case BTEST:
    CheckIntegerArgument(SecondArg,true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "i", "pos");
    ReturnType = GetBinaryReturnType(FirstArg, SecondArg, Context.LogicalTy);
    break;
  case IBCLR:
  case IBSET:
    CheckIntegerArgument(SecondArg,true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "i", "pos");
    ReturnType = FirstArg->getType();
    break;
  case IBITS: {
    auto ThirdArg = Args[2];
    CheckIntegerArgument(SecondArg, true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "i", "pos");
    CheckIntegerArgument(ThirdArg, true);
    CheckArrayArgumentsDimensionCompability(FirstArg, ThirdArg, "i", "len");
    ReturnType = FirstArg->getType();
    break;
  }
  case ISHFT:
  case ISHFTC: // FIXME: optional size
    CheckIntegerArgument(SecondArg, true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "i", "shift");
    ReturnType = FirstArg->getType();
    break;
  case IAND:
  case IEOR:
  case IOR:
    CheckArgumentsTypeCompability(FirstArg, SecondArg, "i", "j", true);
    CheckArrayArgumentsDimensionCompability(FirstArg, SecondArg, "i", "j");
    ReturnType = FirstArg->getType();
    break;
  }

  return false;
}

} // end namespace flang
