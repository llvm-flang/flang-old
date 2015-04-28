//===--- Spec.cpp - AST Builder and Semantic Analysis Implementation ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the application of Specification statements to the
// declarations.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/StorageSet.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

void Sema::ActOnFunctionSpecificationPart() {
  /// If necessary, apply the implicit typing rules to the current function and its arguments.
  if(auto FD = dyn_cast<FunctionDecl>(CurContext)) {
    // function type
    if(FD->isNormalFunction() || FD->isStatementFunction()) {
      if(FD->getType().isNull()) {
        auto Type = ResolveImplicitType(FD->getIdentifier());
        if(Type.isNull()) {
          Diags.Report(FD->getLocation(), diag::err_func_no_implicit_type)
            << FD->getIdentifier();
          // FIXME: add note implicit none was applied here.
        }
        else SetFunctionType(FD, Type, FD->getLocation(), SourceRange()); //FIXME: proper loc and range
      }
    }

    // arguments
    for(auto Arg : FD->getArguments()) {
      if(Arg->getType().isNull()) {
        ApplyImplicitRulesToArgument(Arg);
      }
    }
  }
}

VarDecl *Sema::GetVariableForSpecification(SourceLocation StmtLoc,
                                           const IdentifierInfo *IDInfo,
                                           SourceLocation IDLoc,
                                           bool CanBeArgument) {
  auto Declaration = LookupIdentifier(IDInfo);
  if(!Declaration)
    return CreateImplicitEntityDecl(Context, IDLoc, IDInfo);
  auto VD = dyn_cast<VarDecl>(Declaration);
  if(VD && !(VD->isParameter() || (!CanBeArgument && VD->isArgument()) || VD->isFunctionResult()))
    return VD;
  Diags.Report(StmtLoc, CanBeArgument? diag::err_spec_requires_local_var_arg : diag::err_spec_requires_local_var)
    << IDInfo << getTokenRange(IDLoc);
  if(VD) {
    Diags.Report(Declaration->getLocation(), diag::note_previous_definition_kind)
        << IDInfo << (VD->isArgument()? 0 : 1)
        << getTokenRange(Declaration->getLocation());
  } else
    Diags.Report(Declaration->getLocation(), diag::note_previous_definition);
  return nullptr;
}

void SpecificationScope::AddDimensionSpec(SourceLocation Loc, SourceLocation IDLoc,
                                          const IdentifierInfo *IDInfo,
                                          ArrayRef<ArraySpec*> Dims) {
  if(Dims.empty())
    return;
  auto Start = Dimensions.size();
  Dimensions.append(Dims.begin(), Dims.end());
  StoredDimensionSpec Spec = { Loc, IDLoc, IDInfo, unsigned(Start), unsigned(Dims.size()) };
  DimensionSpecs.push_back(Spec);
}

 bool SpecificationScope::IsDimensionAppliedTo(const IdentifierInfo *IDInfo) const {
   for(auto Spec : DimensionSpecs) {
     if(Spec.IDInfo == IDInfo) return true;
   }
   return false;
 }

void SpecificationScope::ApplyDimensionSpecs(Sema &Visitor) {
  for(auto Spec : DimensionSpecs)
    Visitor.ApplyDimensionSpecification(Spec.Loc, Spec.IDLoc, Spec.IDInfo,
      llvm::makeArrayRef(Dimensions.begin()+Spec.Offset, Spec.Size));
}

bool Sema::ApplyDimensionSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo,
                                       ArrayRef<ArraySpec*> Dims) {
  auto VD = GetVariableForSpecification(Loc, IDInfo, IDLoc);
  if(!VD) return true;
  if(isa<ArrayType>(VD->getType())) {
    Diags.Report(Loc,
                 diag::err_spec_dimension_already_array)
      << VD->getIdentifier();
    return true;
  }

  auto T = ActOnArraySpec(Context, VD->getType(), Dims);
  bool IsTypeImplicit = VD->isTypeImplicit();
  VD->setType(T);
  if(T->isArrayType()) {
    CheckArrayTypeDeclarationCompability(T->asArrayType(), VD);
    VD->MarkUsedAsVariable(Loc);
  }
  VD->setTypeImplicit(IsTypeImplicit);
  return false;
}

void SpecificationScope::AddSaveSpec(SourceLocation Loc, SourceLocation IDLoc,
                                     const IdentifierInfo *IDInfo) {
  StoredSaveSpec Spec = { Loc, IDLoc, IDInfo };
  SaveSpecs.push_back(Spec);
}

void SpecificationScope::AddSaveSpec(SourceLocation Loc, SourceLocation IDLoc,
                                     CommonBlockDecl *Block) {
  StoredSaveCommonBlockSpec Spec = { Loc, IDLoc, Block };
  SaveCommonBlockSpecs.push_back(Spec);
}

void SpecificationScope::ApplySaveSpecs(Sema &Visitor) {
  for(auto Spec : SaveSpecs)
    Visitor.ApplySaveSpecification(Spec.Loc, Spec.IDLoc,
                                   Spec.IDInfo);
  for(auto Spec : SaveCommonBlockSpecs)
    Visitor.ApplySaveSpecification(Spec.Loc, Spec.IDLoc,
                                   Spec.Block);
}

bool Sema::ApplySaveSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                  const IdentifierInfo *IDInfo) {
  if(!IDInfo) {
    for(DeclContext::decl_iterator I = CurContext->decls_begin(),
        End = CurContext->decls_end(); I != End; ++I) {
      auto VD = dyn_cast<VarDecl>(*I);
      if(VD && !(VD->isParameter() || VD->isArgument() || VD->isFunctionResult())) {
        ApplySaveSpecification(Loc, SourceLocation(), VD);
      }
    }
    return false;
  }
  auto VD = GetVariableForSpecification(Loc, IDInfo, IDLoc, false);
  if(!VD) return true;
  return ApplySaveSpecification(Loc, IDLoc, VD);
}

bool Sema::ApplySaveSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                  VarDecl *VD) {
  if(VD->hasStorageSet()) {
    if(auto CBSet = dyn_cast<CommonBlockSet>(VD->getStorageSet())) {
      Diags.Report(Loc, diag::err_spec_not_applicable_to_common_block)
        << "save" << getTokenRange(IDLoc);
      return true;
    }
  }
  auto Type = VD->getType();
  auto Quals = Type.getQualifiers();
  if(Quals.hasAttributeSpec(Qualifiers::AS_save)) {
    if(IDLoc.isValid()) {
      Diags.Report(Loc, diag::err_spec_qual_reapplication)
        << "save" << VD->getIdentifier() << getTokenRange(IDLoc);
    } else {
      Diags.Report(Loc, diag::err_spec_qual_reapplication)
        << "save" << VD->getIdentifier();
    }
    return true;
  }
  Quals.addAttributeSpecs(Qualifiers::AS_save);
  VD->setType(Context.getQualifiedType(Type, Quals));
  return false;
}

bool Sema::ApplySaveSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                  CommonBlockDecl *Block) {
  return false;
}

void SpecificationScope::AddCommonSpec(SourceLocation Loc, SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo,
                                       CommonBlockDecl *Block) {
  StoredCommonSpec Spec = { Loc, IDLoc, IDInfo, Block };
  CommonSpecs.push_back(Spec);
}

class CommonBlockSetBuilder {
public:
  llvm::SmallDenseMap<CommonBlockDecl*,
    SmallVector<CommonBlockSet::Object, 16>, 8> Sets;

  void Add(CommonBlockDecl *CBDecl, CommonBlockSet::Object Object) {
    Sets.insert(std::make_pair(CBDecl,SmallVector<CommonBlockSet::Object, 16>())).first->second.push_back(Object);
  }

  void CreateSets(ASTContext &C) {
    for(auto Set : Sets)
      Set.first->getStorageSet()->setObjects(C, Set.second);
  }
};

void SpecificationScope::ApplyCommonSpecs(Sema &Visitor, CommonBlockSetBuilder &Builder) {
  for(auto Spec : CommonSpecs)
    Visitor.ApplyCommonSpecification(Spec.Loc, Spec.IDLoc,
                                     Spec.IDInfo, Spec.Block,
                                     Builder);
}

bool Sema::ApplyCommonSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo,
                                    CommonBlockDecl *Block, CommonBlockSetBuilder &Builder) {
  auto VD = GetVariableForSpecification(Loc, IDInfo, IDLoc);
  if(!VD) return true;
  if(VD->hasStorageSet()) {
    if(auto CBSet = dyn_cast<CommonBlockSet>(VD->getStorageSet())) {
      Diags.Report(Loc, diag::err_spec_qual_reapplication)
        << "common" << IDInfo
        << getTokenRange(IDLoc);
      return true;
    }
    // FIXME: COMMON + EQUIVALENCE
  }
  Builder.Add(Block, CommonBlockSet::Object(VD));
  VD->setStorageSet(Block->getStorageSet());
  return false;
}

/// Applies the specification statements to the declarations.
void Sema::ActOnSpecificationPart() {
  ActOnFunctionSpecificationPart();

  CurSpecScope->ApplyDimensionSpecs(*this);

  CommonBlockSetBuilder CBBuilder;
  CurSpecScope->ApplyCommonSpecs(*this, CBBuilder);
  CBBuilder.CreateSets(Context);

  CurSpecScope->ApplySaveSpecs(*this);
}

} // namespace flang
