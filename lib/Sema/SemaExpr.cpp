//===- SemaExpr.cpp - Expression AST Builder and Semantic Analysis Implementation -===//
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

/// Returns true if a type is a double precision real type.
bool Sema::IsTypeDoublePrecisionReal(QualType T) const {
  return T->isRealType() && Context.isTypeDoublePrecision(T);
}

/// Returns true if a type is a double precision complex type.
bool Sema::IsTypeDoublePrecisionComplex(QualType T) const {
  return T->isComplexType() && Context.isTypeDoubleComplex(T);
}

/// Returns TST_integer/TST_real/TST_complex if a given type
/// is an arithmetic type, or TST_unspecified otherwise
static TypeSpecifierType GetArithmeticTypeSpec(QualType T) {
  if(T->isIntegerType()) return TST_integer;
  else if(T->isRealType()) return TST_real;
  else if(T->isComplexType()) return TST_complex;
  else return TST_unspecified;
}

/// Returns the largest kind between two arithmetic type qualifiers.
static int GetLargestKind(const ASTContext &C,
                          const ExtQuals *A, const ExtQuals *B,
                          QualType AT, QualType BT) {
  auto KindA = AT->getBuiltinTypeKind();
  auto KindB = BT->getBuiltinTypeKind();
  return C.getTypeKindBitWidth(KindA) >= C.getTypeKindBitWidth(KindB)? 0 : 1;
}

/// Creates an implicit cast expression
static Expr *ImplicitCast(ASTContext &C, QualType T, ExprResult E) {
  return ImplicitCastExpr::Create(C, E.get()->getLocation(), T, E.take());
}

/// Selects the type with the biggest kind from two arithmetic types,
/// applies any required conversions to that type for two expressions,
/// and returns that type.
/// NB: This assumes that type A and type B have the same base type,
/// i.e. Int and Int
static QualType SelectLargestKindApplyConversions(ASTContext &C,
                                                  ExprResult &A, ExprResult &B,
                                                  QualType AType, QualType BType) {
  auto AK = AType->getBuiltinTypeKind();
  auto BK = BType->getBuiltinTypeKind();

  if(AK == BK) return AType;
  else if(C.getTypeKindBitWidth(AK) >=
          C.getTypeKindBitWidth(BK)) {
    B = ImplicitCast(C, AType, B);
    return AType;
  } else {
    A = ImplicitCast(C, BType, A);
    return BType;
  }
}

/// Chooses a type from two arithmetic types,
/// and if another type has larger kind, expands the
/// chosen type to the larger kind.
/// Applies any required conversions to the chosen type for two expressions,
/// and returns the chosen type.
static QualType TakeTypeSelectLargestKindApplyConversion(ASTContext &C,
                                                         int Chosen,
                                                         ExprResult &A, ExprResult &B,
                                                         QualType AType, QualType BType) {
  QualType ChosenType = Chosen == 0? AType : BType;  
  auto AK = AType->getBuiltinTypeKind();
  auto BK = BType->getBuiltinTypeKind();
  auto AKWidth = C.getTypeKindBitWidth(AK);
  auto BKWidth = C.getTypeKindBitWidth(BK);
  if(AK == BK ||
     (Chosen == 0 && AKWidth >= BKWidth) ||
     (Chosen != 0 && BKWidth >= AKWidth)) {
    if(Chosen == 0)
      B = ImplicitCast(C, ChosenType, B);
    else
      A = ImplicitCast(C, ChosenType, A);
    return ChosenType;
  }
  auto ReturnType = C.getQualTypeOtherKind(ChosenType, Chosen == 0? BType : AType);
  A = ImplicitCast(C, ReturnType, A);
  B = ImplicitCast(C, ReturnType, B);
  return ReturnType;
}

static QualType TypeWithKind(ASTContext &C, QualType T, QualType TKind) {
  return C.getQualTypeOtherKind(T, TKind);
}

enum TypecheckAction {
  NoAction,
  ImplicitCastAction,
  ErrorAction
};

static TypecheckAction TypecheckAssignment(ASTContext &Context,
                                           QualType LHSType, QualType RHSType) {
  TypecheckAction Result = NoAction;

  // Arithmetic assigment
  bool IsRHSInteger = RHSType->isIntegerType();
  bool IsRHSReal = RHSType->isRealType();
  bool IsRHSComplex = RHSType->isComplexType();
  bool IsRHSArithmetic = IsRHSInteger || IsRHSReal ||
                         IsRHSComplex;
  auto LHSKind = LHSType->getBuiltinTypeKind();
  auto RHSKind = RHSType->getBuiltinTypeKind();

  if(LHSType->isIntegerType()) {
    if(IsRHSInteger && LHSKind == RHSKind) ;
    else if(IsRHSArithmetic) Result = ImplicitCastAction;
    else Result = ErrorAction;
  } else if(LHSType->isRealType()) {
    if(IsRHSReal && LHSKind == RHSKind) ;
    else if(IsRHSArithmetic) Result = ImplicitCastAction;
    else Result = ErrorAction;
  } else if(LHSType->isComplexType()) {
    if(IsRHSComplex && LHSKind == RHSKind) ;
    else if(IsRHSArithmetic) Result = ImplicitCastAction;
    else Result = ErrorAction;
  }

  // Logical assignment
  else if(LHSType->isLogicalType()) {
    if(!RHSType->isLogicalType())
      Result = ErrorAction;
    else if(LHSKind != RHSKind)
      Result = ImplicitCastAction;
  }

  // Character assignment
  // FIXME: kinds
  else if(LHSType->isCharacterType()) {
    if(!RHSType->isCharacterType()) Result = ErrorAction;
  }

  else if(LHSType->isRecordType()) {
    auto LHSRec = LHSType->asRecordType();
    auto RHSRec = RHSType->asRecordType();
    if(!RHSRec) Result = ErrorAction;
    else if(LHSRec != RHSRec) {
      // FIXME: add field checks
      if(!(LHSRec->getDecl()->getIdentifier() == RHSRec->getDecl()->getIdentifier() &&
           LHSRec->getDecl()->isSequence() && RHSRec->getDecl()->isSequence()))
        Result = ErrorAction;
    }
  }

  // Invalid assignment
  else return ErrorAction;

  return Result;
}

bool Sema::DiagnoseAssignmentResult(AssignConvertType ConvTy,
                                    SourceLocation Loc,
                                    QualType DstType, QualType SrcType,
                                    const Expr *SrcExpr, AssignmentAction Action,
                                    const Expr *DstExpr) {
  unsigned Diag;
  switch(ConvTy) {
  case Compatible:
    return false;
  case Incompatible: {
    switch(Action.getType()) {
    case AssignmentAction::Assigning:
      Diag = diag::err_typecheck_assign_incompatible;
      break;
    case AssignmentAction::Initializing:
      Diag = diag::err_typecheck_initialization_incompatible;
      break;
    case AssignmentAction::Passing:
      Diag = diag::err_typecheck_passing_incompatible;
      break;
    case AssignmentAction::Returning:
      Diag = diag::err_typecheck_return_incompatible;
      break;
    default:
      llvm_unreachable("invalid assignment action");
      return true;
    }

    auto Reporter = Diags.Report(Loc, Diag);
    Reporter << SrcExpr->getSourceRange();
    if(DstExpr)
      Reporter << DstExpr->getSourceRange();
    if(Action.getType() == AssignmentAction::Assigning ||
       Action.getType() == AssignmentAction::Initializing)
      Reporter << DstType << SrcType;
    else if(Action.getType() == AssignmentAction::Returning)
      Reporter << SrcType << DstType;
  } {
    if(auto Field = dyn_cast_or_null<FieldDecl>(Action.getDecl())) {
      Diags.Report(Field->getLocation(), diag::note_typecheck_initializing_member_here)
        << Field->getIdentifier();
    }
  }
    break;
  case IncompatibleDimensions:
    break;
  }
  return true;
}

ExprResult Sema::
CheckAndApplyAssignmentConstraints(SourceLocation Loc, QualType LHSType,
                                   Expr *RHS, AssignmentAction AAction, const Expr *LHS) {

  auto RHSType = RHS->getType();
  if(LHSType->isArrayType()) {
    auto LHSElementType = LHSType->asArrayType()->getElementType();
    if(RHSType->isArrayType()) {
      auto RHSElementType = RHSType->asArrayType()->getElementType();

      // Check the array compabilities
      if(!CheckArrayDimensionsCompability(LHSType->asArrayType(),
                                          RHSType->asArrayType(),
                                          Loc,
                                          LHS? LHS->getSourceRange() : SourceRange(),
                                          RHS->getSourceRange())) {
        return ExprError();
      }

      // cast an array to appropriate type.
      auto Action = ::flang::TypecheckAssignment(Context, LHSElementType, RHSElementType);
      if(Action == NoAction)
        return RHS;
      else if(Action == ImplicitCastAction)
        return ImplicitCastExpr::Create(Context, RHS->getLocation(),
                                        Context.getArrayType(LHSElementType,
                                                             RHSType->asArrayType()->getDimensions()),
                                        RHS);
      DiagnoseAssignmentResult(Incompatible, Loc,
                               LHSElementType, RHSElementType, RHS,
                               AAction, LHS);
      return ExprError();
    }
    else
      LHSType = LHSElementType; // fallthrough
  }

  auto Action = ::flang::TypecheckAssignment(Context, LHSType, RHSType);
  if(Action == NoAction)
    return RHS;
  else if(Action == ImplicitCastAction)
    return ImplicitCastExpr::Create(Context, RHS->getLocation(),
                                    LHSType, RHS);
  DiagnoseAssignmentResult(Incompatible, Loc,
                           LHSType, RHSType, RHS,
                           AAction, LHS);
  return ExprError();
}

ExprResult Sema::ActOnComplexConstantExpr(ASTContext &C, SourceLocation Loc,
                                          SourceLocation MaxLoc,
                                          ExprResult RealPart, ExprResult ImPart) {
  QualType RealType = RealPart.get()->getType();
  QualType ImType = ImPart.get()->getType();
  QualType ElementType;

  CheckIntegerOrRealConstantExpression(RealPart.get());
  CheckIntegerOrRealConstantExpression(ImPart.get());

  if(RealType->isRealType() && ImType->isRealType()) {
    auto ReWidth = C.getTypeKindBitWidth(RealType->getBuiltinTypeKind());
    auto ImWidth = C.getTypeKindBitWidth(ImType->getBuiltinTypeKind());
    if(ReWidth > ImWidth) {
      ElementType = RealType;
      ImPart = ImplicitCast(C, ElementType, ImPart);
    } else if(ImWidth > ReWidth) {
      ElementType = ImType;
      RealPart = ImplicitCast(C, ElementType, RealPart);
    } else ElementType = RealType;
  } else if(RealType->isRealType()) {
    ElementType = RealType;
    ImPart = ImplicitCast(C, ElementType, ImPart);
  } else if(ImType->isRealType()){
    ElementType = ImType;
    RealPart = ImplicitCast(C, ElementType, RealPart);
  } else {
    ElementType = C.RealTy;
    RealPart = ImplicitCast(C, ElementType, RealPart);
    ImPart = ImplicitCast(C, ElementType, ImPart);
  }

  return ComplexConstantExpr::Create(C, SourceRange(Loc, MaxLoc),
                                     RealPart.get(), ImPart.get(),
                                     C.getComplexType(ElementType));
}

static TypecheckAction TypecheckUnaryExpr(UnaryExpr::Operator Op,
                                          QualType T, unsigned &Diagnostic) {
  switch(Op) {
  // Arithmetic unary expression
  case UnaryExpr::Plus: case UnaryExpr::Minus:
    if(!(T->isIntegerType() || T->isRealType() || T->isComplexType())) {
      Diagnostic = diag::err_typecheck_arith_unary_expr;
      return ErrorAction;
    }
    break;

  // Logical unary expression
  case UnaryExpr::Not:
    if(!T->isLogicalType()) {
      Diagnostic = diag::err_typecheck_logical_unary_expr;
      return ErrorAction;
    }
    break;

  default:
    llvm_unreachable("Unknown unary expression");
  }
  return NoAction;
}

QualType Sema::GetUnaryReturnType(const Expr *E, QualType T) {
  auto EType = E->getType();
  if(auto ATy = EType->asArrayType())
    return Context.getArrayType(T, ATy->getDimensions());
  return T;
}

ExprResult Sema::ActOnUnaryExpr(ASTContext &C, SourceLocation Loc,
                                UnaryExpr::Operator Op, ExprResult E) {
  unsigned Diagnostic = 0;

  auto EType = E.get()->getType();
  if(EType->isArrayType()) {
    CheckArrayExpr(E.get());
    EType = EType->asArrayType()->getElementType();
  }

  auto Action = TypecheckUnaryExpr(Op, EType, Diagnostic);
  if(Action == ErrorAction) {
    Diags.Report(Loc, Diagnostic)
        << EType
        << SourceRange(Loc, E.get()->getLocEnd());
    return ExprError();
  }
  return UnaryExpr::Create(C, Loc, Op, E.get());
}

// Kind selection rules:
// where typeof(x i) == Integer and typeof(x !i) : Real/Complex, Kind = Real/Complex
// where kindof(x i) == kindof(x !i), Kind = x i
// where typeof(x1 and x2) == Integer, Kind = largest kind
// where typeof(x1 and x2) == Real/Complex, Kind = largest kind

// Conversion matrix:
// LHS     RHS
//  I  | I, R, Z => I, R, Z
//  R  | I, R, Z => R, R, Z
//  Z  | I, R, Z => Z, Z, Z
//
// x1 ** x2 where x1 is real/complex and x2 is int, x2 not converted.
static void Fortran90ArithmeticBinaryTypingRules(ASTContext &C,
                                                 BinaryExpr::Operator Op,
                                                 QualType &ReturnType,
                                                 ExprResult &LHS, ExprResult &RHS,
                                                 QualType LHSType, QualType RHSType,
                                                 TypeSpecifierType LHSTypeSpec,
                                                 TypeSpecifierType RHSTypeSpec) {
  if(LHSTypeSpec == TST_integer) {
    if(RHSTypeSpec == TST_integer)
      ReturnType = SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
    else {
      ReturnType = RHSType;
      LHS = ImplicitCast(C, ReturnType, LHS);
    }
  } else {
    // LHS is real/complex
    if(RHSTypeSpec == TST_integer) {
      ReturnType = LHSType;
      // no need for conversion when ** is used.
      if(Op != BinaryExpr::Power) RHS = ImplicitCast(C, ReturnType, RHS);
    }
    else if(LHSTypeSpec == TST_real) {
      if(RHSTypeSpec == TST_real)
        ReturnType = SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
      else  // RHS is complex
        ReturnType = TakeTypeSelectLargestKindApplyConversion(C, 1, LHS, RHS,
                                                              LHSType, RHSType);
    }
    else if(LHSTypeSpec == TST_complex) {
      if(RHSTypeSpec == TST_complex)
        ReturnType = SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
      else  // RHS is real
        ReturnType = TakeTypeSelectLargestKindApplyConversion(C, 0, LHS, RHS,
                                                              LHSType, RHSType);
    }
  }
}

/// \brief Adjust the return type of an implicit cast expression,
/// so that it returns a proper array type when an array expression
/// is being given to it.
static void AdjustArrayImplicitCast(ASTContext &C, Expr *E) {
  if(auto Cast = dyn_cast<ImplicitCastExpr>(E)) {
    if(auto ATy = Cast->getExpression()->getType()->asArrayType()) {
      if(!Cast->getType()->isArrayType())
        Cast->setType(C.getArrayType(Cast->getType(), ATy->getDimensions()));
    }
  }
}

QualType Sema::GetBinaryReturnType(const Expr *LHS, const Expr *RHS,
                                   QualType T) {
  auto LHSType = LHS->getType();
  auto RHSType = RHS->getType();
  if(auto ATy = LHSType->asArrayType())
    return Context.getArrayType(T, ATy->getDimensions());
  if(auto ATy = RHSType->asArrayType())
    return Context.getArrayType(T, ATy->getDimensions());
  return T;
}

ExprResult Sema::ActOnBinaryExpr(ASTContext &C, SourceLocation Loc,
                                 BinaryExpr::Operator Op,
                                 ExprResult LHS, ExprResult RHS) {
  unsigned DiagType = 0;

  auto LHSType = LHS.get()->getType();
  auto RHSType = RHS.get()->getType();
  if(LHSType.isNull() || RHSType.isNull())
    return ExprError();
  QualType ReturnType;
  const ArrayType *ReturnArrayType = nullptr;

  if(LHSType->isArrayType()) {
    if(RHSType->isArrayType()) {
      if(!CheckArrayDimensionsCompability(LHSType->asArrayType(), RHSType->asArrayType(), Loc,
                                          LHS.get()->getSourceRange(),
                                          RHS.get()->getSourceRange()))
        return ExprError();
      RHSType = RHSType->asArrayType()->getElementType();
    } else CheckArrayExpr(LHS.get());
    ReturnArrayType = LHSType->asArrayType();
    LHSType = LHSType->asArrayType()->getElementType();
  } else if(RHSType->isArrayType()) {
    CheckArrayExpr(RHS.get());
    ReturnArrayType = RHSType->asArrayType();
    RHSType = RHSType->asArrayType()->getElementType();
  }

  switch(Op) {
  // Arithmetic binary expression
  case BinaryExpr::Plus: case BinaryExpr::Minus:
  case BinaryExpr::Multiply: case BinaryExpr::Divide:
  case BinaryExpr::Power: {
    DiagType = diag::err_typecheck_arith_invalid_operands;

    auto LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    auto RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

    if(LHSTypeSpec == TST_unspecified || RHSTypeSpec == TST_unspecified)
      goto typecheckInvalidOperands;

    if (getLangOpts().Fortran77) {
      // Fortran 77: Disallow operations between double precision and complex
      // NB: Produce warning
      if((LHSTypeSpec == TST_complex ||
          RHSTypeSpec == TST_complex)) {
        if((IsTypeDoublePrecisionReal(LHSType) && !IsTypeDoublePrecisionComplex(RHSType)) ||
           (IsTypeDoublePrecisionReal(RHSType) && !IsTypeDoublePrecisionComplex(LHSType))) {
          Diags.Report(Loc, diag::warn_f77_typecheck_arith_invalid_operands)
              << LHSType << RHSType
              << SourceRange(LHS.get()->getLocStart(),
                             RHS.get()->getLocEnd());

        }
      }
    }

    Fortran90ArithmeticBinaryTypingRules(C, Op, ReturnType, LHS, RHS,
                                         LHSType, RHSType, LHSTypeSpec, RHSTypeSpec);

    break;
  }

  // Logical binary expression
  case BinaryExpr::And: case BinaryExpr::Or:
  case BinaryExpr::Eqv: case BinaryExpr::Neqv: {
    DiagType = diag::err_typecheck_logical_invalid_operands;

    if(!LHSType->isLogicalType()) goto typecheckInvalidOperands;
    if(!RHSType->isLogicalType()) goto typecheckInvalidOperands;
    ReturnType = C.LogicalTy;
    break;
  }

  // Character binary expression
  case BinaryExpr::Concat: {
    DiagType = diag::err_typecheck_char_invalid_operands;

    if(!LHSType->isCharacterType()) goto typecheckInvalidOperands;
    if(!RHSType->isCharacterType()) goto typecheckInvalidOperands;
    ReturnType = C.CharacterTy;
    break;
  }

  // relational binary expression
  case BinaryExpr::Equal: case BinaryExpr::NotEqual:
  case BinaryExpr::GreaterThan: case BinaryExpr::GreaterThanEqual:
  case BinaryExpr::LessThan: case BinaryExpr::LessThanEqual: {
    DiagType = diag::err_typecheck_relational_invalid_operands;
    ReturnType = C.LogicalTy;

    // Character relational expression
    if(LHSType->isCharacterType() && RHSType->isCharacterType()) break;

    // Arithmetic relational expression
    auto LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    auto RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

    if(LHSTypeSpec == TST_unspecified || RHSTypeSpec == TST_unspecified)
      goto typecheckInvalidOperands;

    // A complex operand is permitted only when the relational operator is .EQ. or .NE.
    if((LHSTypeSpec == TST_complex ||
        RHSTypeSpec == TST_complex)) {
      if(Op != BinaryExpr::Equal && Op != BinaryExpr::NotEqual)
        goto typecheckInvalidOperands;

      if(getLangOpts().Fortran77) {
        // Fortran 77: The comparison of a double precision value and a complex value is not permitted.
        if(IsTypeDoublePrecisionReal(LHSType) ||
           IsTypeDoublePrecisionReal(RHSType)) {
          Diags.Report(Loc, diag::warn_f77_typecheck_relational_invalid_operands)
              << LHSType << RHSType
              << SourceRange(LHS.get()->getLocStart(),
                             RHS.get()->getLocEnd());
        }
      }
    }

    if(LHSTypeSpec == RHSTypeSpec) {
      // upcast to largest kind
      SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
    } else {
      if(LHSTypeSpec == TST_integer)
        // RHS is real/complex
        LHS = ImplicitCast(C, RHSType, LHS);
      else if(LHSTypeSpec == TST_real) {
        if(RHSTypeSpec == TST_integer)
          RHS = ImplicitCast(C, LHSType, RHS);
        else LHS = ImplicitCast(C, RHSType, LHS);
      } else {
        // lhs is complex
        // rhs is int/real
        RHS = ImplicitCast(C, LHSType, RHS);
      }
    }

    break;
  }

  default:
    llvm_unreachable("Unknown binary expression");
  }

  if(ReturnArrayType) {
    SmallVector<ArraySpec*, 8> Dims;
    for(auto I : ReturnArrayType->getDimensions())
      Dims.push_back(DeferredShapeSpec::Create(C));
    ReturnType = Context.getArrayType(ReturnType, Dims);

    AdjustArrayImplicitCast(C, LHS.get());
    AdjustArrayImplicitCast(C, RHS.get());
  }

  return BinaryExpr::Create(C, Loc, Op, ReturnType, LHS.take(), RHS.take());

typecheckInvalidOperands:
  Diags.Report(Loc,DiagType)
      << LHSType << RHSType
      << SourceRange(LHS.get()->getSourceRange())
      << SourceRange(RHS.get()->getSourceRange());
  return ExprError();
}

ExprResult Sema::ActOnSubstringExpr(ASTContext &C, SourceLocation Loc,
                                    Expr *Target,
                                    Expr *StartingPoint, Expr *EndPoint) {
  // FIXME: other substring constraints?
  if(StartingPoint && !CheckIntegerExpression(StartingPoint))
    StartingPoint = nullptr;
  if(EndPoint && !CheckIntegerExpression(EndPoint))
    EndPoint = nullptr;

  return SubstringExpr::Create(C, Loc, Target,
                               StartingPoint, EndPoint);
}

bool Sema::CheckSubscriptExprDimensionCount(SourceLocation Loc,
                                            SourceLocation RParenLoc,
                                            Expr* Target,
                                            ArrayRef<Expr*> Arguments) {
  auto AT = Target->getType()->asArrayType();
  assert(AT);
  if(AT->getDimensionCount() != Arguments.size()) {
    Diags.Report(Loc,
                 diag::err_array_subscript_dimension_count_mismatch)
      << int(AT->getDimensionCount())
      << SourceRange(Loc, RParenLoc);
    return false;
  }
  return true;
}

ExprResult Sema::ActOnSubscriptExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                                    Expr *Target, llvm::ArrayRef<Expr*> Subscripts) {
  if(Subscripts.empty())
    return ExprError();  
  CheckSubscriptExprDimensionCount(Loc, RParenLoc, Target, Subscripts);

  //FIXME constraint
  //A subscript expression may contain array element references and function references.
  //NB: typecheck only for the valid expressions
  size_t ValidCount = std::min(Target->getType()->asArrayType()->getDimensionCount(),
                               Subscripts.size());
  bool IsArrayElement = true;
  for(size_t I = 0; I < ValidCount; ++I) {
    if(auto Range = dyn_cast<RangeExpr>(Subscripts[I])) {
      IsArrayElement = false;
      if(Range->hasFirstExpr())
        CheckIntegerExpression(Range->getFirstExpr());
      if(Range->hasSecondExpr())
        CheckIntegerExpression(Range->getSecondExpr());
    }
    else if(auto Range = dyn_cast<StridedRangeExpr>(Subscripts[I])) {
      IsArrayElement = false;
      if(Range->hasFirstExpr())
        CheckIntegerExpression(Range->getFirstExpr());
      if(Range->hasSecondExpr())
        CheckIntegerExpression(Range->getSecondExpr());
      CheckIntegerExpression(Range->getStride());
    }
    else
      CheckIntegerExpression(Subscripts[I]);
  }

  if(IsArrayElement)
    return ArrayElementExpr::Create(C, Loc, Target, Subscripts);
  SmallVector<ArraySpec*, 8> Dims;
  for(size_t I = 0; I < ValidCount; ++I) {
    if(isa<RangeExpr>(Subscripts[I]) || isa<StridedRangeExpr>(Subscripts[I]))
      Dims.push_back(DeferredShapeSpec::Create(Context));
  }
  auto T = C.getArrayType(Target->getType()->asArrayType()->getElementType(),
                          Dims);
  return ArraySectionExpr::Create(C, Loc, Target, Subscripts, T);
}

bool Sema::CheckCallArguments(FunctionDecl *Function, llvm::MutableArrayRef<Expr*> Arguments,
                              SourceLocation Loc, SourceLocation IDLoc) {
  if(Function->isExternal()) {
    if(Arguments.empty()) return true;
    if(Function->getArguments().empty()) {
      // Infer external function arguments.
      llvm::SmallVector<VarDecl*, 8> Args(Arguments.size());
      for(size_t I = 0; I < Arguments.size(); ++I) {
        auto Arg = VarDecl::CreateArgument(Context, Function,
                                           Function->getLocation(), Function->getIdentifier());
        Arg->setType(Arguments[I]? Arguments[I]->getType() :
                                   Context.RealTy);
        Args[I] = Arg;
      }
      Function->setArguments(Context, Args);
    }
  }

  // check the arguments.
  // NB: Highlight as clang does:
  // Too few args, range with function name, loc - ')' location
  // Too many args, range with function name, loc and range - extra arguments.
  auto FunctionArgs = Function->getArguments();
  if(Arguments.size() != FunctionArgs.size()) {
    unsigned FuncType = /*function=*/ (Function->isSubroutine()? 2 : 1);
    if(Arguments.size() < FunctionArgs.size()) {
      Diags.Report(Loc, diag::err_typecheck_call_too_few_args)
          << FuncType << unsigned(FunctionArgs.size())
          << unsigned(Arguments.size())
          << getTokenRange(IDLoc);
    } else {
      auto LocStart = Arguments[FunctionArgs.size()]?
                        Arguments[FunctionArgs.size()]->getLocStart() : Loc;
      auto LocEnd   = Arguments.back()?
                        Arguments.back()->getLocEnd() : Loc;
      Diags.Report(LocStart, diag::err_typecheck_call_too_many_args)
          << FuncType << unsigned(FunctionArgs.size())
          << unsigned(Arguments.size())
          << getTokenRange(IDLoc) << SourceRange(LocStart, LocEnd);
    }
    return false;
  }

  // FIXME: Typecheck arguments

  for(size_t I = 0; I < Arguments.size(); ++I) {
    auto T = Arguments[I]->getType();
    if(T->isArrayType())
      Arguments[I] = ActOnArrayArgument(FunctionArgs[I], Arguments[I]);
  }

  return true;
}

static bool IsDirectArrayExpr(const Expr *E) {
  return isa<VarExpr>(E);
}

bool Sema::ArrayExprNeedsTemp(const Expr *E) {
  if(auto Section = dyn_cast<ArraySectionExpr>(E))
    return !IsDirectArrayExpr(Section->getTarget());
  if(auto Unary = dyn_cast<UnaryExpr>(E)) {
    if(Unary->getOperator() == UnaryExpr::Plus)
      return ArrayExprNeedsTemp(Unary->getExpression());
  }
  return !IsDirectArrayExpr(E);
}

/// FIXME: ':' array spec interface declared arguments don't need strides
Expr *Sema::ActOnArrayArgument(VarDecl *Arg, Expr *E) {
  if(ArrayExprNeedsTemp(E))
    return ImplicitTempArrayExpr::Create(Context, E);
  else if(!E->IsArrayExprContiguous())
    E = ImplicitArrayPackExpr::Create(Context, E);
  return E;
}

ExprResult Sema::ActOnCallExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                               SourceLocation IDLoc,
                               FunctionDecl *Function, llvm::MutableArrayRef<Expr*> Arguments) {
  assert(!Function->isSubroutine());

  CheckCallArguments(Function, Arguments, RParenLoc, IDLoc);
  return CallExpr::Create(C, Loc, Function, Arguments);
}

ExprResult Sema::ActOnIntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                                                const IntrinsicFunctionDecl *FunctionDecl,
                                                ArrayRef<Expr*> Arguments) {
  using namespace intrinsic;

  auto Function = FunctionDecl->getFunction();

  // Check argument count
  if(CheckIntrinsicCallArgumentCount(Function, Arguments, Loc))
    return ExprError();

  // Per function type checks.
  QualType ReturnType;
  switch(getFunctionGroup(Function)) {
  case GROUP_CONVERSION:
    CheckIntrinsicConversionFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_TRUNCATION:
    CheckIntrinsicTruncationFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_COMPLEX:
    CheckIntrinsicComplexFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_MATHS:
    CheckIntrinsicMathsFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_CHARACTER:
    CheckIntrinsicCharacterFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_ARRAY:
    CheckIntrinsicArrayFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_NUMERIC_INQUIRY:
    CheckIntrinsicNumericInquiryFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_SYSTEM:
    CheckIntrinsicSystemFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_INQUIRY:
    CheckIntrinsicInquiryFunc(Function, Arguments, ReturnType);
    break;

  case GROUP_BITOPS:
    CheckIntrinsicBitFunc(Function, Arguments, ReturnType);
    break;

  default:
    llvm_unreachable("invalid intrinsic function");
  }

  if(ReturnType.isNull())
    ReturnType = C.RealTy; //An error occurred.

  return IntrinsicCallExpr::Create(C, Loc, Function,
                                   Arguments, ReturnType);
}

ExprResult Sema::ActOnTypeConstructorExpr(ASTContext &C, SourceLocation Loc,
                                          SourceLocation LParenLoc, SourceLocation RParenLoc,
                                          RecordDecl *Record, ArrayRef<Expr*> Arguments) {
  SmallVector<Expr*, 8> Args;
  auto ReturnType = C.getRecordType(Record);
  auto Fields = cast<RecordType>(ReturnType.getTypePtr())->getElements();
  size_t ArgumentId = 0;
  for(size_t I = 0; I < Fields.size(); ++I, ++ArgumentId) {
    if(ArgumentId >= Arguments.size()) {
      Diags.Report(LParenLoc, diag::err_typecheck_call_too_few_args)
          << /* type constructor= */ 3
          << unsigned(Fields.size())
          << unsigned(Arguments.size())
          << getTokenRange(Loc);
      break;
    }
    auto Arg = Arguments[ArgumentId];
    auto E = CheckAndApplyAssignmentConstraints(Arg->getLocation(), Fields[I]->getType(),
                                                Arg, AssignmentAction(AssignmentAction::Initializing,
                                                                      Fields[I]));
    if(E.isUsable())
      Args.push_back(E.get());
  }

  if(ArgumentId < Arguments.size()) {
    auto LocStart = Arguments[Fields.size()]?
                      Arguments[Fields.size()]->getLocStart() : LParenLoc;
    auto LocEnd   = RParenLoc;
    Diags.Report(LocStart, diag::err_typecheck_call_too_many_args)
        << /* type constructor= */ 3
        << unsigned(Fields.size())
        << unsigned(Arguments.size())
        << getTokenRange(Loc) << SourceRange(LocStart, LocEnd);
  }

  return TypeConstructorExpr::Create(C, Loc, Record, Args);
}

ExprResult Sema::ActOnStructureComponentExpr(ASTContext &C, SourceLocation Loc,
                                             SourceLocation IDLoc,
                                             const IdentifierInfo *IDInfo,
                                             Expr *Target) {
  auto RecordTy = Target->getType().getSelfOrArrayElementType()->asRecordType();
  auto Record = RecordTy->getDecl();

  FieldDecl *Field;
  auto Result = Record->lookup(IDInfo);
  if(Result.first >= Result.second) Field = nullptr;
  else Field = dyn_cast<FieldDecl>(*Result.first);

  if(!Field) {
    Diags.Report(IDLoc, diag::err_no_member)
      << IDInfo << Target->getType().getSelfOrArrayElementType()
      << Target->getSourceRange();
    return ExprError();
  }
  return MemberExpr::Create(C, Loc, Target, Field, Field->getType());
}

} // namespace flang
