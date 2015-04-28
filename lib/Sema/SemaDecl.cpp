//===- SemaDecl.cpp - Declaration AST Builder and Semantic Analysis -------===//
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
#include "flang/Sema/SemaInternal.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"

namespace flang {

bool Sema::ActOnAttrSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::AS Val) {
  if (DS.hasAttributeSpec(Val)) {
    Diags.Report(Loc, diag::err_duplicate_attr_spec)
      << DeclSpec::getSpecifierName(Val);
    return true;
  }
  DS.setAttributeSpec(Val);
  return false;
}

bool Sema::ActOnDimensionAttrSpec(ASTContext &C, SourceLocation Loc,
                                  DeclSpec &DS,
                                  ArrayRef<ArraySpec*> Dimensions) {
  if (DS.hasAttributeSpec(DeclSpec::AS_dimension)) {
    Diags.Report(Loc, diag::err_duplicate_attr_spec)
      << DeclSpec::getSpecifierName(DeclSpec::AS_dimension);
    return true;
  }
  DS.setAttributeSpec(DeclSpec::AS_dimension);
  DS.setDimensions(Dimensions);
  return false;
}

bool Sema::ActOnAccessSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::AC Val) {
  if (DS.hasAccessSpec(Val)) {
    Diags.Report(Loc, diag::err_duplicate_access_spec)
      << DeclSpec::getSpecifierName(Val);
    return true;
  }
  DS.setAccessSpec(Val);
  return false;
}

bool Sema::ActOnIntentSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::IS Val) {
  if (DS.hasIntentSpec(Val)) {
    Diags.Report(Loc, diag::err_duplicate_intent_spec)
      << DeclSpec::getSpecifierName(Val);
    return true;
  }
  DS.setIntentSpec(Val);
  return false;
}

bool Sema::ActOnObjectArraySpec(ASTContext &C, SourceLocation Loc,
                                DeclSpec &DS,
                                ArrayRef<ArraySpec*> Dimensions) {
  if(!DS.hasAttributeSpec(DeclSpec::AS_dimension))
    DS.setAttributeSpec(DeclSpec::AS_dimension);
  DS.setDimensions(Dimensions);
  return false;
}

void Sema::ActOnTypeDeclSpec(ASTContext &C, SourceLocation Loc,
                             const IdentifierInfo *IDInfo, DeclSpec &DS) {
  DS.SetTypeSpecType(DeclSpec::TST_struct);
  auto D = ResolveIdentifier(IDInfo);
  if(!D) {
    Diags.Report(Loc, diag::err_undeclared_var_use)
      << IDInfo;
    return;
  }
  auto Record = dyn_cast<RecordDecl>(D);
  if(!Record) {
    Diags.Report(Loc, diag::err_use_of_not_typename)
      << IDInfo;
    return;
  }
  DS.setRecord(Record);
}

void Sema::DiagnoseRedefinition(SourceLocation Loc, const IdentifierInfo *IDInfo,
                                Decl *Prev) {
  Diags.Report(Loc, diag::err_redefinition) << IDInfo;
  Diags.Report(Prev->getLocation(), diag::note_previous_definition);
}

bool Sema::CheckDeclaration(const IdentifierInfo *IDInfo, SourceLocation IDLoc) {
  if(!IDInfo) return false;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    DiagnoseRedefinition(IDLoc, IDInfo, Prev);
    return false;
  }
  return true;
}

//
// Type construction
//

uint64_t Sema::EvalAndCheckCharacterLength(const Expr *E) {
  return CheckIntGT0(E, EvalAndCheckIntExpr(E, 1));
}

/// \brief Convert the specified DeclSpec to the appropriate type object.
QualType Sema::ActOnTypeName(ASTContext &C, DeclSpec &DS) {
  QualType Result;
  switch (DS.getTypeSpecType()) {
  case DeclSpec::TST_integer:
    Result = C.IntegerTy;
    break;
  case DeclSpec::TST_unspecified: // FIXME: Correct?
  case DeclSpec::TST_real:
    Result = C.RealTy;
    break;
  case DeclSpec::TST_character:
    if(DS.isStarLengthSelector())
      Result = C.NoLengthCharacterTy;
    else if(DS.hasLengthSelector())
      Result = QualType(C.getCharacterType(
                          EvalAndCheckCharacterLength(DS.getLengthSelector())), 0);
    else Result = C.CharacterTy;
    break;
  case DeclSpec::TST_logical:
    Result = C.LogicalTy;
    break;
  case DeclSpec::TST_complex:
    Result = C.ComplexTy;
    break;
  case DeclSpec::TST_struct:
    if(!DS.getRecord())
      Result = C.RealTy;
    else
      Result = C.getRecordType(DS.getRecord());
    break;
  }

  Type::TypeKind Kind = Type::NoKind;
  if(DS.hasKindSelector())
    Kind = EvalAndCheckTypeKind(Result, DS.getKindSelector());

  if(Kind != Type::NoKind || DS.isDoublePrecision()) {
    switch (DS.getTypeSpecType()) {
    case DeclSpec::TST_integer:
      Result = Kind == Type::NoKind? C.IntegerTy :
                         QualType(C.getBuiltinType(BuiltinType::Integer, Kind, true), 0);
      break;
    case DeclSpec::TST_real:
      Result = Kind == Type::NoKind? (DS.isDoublePrecision()? C.DoublePrecisionTy : C.RealTy) :
                         QualType(C.getBuiltinType(BuiltinType::Real, Kind, true), 0);
      break;
    case DeclSpec::TST_logical:
      Result = Kind == Type::NoKind? C.LogicalTy :
                         QualType(C.getBuiltinType(BuiltinType::Logical, Kind, true), 0);
      break;
    case DeclSpec::TST_complex:
      Result = Kind == Type::NoKind? (DS.isDoublePrecision()? C.DoubleComplexTy : C.ComplexTy) :
                         QualType(C.getBuiltinType(BuiltinType::Complex, Kind, true), 0);
      break;
    default:
      break;
    }
  }

  if (!DS.hasAttributes())
    return Result;

  const Type *TypeNode = Result.getTypePtr();
  Qualifiers Quals = Qualifiers::fromOpaqueValue(DS.getAttributeSpecs());
  Quals.setIntentAttr(DS.getIntentSpec());
  Quals.setAccessAttr(DS.getAccessSpec());

  Result = C.getExtQualType(TypeNode, Quals);

  if (!Quals.hasAttributeSpec(Qualifiers::AS_dimension))
    return Result;

  return ActOnArraySpec(C, Result, DS.getDimensions());
}


//
// Entity declarations.
//

VarDecl *Sema::CreateImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                        const IdentifierInfo *IDInfo) {
  auto Type = ResolveImplicitType(IDInfo);
  if(Type.isNull()) {
    Diags.Report(IDLoc, diag::err_undeclared_var_use)
      << IDInfo;
    return nullptr;
  }
  auto VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, Type);
  // FIXME: type checks?
  VD->setTypeImplicit(true);
  CurContext->addDecl(VD);
  return VD;
}

static Qualifiers getDeclQualifiers(const Decl *D) {
  if(auto Value = dyn_cast<ValueDecl>(D))
    return Value->getType().split().second;
  return Qualifiers();
}

static QualType getTypeWithAttribute(ASTContext &C, QualType T, Qualifiers::AS AS) {
  auto Split = T.split();
  if(Split.second.hasAttributeSpec(AS))
    return T;
  QualifierCollector QC(Split.second);
  QC.addAttributeSpecs(AS);
  return C.getTypeWithQualifers(T, QC);
}

void Sema::SetFunctionType(FunctionDecl *Function, QualType Type,
                           SourceLocation DiagLoc, SourceRange DiagRange) {
  if(Function->isExternal())
    Type = getTypeWithAttribute(Context, Type, Qualifiers::AS_external);
  if(!IsValidFunctionType(Type)) {
    Diags.Report(DiagLoc, diag::err_func_invalid_type)
      << Function->getIdentifier(); // FIXME: add diag range.
    return;
  }
  Function->setType(Type);
  if(Function->hasResult())
    Function->getResult()->setType(Type);
}

Decl *Sema::ActOnExternalEntityDecl(ASTContext &C, QualType T,
                                    SourceLocation IDLoc, const IdentifierInfo *IDInfo) {
  SourceLocation TypeLoc;
  VarDecl *ArgumentExternal = nullptr;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    auto Quals = getDeclQualifiers(Prev);
    if(Quals.hasAttributeSpec(Qualifiers::AS_external)) {
      Diags.Report(IDLoc, diag::err_duplicate_attr_spec)
        << DeclSpec::getSpecifierName(Qualifiers::AS_external);
      return Prev;
    }

    // apply EXTERNAL to an unused symbol or an argument.
    auto VD = dyn_cast<VarDecl>(Prev);
    if(VD && (VD->isUnusedSymbol() || VD->isArgument()) ) {
      T = VD->getType();
      TypeLoc = VD->getLocation();
      CurContext->removeDecl(VD);
      if(VD->isArgument())
        ArgumentExternal = VD;
    } else {
      DiagnoseRedefinition(IDLoc, IDInfo, Prev);
      return nullptr;
    }
  }
  if(T.isNull())
    T = C.VoidTy;

  DeclarationNameInfo DeclName(IDInfo,IDLoc);
  auto Decl = FunctionDecl::Create(C, ArgumentExternal? FunctionDecl::ExternalArgument :
                                                        FunctionDecl::External,
                                   CurContext, DeclName, T);
  SetFunctionType(Decl, T, TypeLoc, SourceRange()); //FIXME: proper loc, and range
  CurContext->addDecl(Decl);
  if(ArgumentExternal)
    ArgumentExternal->setType(C.getFunctionType(Decl));
  return Decl;
}

Decl *Sema::ActOnIntrinsicEntityDecl(ASTContext &C, QualType T,
                                     SourceLocation IDLoc, const IdentifierInfo *IDInfo) {
  auto FuncResult = IntrinsicFunctionMapping.Resolve(IDInfo);
  if(FuncResult.IsInvalid) {
    Diags.Report(IDLoc, diag::err_intrinsic_invalid_func)
      << IDInfo << getTokenRange(IDLoc);
    return nullptr;
  }

  QualType Type = T.isNull()? C.RealTy : T;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    auto Quals = getDeclQualifiers(Prev);
    if(Quals.hasAttributeSpec(Qualifiers::AS_intrinsic)) {
      Diags.Report(IDLoc, diag::err_duplicate_attr_spec)
        << DeclSpec::getSpecifierName(Qualifiers::AS_intrinsic);
      return Prev;
    }

    auto VD = dyn_cast<VarDecl>(Prev);
    if(VD && VD->isUnusedSymbol()) {
      Type = VD->getType();
      CurContext->removeDecl(VD);
    } else {
      DiagnoseRedefinition(IDLoc, IDInfo, Prev);
      return nullptr;
    }
  }

  auto Decl = IntrinsicFunctionDecl::Create(C, CurContext, IDLoc, IDInfo,
                                            Type, FuncResult.Function);
  CurContext->addDecl(Decl);
  return Decl;
}

// FIXME:
Decl *Sema::ActOnParameterEntityDecl(ASTContext &C, QualType T,
                                     SourceLocation IDLoc, const IdentifierInfo *IDInfo,
                                     SourceLocation EqualLoc, ExprResult Value) {
  VarDecl *VD = nullptr;

  if (auto Prev = LookupIdentifier(IDInfo)) {
    VD = dyn_cast<VarDecl>(Prev);
    if(!VD || VD->isParameter() || VD->isArgument() ||
       VD->isFunctionResult()) {
      DiagnoseRedefinition(IDLoc, IDInfo, Prev);
      return nullptr;
    }
  }

  // Make sure the value is a constant expression.
  if(!Value.get()->isEvaluatable(C)) {
    llvm::SmallVector<const Expr*, 16> Results;
    Value.get()->GatherNonEvaluatableExpressions(C, Results);
    Diags.Report(IDLoc, diag::err_parameter_requires_const_init)
        << IDInfo << Value.get()->getSourceRange();
    for(auto E : Results) {
      Diags.Report(E->getLocation(), diag::note_parameter_value_invalid_expr)
          << E->getSourceRange();
    }
    return nullptr;
  }

  if(VD) {
    Value = CheckAndApplyAssignmentConstraints(EqualLoc,
                                               VD->getType(), Value.get(),
                                               Sema::AssignmentAction::Initializing);
    if(Value.isInvalid()) return nullptr;
    // FIXME: if value is invalid, mutate into parameter givin a zero value
    VD->MutateIntoParameter(Value.get());
  } else {
    QualType T = Value.get()->getType();
    VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
    VD->MutateIntoParameter(Value.get());
    CurContext->addDecl(VD);
  }
  return VD;
}

Decl *Sema::ActOnEntityDecl(ASTContext &C, const QualType &T,
                            SourceLocation IDLoc, const IdentifierInfo *IDInfo) {
  auto Quals = T.getQualifiers();

  if(Quals.hasAttributeSpec(Qualifiers::AS_external))
    return ActOnExternalEntityDecl(C, T, IDLoc, IDInfo);
  else if(Quals.hasAttributeSpec(Qualifiers::AS_intrinsic))
    return ActOnIntrinsicEntityDecl(C, T, IDLoc, IDInfo);

  if (auto Prev = LookupIdentifier(IDInfo)) {
    FunctionDecl *FD = dyn_cast<FunctionDecl>(Prev);
    if(auto VD = dyn_cast<VarDecl>(Prev)) {
      if(VD->isArgument() && VD->getType().isNull()) {
        VD->setType(T);
        return VD;
      } else if(VD->isFunctionResult())
         FD = CurrentContextAsFunction();
    }

    if(FD && (FD->isNormalFunction() || FD->isExternal())) {
      if(FD->getType().isNull() || FD->getType()->isVoidType()) {
        SetFunctionType(FD, T, IDLoc, SourceRange()); //Fixme: proper loc and range
        return FD;
      } else {
        Diags.Report(IDLoc, diag::err_func_return_type_already_specified) << IDInfo;
        return nullptr;
      }
    }
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return nullptr;
  }

  VarDecl *VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
  CurContext->addDecl(VD);

  if(!T.isNull()) {
    auto SubT = T;
    if(T->isArrayType()) {
      CheckArrayTypeDeclarationCompability(T->asArrayType(), VD);
      SubT = T->asArrayType()->getElementType();
      VD->MarkUsedAsVariable(IDLoc);
    }
    else if(SubT->isCharacterType())
      CheckCharacterLengthDeclarationCompability(SubT, VD);
  }

  return VD;
}

Decl *Sema::ActOnEntityDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo) {
  QualType T = ActOnTypeName(C, DS);
  return ActOnEntityDecl(C, T, IDLoc, IDInfo);
}

//
// derived types
//

RecordDecl *Sema::ActOnDerivedTypeDecl(ASTContext &C, SourceLocation Loc,
                                       SourceLocation IDLoc,
                                       const IdentifierInfo* IDInfo) {
  auto Record = RecordDecl::Create(C, CurContext, Loc, IDLoc, IDInfo);
  if(CheckDeclaration(IDInfo, IDLoc))
    CurContext->addDecl(Record);
  PushDeclContext(Record);
  return Record;
}

void Sema::ActOnDerivedTypeSequenceStmt(ASTContext &C, SourceLocation Loc) {
  cast<RecordDecl>(CurContext)->setIsSequence(true);
}

FieldDecl *Sema::ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                                           const IdentifierInfo *IDInfo,
                                           ExprResult Init) {
  QualType T = ActOnTypeName(C, DS);
  if(auto ArrTy = T->asArrayType()) {
    // FIXME: check deferred shape when pointer is used.
    for(auto Dim : ArrTy->getDimensions()) {
      if(!isa<ExplicitShapeSpec>(Dim)) {
        Diags.Report(IDLoc, diag::err_invalid_type_field_array_shape);
        break;
      }
    }
  } else if(auto RecordTy = T->asRecordType()) {
    if(cast<RecordDecl>(CurContext)->isSequence()) {
      auto Record = RecordTy->getElement(0)->getParent();
      if(!Record->isSequence()) {
        Diags.Report(IDLoc, diag::err_record_member_not_sequence)
          << IDInfo << T;
      }
    }
  }

  FieldDecl* Field = FieldDecl::Create(C, CurContext, IDLoc, IDInfo, T);

  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_duplicate_member) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_declaration);
  } else
    CurContext->addDecl(Field);

  return Field;
}

void Sema::ActOnENDTYPE(ASTContext &C, SourceLocation Loc,
                        SourceLocation IDLoc, const IdentifierInfo* IDInfo) {
  auto Record = dyn_cast<RecordDecl>(CurContext);
  if(IDInfo && IDInfo != Record->getIdentifier()) {
    Diags.Report(IDLoc, diag::err_expected_type_name)
      << Record->getIdentifier() << /*Kind=*/ 0
      << getTokenRange(IDLoc);
  }
}

void Sema::ActOnEndDerivedTypeDecl(ASTContext &C) {
  PopDeclContext();
}

} // namespace flang
