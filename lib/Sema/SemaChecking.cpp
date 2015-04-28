//===--- SemaChecking.cpp - Extra Semantic Checking -----------------------===//
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
#include "flang/AST/ExprVisitor.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

int64_t Sema::EvalAndCheckIntExpr(const Expr *E,
                                  int64_t ErrorValue) {
  auto Type = E->getType();
  if(Type.isNull() || !Type->isIntegerType())
    goto error;
  int64_t Result;
  if(!E->EvaluateAsInt(Result, Context))
    goto error;

  return Result;
error:
  Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
    << E->getSourceRange();
  return ErrorValue;
}

int64_t Sema::CheckIntGT0(const Expr *E, int64_t EvalResult,
                          int64_t ErrorValue) {
  if(EvalResult < 1) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_gt_0)
      << E->getSourceRange();
    return ErrorValue;
  }
  return EvalResult;
}

BuiltinType::TypeKind Sema::EvalAndCheckTypeKind(QualType T,
                                                 const Expr *E) {
  auto Result = EvalAndCheckIntExpr(E, 4);
  switch(cast<BuiltinType>(T.getTypePtr())->getTypeSpec()) {
  case BuiltinType::Integer:
  case BuiltinType::Logical: {
    switch(Result) {
    case 1: return BuiltinType::Int1;
    case 2: return BuiltinType::Int2;
    case 4: return BuiltinType::Int4;
    case 8: return BuiltinType::Int8;
    }
    break;
  }
  case BuiltinType::Real:
  case BuiltinType::Complex: {
    switch(Result) {
    case 4: return BuiltinType::Real4;
    case 8: return BuiltinType::Real8;
    }
    break;
  }
  default:
    llvm_unreachable("invalid type kind");
  }
  Diags.Report(E->getLocation(), diag::err_invalid_kind_selector)
    << int(Result) << T << E->getSourceRange();
  return BuiltinType::NoKind;
}

QualType Sema::ApplyTypeKind(QualType T, const Expr *E) {
  auto Kind = EvalAndCheckTypeKind(T, E);
  return Kind != BuiltinType::NoKind?
        QualType(Context.getBuiltinType(T->asBuiltinType()->getTypeSpec(),
                   Kind, true), 0) : T;
}

bool Sema::CheckConstantExpression(const Expr *E) {
  if(!E->isEvaluatable(Context)) {
    Diags.Report(E->getLocation(),
                 diag::err_expected_constant_expr)
      << E->getSourceRange();
    return false;
  }
  return true;
}

bool Sema::CheckIntegerOrRealConstantExpression(const Expr *E) {
  auto T = E->getType();
  if(!(T->isIntegerType() || T->isRealType()) ||
     !E->isEvaluatable(Context)) {
    Diags.Report(E->getLocation(),
                 diag::err_expected_integer_or_real_constant_expr)
      << E->getSourceRange();
    return false;
  }
  return false;
}

class ArgumentDependentExprChecker : public ExprVisitor<ArgumentDependentExprChecker> {
public:
  ASTContext &Context;
  Sema &Sem;
  DiagnosticsEngine &Diags;
  bool HasArgumentTypeErrors;
  bool CheckEvaluatable;
  bool ArgumentDependent;
  bool Evaluatable;

  ArgumentDependentExprChecker(ASTContext &C, Sema &S,
                               DiagnosticsEngine &Diag)
    : Context(C), Sem(S), Diags(Diag),
      HasArgumentTypeErrors(false),
      CheckEvaluatable(false), ArgumentDependent(false), Evaluatable(true) {}

  void VisitVarExpr(VarExpr *E) {
    auto VD = E->getVarDecl();
    if(VD->isArgument()) {
      ArgumentDependent = true;
      if(!CheckEvaluatable) {
        if(VD->getType().isNull()) {
          if(!Sem.ApplyImplicitRulesToArgument(const_cast<VarDecl*>(VD),
                                               E->getSourceRange())) {
            HasArgumentTypeErrors = true;
            return;
          }
          E->setType(VD->getType());
        }
        if(!VD->getType()->isIntegerType()) {
          Diags.Report(E->getLocation(), diag::err_array_explicit_shape_requires_int_arg)
            << VD->getType() << E->getSourceRange();
          HasArgumentTypeErrors = true;
        }
      }
    } else
      VisitExpr(E);
  }

  void VisitBinaryExpr(BinaryExpr *E) {
    Visit(E->getLHS());
    Visit(E->getRHS());
  }
  void VisitUnaryExpr(UnaryExpr *E) {
    Visit(E->getExpression());
  }
  void VisitImplicitCastExpr(ImplicitCastExpr *E) {
    Visit(E->getExpression());
  }
  void VisitExpr(Expr *E) {
    if(!E->isEvaluatable(Context)) {
      Evaluatable = false;
      if(CheckEvaluatable) {
        Diags.Report(E->getLocation(),
                     diag::err_expected_constant_expr)
          << E->getSourceRange();
      }
    }
  }
};

bool Sema::CheckArgumentDependentEvaluatableIntegerExpression(Expr *E) {
  ArgumentDependentExprChecker EV(Context, *this, Diags);
  EV.Visit(E);
  if(EV.ArgumentDependent) {
    /// Report unevaluatable errors.
    if(!EV.Evaluatable) {
      EV.CheckEvaluatable = true;
      EV.Visit(E);
    } else if(!EV.HasArgumentTypeErrors)
      CheckIntegerExpression(E);
    return true;
  }
  return false;
}

bool Sema::StatementRequiresConstantExpression(SourceLocation Loc, const Expr *E) {
  if(!E->isEvaluatable(Context)) {
    Diags.Report(Loc,
                 diag::err_stmt_requires_consant_expr)
      << E->getSourceRange();
    return false;
  }
  return true;
}

bool Sema::CheckIntegerExpression(const Expr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_integer_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresIntegerExpression(SourceLocation Loc, const Expr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_int_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::CheckScalarNumericExpression(const Expr *E) {
  if(E->getType()->isIntegerType() ||
     E->getType()->isRealType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_expected_scalar_numeric_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresIntegerVar(SourceLocation Loc, const VarExpr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(Loc,
               diag::err_typecheck_stmt_requires_int_var)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresScalarNumericVar(SourceLocation Loc, const VarExpr *E, unsigned DiagId) {
  if(E->getType()->isIntegerType() ||
     E->getType()->isRealType())
    return true;
  Diags.Report(Loc, DiagId)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::CheckLogicalExpression(const Expr *E) {
  if(E->getType()->isLogicalType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_logical_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresLogicalExpression(SourceLocation Loc, const Expr *E) {
  if(E->getType()->isLogicalType())
    return true;
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_logical_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresLogicalArrayExpression(SourceLocation Loc, const Expr *E) {
  auto T = E->getType();
  if(T->isArrayType()) {
    T = T->asArrayType()->getElementType();
    if(T->isLogicalType())
      return true;
  }
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_logical_array_expr)
    << T << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresIntegerOrLogicalOrCharacterExpression(SourceLocation Loc, const Expr *E) {
  auto Type = E->getType();
  if(Type->isIntegerType() || Type->isLogicalType() || Type->isCharacterType())
    return true;
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_int_logical_char_expr)
    << Type << E->getSourceRange();
  return false;
}

bool Sema::CheckCharacterExpression(const Expr *E) {
  if(E->getType()->isCharacterType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_char_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::AreTypesOfSameKind(QualType A, QualType B) const {
  if(A->isCharacterType())
    return B->isCharacterType();
  if(auto ABTy = dyn_cast<BuiltinType>(A.getTypePtr())) {
    auto BBTy = dyn_cast<BuiltinType>(B.getTypePtr());
    if(!BBTy) return false;    
    auto Spec = ABTy->getTypeSpec();
    if(Spec != BBTy->getTypeSpec()) return false;
    return ABTy->getBuiltinTypeKind() == BBTy->getBuiltinTypeKind();
  }
  return false;
}

bool Sema::CheckTypesOfSameKind(QualType A, QualType B,
                                const Expr *E) const {
  if(AreTypesOfSameKind(A,B)) return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_expr_of_type)
    << A << B << E->getSourceRange();
  return false;
}

bool Sema::CheckTypeScalarOrCharacter(const Expr *E, QualType T, bool IsConstant) {
  if(T->isBuiltinType() || T->isCharacterType()) return true;
  Diags.Report(E->getLocation(), IsConstant?
                 diag::err_expected_scalar_or_character_constant_expr :
                 diag::err_expected_scalar_or_character_expr)
    << E->getSourceRange();
  return false;
}

Expr *Sema::TypecheckExprIntegerOrLogicalOrSameCharacter(Expr *E,
                                                         QualType ExpectedType) {
  auto GivenType = E->getType();
  if(ExpectedType->isIntegerType()) {
    if(CheckIntegerExpression(E)) {
      if(!AreTypesOfSameKind(ExpectedType, GivenType))
        return ImplicitCastExpr::Create(Context, E->getLocation(), ExpectedType, E);
    }
  }
  else if(ExpectedType->isLogicalType()) {
    if(CheckLogicalExpression(E)) {
      if(!AreTypesOfSameKind(ExpectedType, GivenType))
        return ImplicitCastExpr::Create(Context, E->getLocation(), ExpectedType, E);
    }
  } else {
    assert(ExpectedType->isCharacterType());
    if(!GivenType->isCharacterType() || !AreTypesOfSameKind(ExpectedType, GivenType)) {
      Diags.Report(E->getLocation(),
                   diag::err_typecheck_expected_char_expr)
        << E->getType() << E->getSourceRange();
    }
  }
  return E;
}

bool Sema::IsDefaultBuiltinOrDoublePrecisionType(QualType T) {
  if(T->isCharacterType())
    return true;
  if(!T->isBuiltinType())
    return false;
  auto BTy = T->asBuiltinType();
  if(BTy->isDoublePrecisionKindSpecified() ||
     !BTy->isKindExplicitlySpecified())
    return true;
  return false;
}

bool Sema::CheckDefaultBuiltinOrDoublePrecisionExpression(const Expr *E) {
  if(IsDefaultBuiltinOrDoublePrecisionType(E->getType()))
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_default_kind_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

void Sema::CheckExpressionListSameTypeKind(ArrayRef<Expr*> Expressions, bool AllowArrays) {
  assert(!Expressions.empty());
  auto T = Expressions.front()->getType();
  for(size_t I = 0; I < Expressions.size(); ++I) {
    auto E = Expressions[I];
    if(!AreTypesOfSameKind(T, E->getType())) {
      Diags.Report(E->getLocation(), 0) // FIXME
        << E->getSourceRange();
    }
  }
}

static QualType ScalarizeType(const QualType &T, bool AllowArrays) {
  if(AllowArrays)
    return T.getSelfOrArrayElementType();
  return T;
}

static const BuiltinType *getBuiltinType(const Expr *E, bool AllowArrays = false) {
  return dyn_cast<BuiltinType>(ScalarizeType(E->getType(), AllowArrays).getTypePtr());
}

static const BuiltinType *getBuiltinType(QualType T) {
  return dyn_cast<BuiltinType>(T.getTypePtr());
}

bool Sema::DiagnoseIncompatiblePassing(const Expr *E, QualType T,
                                       bool AllowArrays,
                                       StringRef ArgName) {
  QualType ET = AllowArrays? E->getType().getSelfOrArrayElementType() :
                             E->getType();
  if(ArgName.empty())
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << ET << T << E->getSourceRange();
  else
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible_named_arg)
      << ET << ArgName << T << E->getSourceRange();
  return true;
}

bool Sema::DiagnoseIncompatiblePassing(const Expr *E, StringRef T,
                                       bool AllowArrays,
                                       StringRef ArgName) {
  QualType ET = AllowArrays? E->getType().getSelfOrArrayElementType() :
                             E->getType();
  if(ArgName.empty())
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << ET << T << E->getSourceRange();
  else
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible_named_arg)
      << ET << ArgName << T << E->getSourceRange();
  return true;
}

bool Sema::CheckArgumentsTypeCompability(const Expr *E1, const Expr *E2,
                                         StringRef ArgName1, StringRef ArgName2,
                                         bool AllowArrays) {
  // assume builtin type
  auto T1 = ScalarizeType(E1->getType(), AllowArrays);
  auto T2 = ScalarizeType(E2->getType(), AllowArrays);

  if(T1->isCharacterType()) {
    if(T2->isCharacterType())
      return false;
  } else {
    auto Type1 = getBuiltinType(E1, AllowArrays);
    auto Type2 = getBuiltinType(E2, AllowArrays);
    if(!(!Type2 || Type1->getTypeSpec() != Type2->getTypeSpec() ||
       Type1->getBuiltinTypeKind() != Type2->getBuiltinTypeKind()))
      return false;
  }

  Diags.Report(E2->getLocation(), diag::err_typecheck_arg_conflict_type)
   << ArgName1 << ArgName2
   << T1 << T2
   << E1->getSourceRange() << E2->getSourceRange();
  return true;
}

bool Sema::CheckBuiltinTypeArgument(const Expr *E, bool AllowArrays) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type)
    return DiagnoseIncompatiblePassing(E, "'intrinsic type'", AllowArrays);
  return false;
}

bool Sema::CheckIntegerArgument(const Expr *E, bool AllowArrays, StringRef ArgName) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type || !Type->isIntegerType())
    return DiagnoseIncompatiblePassing(E, Context.IntegerTy, AllowArrays, ArgName);
  return false;
}

bool Sema::CheckRealArgument(const Expr *E, bool AllowArrays) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type || !Type->isRealType())
    return DiagnoseIncompatiblePassing(E, Context.RealTy, AllowArrays);
  return false;
}

bool Sema::CheckComplexArgument(const Expr *E, bool AllowArrays) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type || !Type->isComplexType())
    return DiagnoseIncompatiblePassing(E, Context.ComplexTy, AllowArrays);
  return false;
}

bool Sema::CheckStrictlyRealArgument(const Expr *E, bool AllowArrays) {
  auto T = ScalarizeType(E->getType(), AllowArrays);
  if(!T->isRealType() || IsTypeDoublePrecisionReal(T))
    return DiagnoseIncompatiblePassing(E, Context.RealTy, AllowArrays);
  return false;
}

bool Sema::CheckStrictlyRealArrayArgument(const Expr *E, StringRef ArgName) {
  auto T = E->getType()->asArrayType();
  if(T) {
    auto ET = T->getElementType();
    if(ET->isRealType() && !IsTypeDoublePrecisionReal(ET))
      return false;
  }
  return DiagnoseIncompatiblePassing(E, "'real array'", false, ArgName);
}

bool Sema::CheckDoublePrecisionRealArgument(const Expr *E, bool AllowArrays) {
  if(!IsTypeDoublePrecisionReal(ScalarizeType(E->getType(), AllowArrays)))
    return DiagnoseIncompatiblePassing(E, Context.DoublePrecisionTy, AllowArrays);
  return false;
}

bool Sema::CheckDoubleComplexArgument(const Expr *E, bool AllowArrays) {
  if(!IsTypeDoublePrecisionComplex(ScalarizeType(E->getType(), AllowArrays)))
    return DiagnoseIncompatiblePassing(E, Context.DoubleComplexTy, AllowArrays);
  return false;
}

bool Sema::CheckCharacterArgument(const Expr *E) {
  if(!E->getType()->isCharacterType())
    return DiagnoseIncompatiblePassing(E, Context.CharacterTy, false);
  return false;
}

bool Sema::CheckIntegerOrRealArgument(const Expr *E, bool AllowArrays) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type || !Type->isIntegerOrRealType())
    return DiagnoseIncompatiblePassing(E, "'integer' or 'real'", AllowArrays);
  return false;
}

bool Sema::CheckIntegerOrRealArrayArgument(const Expr *E, StringRef ArgName) {
  auto T = E->getType()->asArrayType();
  if(T) {
    auto Element = getBuiltinType(T->getElementType());
    if(Element && Element->isIntegerOrRealType())
      return false;
  }

  Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible_named_arg)
    << E->getType() << ArgName << "'integer array' or 'real array'"
    << E->getSourceRange();
  return true;
}

bool Sema::CheckIntegerOrRealOrComplexArgument(const Expr *E, bool AllowArrays) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type || !Type->isIntegerOrRealOrComplexType())
    return DiagnoseIncompatiblePassing(E, "'integer' or 'real' or 'complex'", AllowArrays);
  return false;
}

bool Sema::CheckRealOrComplexArgument(const Expr *E, bool AllowArrays) {
  auto Type = getBuiltinType(E, AllowArrays);
  if(!Type || !Type->isRealOrComplexType())
    return DiagnoseIncompatiblePassing(E, "'real' or 'complex'", AllowArrays);
  return false;
}

bool Sema::IsLogicalArray(const Expr *E) {
  auto T = E->getType()->asArrayType();
  if(T) {
    auto Element = getBuiltinType(T->getElementType());
    if(Element && Element->isLogicalType())
      return true;
  }
  return false;
}

bool Sema::CheckLogicalArrayArgument(const Expr *E, StringRef ArgName) {
  if(IsLogicalArray(E)) return false;

  Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible_named_arg)
    << E->getType() << ArgName << "'logical array'"
    << E->getSourceRange();
  return true;
}

bool Sema::CheckIntegerArgumentOrLogicalArrayArgument(const Expr *E, StringRef ArgName1,
                                                      StringRef ArgName2) {
  if(E->getType()->isIntegerType() || IsLogicalArray(E))
    return false;

  Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible_named_args)
    << E->getType() << ArgName1 << "'integer'"
    << ArgName2 << "'logical array'"
    << E->getSourceRange();
  return true;
}

bool Sema::CheckArrayNoImpliedDimension(const ArrayType *T,
                                        SourceRange Range) {
  if(isa<ImpliedShapeSpec>(T->getDimensions().back())) {
    Diags.Report(Range.Start, diag::err_typecheck_use_of_implied_shape_array)
      << Range;
    return false;
  }
  return true;
}

bool Sema::CheckArrayExpr(const Expr *E) {
  return CheckArrayNoImpliedDimension(E->getType()->asArrayType(), E->getSourceRange());
}

// FIXME: ARR(:) = ARR(:)

template<typename T>
static bool CheckArrayCompability(Sema &S, T& Handler,
                                  const ArrayType *LHS,
                                  const ArrayType *RHS,
                                  SourceLocation Loc,
                                  SourceRange LHSRange,
                                  SourceRange RHSRange) {
  if(LHS->getDimensionCount() !=
     RHS->getDimensionCount()) {
    Handler.DimensionCountMismatch(Loc, LHS->getDimensionCount(),
                                   RHS->getDimensionCount(),
                                   LHSRange, RHSRange);
    return false;
  }

  bool Valid = S.CheckArrayNoImpliedDimension(LHS, LHSRange);
  if(!S.CheckArrayNoImpliedDimension(RHS, RHSRange))
    Valid = false;
  if(!Valid)
    return false;

  for(size_t I = 0, Count = LHS->getDimensionCount(); I < Count; ++I) {
    auto LHSDim = LHS->getDimensions()[I];
    auto RHSDim = RHS->getDimensions()[I];
    EvaluatedArraySpec LHSSpec, RHSSpec;
    if(LHSDim->Evaluate(LHSSpec, S.getContext())) {
      if(RHSDim->Evaluate(RHSSpec, S.getContext())) {
        auto LHSSize = LHSSpec.Size;
        auto RHSSize = RHSSpec.Size;
        if(LHSSize != RHSSize) {
          Handler.DimensionSizeMismatch(Loc, LHSSize, I+1, RHSSize,
                                        LHSRange, RHSRange);
          return false;
        }
      }
    }
  }

  return true;
}

bool Sema::CheckArrayDimensionsCompability(const ArrayType *LHS,
                                           const ArrayType *RHS,
                                           SourceLocation Loc,
                                           SourceRange LHSRange,
                                           SourceRange RHSRange) {
  struct Handler {
    DiagnosticsEngine &Diags;

    Handler(DiagnosticsEngine &D) : Diags(D) {}

    void DimensionCountMismatch(SourceLocation Loc, int LHSDims,
                                int RHSDims, SourceRange LHSRange,
                                SourceRange RHSRange) {
      Diags.Report(Loc, diag::err_typecheck_expected_array_of_dim_count)
        << LHSDims << RHSDims
        << LHSRange << RHSRange;
    }

    void DimensionSizeMismatch(SourceLocation Loc, int64_t LHSSize,
                               int Dim, int64_t RHSSize, SourceRange LHSRange,
                               SourceRange RHSRange) {
      Diags.Report(Loc, diag::err_typecheck_array_dim_shape_mismatch)
       << Dim << int(LHSSize) << int(RHSSize)
       << LHSRange << RHSRange;
    }
  };

  Handler Reporter(Diags);
  return CheckArrayCompability(*this, Reporter, LHS, RHS, Loc, LHSRange, RHSRange);
}

bool Sema::CheckArrayArgumentDimensionCompability(const Expr *E, const ArrayType *AT,
                                                  StringRef ArgName) {
  auto AT1 = E->getType()->asArrayType();

  struct Handler {
    DiagnosticsEngine &Diags;
    StringRef ArgName;

    Handler(DiagnosticsEngine &D,
            StringRef Name1)
      : Diags(D), ArgName(Name1) {}

    void DimensionCountMismatch(SourceLocation Loc, int LHSDims,
                                int RHSDims, SourceRange LHSRange,
                                SourceRange RHSRange) {
      Diags.Report(Loc, diag::err_typecheck_arg_conflict_array_dim_count)
        << LHSDims << RHSDims
        << ArgName << LHSRange;
    }

    void DimensionSizeMismatch(SourceLocation Loc, int64_t LHSSize,
                               int Dim, int64_t RHSSize, SourceRange LHSRange,
                               SourceRange RHSRange) {
      Diags.Report(Loc, diag::err_typecheck_arg_conflict_array_dim_size)
       << Dim << int(LHSSize)  << int(RHSSize)
       << ArgName << LHSRange;
    }
  };

  Handler Reporter(Diags, ArgName);
  return CheckArrayCompability(*this, Reporter, AT1, AT, E->getLocation(),
                               E->getSourceRange(), SourceRange());
}

bool Sema::CheckArrayArgumentsDimensionCompability(const Expr *E1, const Expr *E2,
                                                   StringRef ArgName1, StringRef ArgName2) {
  auto AT1 = E1->getType()->asArrayType();
  auto AT2 = E2->getType()->asArrayType();
  if(!AT1 || !AT2) return true;

  struct Handler {
    DiagnosticsEngine &Diags;
    StringRef A1, A2;

    Handler(DiagnosticsEngine &D,
            StringRef Name1, StringRef Name2)
      : Diags(D), A1(Name1), A2(Name2) {}

    void DimensionCountMismatch(SourceLocation Loc, int LHSDims,
                                int RHSDims, SourceRange LHSRange,
                                SourceRange RHSRange) {
      Diags.Report(Loc, diag::err_typecheck_args_conflict_array_dim_count)
        << LHSDims << RHSDims
        << A1 << A2
        << LHSRange << RHSRange;
    }

    void DimensionSizeMismatch(SourceLocation Loc, int64_t LHSSize,
                               int Dim, int64_t RHSSize, SourceRange LHSRange,
                               SourceRange RHSRange) {
      Diags.Report(Loc, diag::err_typecheck_args_conflict_array_dim_size)
       << Dim << int(LHSSize)  << int(RHSSize)
       << A1 << A2
       << LHSRange << RHSRange;
    }
  };

  Handler Reporter(Diags, ArgName1, ArgName2);
  return CheckArrayCompability(*this, Reporter, AT1, AT2, E2->getLocation(),
                               E1->getSourceRange(), E2->getSourceRange());
}

bool Sema::CheckVarIsAssignable(const VarExpr *E) {
  for(auto I : CurLoopVars) {
    if(I->getVarDecl() == E->getVarDecl()) {
      Diags.Report(E->getLocation(), diag::err_var_not_assignable)
        << E->getVarDecl()->getIdentifier() << E->getSourceRange();
      Diags.Report(I->getLocation(), diag::note_var_prev_do_use)
        << I->getSourceRange();
      return false;
    }
  }
  return true;
}

bool Sema::CheckRecursiveFunction(SourceLocation Loc) {
  auto Function = cast<FunctionDecl>(CurContext);
  if(!Function->isRecursive()) {
    Diags.Report(Loc, diag::err_call_non_recursive)
      << (Function->isSubroutine()? 1: 0)
      << Function->getIdentifier()
      << getTokenRange(Loc);
    return false;
  }
  return true;
}

} // end namespace flang
