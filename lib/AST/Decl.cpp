//===--- Decl.cpp - Classes for representing Declarations ------------------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Decl and related classes.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/Decl.h"
#include "flang/AST/DeclContextInternals.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/ASTContext.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

//===----------------------------------------------------------------------===//
// DeclContext Implementation
//===----------------------------------------------------------------------===//

DeclContext::~DeclContext() {}

void DeclContext::addDecl(Decl *D) {
  assert(D->getDeclContext() == this &&
         "Decl inserted into wrong lexical context");
  assert(!D->getNextDeclInContext() && D != LastDecl &&
         "Decl already inserted into a DeclContext");

  if (FirstDecl) {
    LastDecl->NextDeclInContext = D;
    LastDecl = D;
  } else {
    FirstDecl = LastDecl = D;
  }

  if (NamedDecl *ND = dyn_cast<NamedDecl>(D))
    ND->getDeclContext()->makeDeclVisibleInContext(ND);
}

void DeclContext::removeDecl(Decl *D) {
  assert((D->NextDeclInContext || D == LastDecl) &&
         "decl is not in decls list");

  // Remove D from the decl chain.  This is O(n) but hopefully rare.
  if (D == FirstDecl) {
    if (D == LastDecl)
      FirstDecl = LastDecl = 0;
    else
      FirstDecl = D->NextDeclInContext;
  } else {
    for (Decl *I = FirstDecl; true; I = I->NextDeclInContext) {
      assert(I && "Decl not found in linked list");
      if (I->NextDeclInContext == D) {
        I->NextDeclInContext = D->NextDeclInContext;
        if (D == LastDecl) LastDecl = I;
        break;
      }
    }
  }

  // Mark that D is no longer in the decl chain.
  D->NextDeclInContext = 0;

  // Remove D from the lookup table if necessary.
  if (NamedDecl *ND = dyn_cast<NamedDecl>(D)) {
    // Remove only decls that have a name
    if (!ND->getDeclName()) return;

    StoredDeclsMap *Map = LookupPtr;
    if (!Map) return;

    StoredDeclsMap::iterator Pos = Map->find(ND->getDeclName());
    assert(Pos != Map->end() && "No lookup entry for decl");
    Pos->second.remove(ND);
  }
}

/// buildLookup - Build the lookup data structure with all of the declarations
/// in DCtx (and any other contexts linked to it or transparent contexts nested
/// within it).
void DeclContext::buildLookup(DeclContext *DCtx) {
  for (decl_iterator
         D = DCtx->decls_begin(), DEnd = DCtx->decls_end(); D != DEnd; ++D)
    // Insert this declaration into the lookup structure, but only if it's
    // semantically in its decl context.  During non-lazy lookup building, this
    // is implicitly enforced by addDecl.
    if (NamedDecl *ND = dyn_cast<NamedDecl>(*D))
      if (D->getDeclContext() == DCtx)
        makeDeclVisibleInContextImpl(ND);
}

DeclContext::lookup_result
DeclContext::lookup(DeclarationName Name) {
  /// If there is no lookup data structure, build one now by walking
  /// all of the linked DeclContexts (in declaration order!) and
  /// inserting their values.
  if (!LookupPtr) {
    buildLookup(this);

    if (!LookupPtr)
      return lookup_result(lookup_iterator(0), lookup_iterator(0));
  }

  StoredDeclsMap::iterator Pos = LookupPtr->find(Name);
  if (Pos == LookupPtr->end())
    return lookup_result(lookup_iterator(0), lookup_iterator(0));
  return Pos->second.getLookupResult();
}

void DeclContext::makeDeclVisibleInContext(NamedDecl *D) {
  // If we already have a lookup data structure, perform the insertion into
  // it.
  if (!LookupPtr)
    return;

  makeDeclVisibleInContextImpl(D);
}

void DeclContext::makeDeclVisibleInContextImpl(NamedDecl *D) {
  // Skip unnamed declarations.
  if (!D->getDeclName())
    return;

  ASTContext *C = 0;
  if (!LookupPtr) {
    C = &getParentASTContext();
    CreateStoredDeclsMap(*C);
  }

  // Insert this declaration into the map.
  StoredDeclsList &DeclNameEntries = (*LookupPtr)[D->getDeclName()];
  if (DeclNameEntries.isNull()) {
    DeclNameEntries.setOnlyValue(D);
    return;
  }

  // Put this declaration into the appropriate slot.
  DeclNameEntries.AddSubsequentDecl(D);
}

//===----------------------------------------------------------------------===//
// Declaration Implementations
//===----------------------------------------------------------------------===//

Decl::~Decl() {}

Decl *Decl::castFromDeclContext (const DeclContext *D) {
  Decl::Kind DK = D->getDeclKind();
  switch(DK) {
#define DECL(NAME, BASE)
#define DECL_CONTEXT(NAME) \
    case Decl::NAME:       \
      return static_cast<NAME##Decl*>(const_cast<DeclContext*>(D));
#define DECL_CONTEXT_BASE(NAME)
#include "flang/AST/DeclNodes.inc"
    default:
#define DECL(NAME, BASE)
#define DECL_CONTEXT_BASE(NAME)                  \
      if (DK >= first##NAME && DK <= last##NAME) \
        return static_cast<NAME##Decl*>(const_cast<DeclContext*>(D));
#include "flang/AST/DeclNodes.inc"
      assert(false && "a decl that inherits DeclContext isn't handled");
      return 0;
  }
}

DeclContext *Decl::castToDeclContext(const Decl *D) {
  Decl::Kind DK = D->getKind();
  switch(DK) {
#define DECL(NAME, BASE)
#define DECL_CONTEXT(NAME) \
    case Decl::NAME:       \
      return static_cast<NAME##Decl*>(const_cast<Decl*>(D));
#define DECL_CONTEXT_BASE(NAME)
#include "flang/AST/DeclNodes.inc"
    default:
#define DECL(NAME, BASE)
#define DECL_CONTEXT_BASE(NAME)                                   \
      if (DK >= first##NAME && DK <= last##NAME)                  \
        return static_cast<NAME##Decl*>(const_cast<Decl*>(D));
#include "flang/AST/DeclNodes.inc"
      assert(false && "a decl that inherits DeclContext isn't handled");
      return 0;
  }
}

TranslationUnitDecl *Decl::getTranslationUnitDecl() {
  if (TranslationUnitDecl *TUD = dyn_cast<TranslationUnitDecl>(this))
    return TUD;

  DeclContext *DC = getDeclContext();
  assert(DC && "This decl is not contained in a program unit!");

  while (!DC->isTranslationUnit()) {
    DC = DC->getParent();
    assert(DC && "This decl is not contained in a program unit!");
  }

  return TranslationUnitDecl::castFromDeclContext(DC);
}

ASTContext &Decl::getASTContext() const {
  return getTranslationUnitDecl()->getASTContext();
}

//===----------------------------------------------------------------------===//
// TranslationUnitDecl Implementation
//===----------------------------------------------------------------------===//

TranslationUnitDecl *TranslationUnitDecl::Create(ASTContext &C) {
  return new (C) TranslationUnitDecl(C);
}

//===----------------------------------------------------------------------===//
// MainProgramDecl Implementation
//===----------------------------------------------------------------------===//

MainProgramDecl *MainProgramDecl::Create(ASTContext &C, DeclContext *DC,
                                         const DeclarationNameInfo &NameInfo) {
  return new (C) MainProgramDecl(DC, NameInfo);
}

void MainProgramDecl::setBody(Stmt *S) {
  assert(!Body);
  Body = S;
}

//===----------------------------------------------------------------------===//
// FunctionDecl Implementation
//===----------------------------------------------------------------------===//

FunctionDecl *FunctionDecl::Create(ASTContext &C, FunctionKind FK,
                                   DeclContext *DC,
                                   const DeclarationNameInfo &NameInfo,
                                   QualType ReturnType, int Attr) {
  return new(C) FunctionDecl(Function, FK, DC, NameInfo, ReturnType, Attr);
}

void FunctionDecl::setBody(Stmt *S) {
  Body = S;
}

void FunctionDecl::setBody(Expr *E) {
  Body = E;
}

void FunctionDecl::setArguments(ASTContext &C, ArrayRef<VarDecl*> ArgumentList) {
  assert(!Arguments);
  if(ArgumentList.size()) {
    ArgumentCount = ArgumentList.size();
    Arguments = new(C) VarDecl*[ArgumentCount];
    for(unsigned I = 0; I < ArgumentCount; ++I)
      Arguments[I] = ArgumentList[I];
  }
}

void FunctionDecl::setResult(VarDecl *VD) {
  Result = VD;
}

//===----------------------------------------------------------------------===//
// SelfDecl Implementation
//===----------------------------------------------------------------------===//

SelfDecl *SelfDecl::Create(ASTContext &C, DeclContext *DC,
                           DeclaratorDecl *Self) {
  return new(C) SelfDecl(DC, Self);
}

//===----------------------------------------------------------------------===//
// RecordDecl Implementation
//===----------------------------------------------------------------------===//

RecordDecl *RecordDecl::Create(const ASTContext &C, DeclContext *DC,
                               SourceLocation StartLoc, SourceLocation IdLoc,
                               const IdentifierInfo *Id,
                               RecordDecl *PrevDecl) {
  auto R = new(C) RecordDecl(Record, DC, StartLoc, IdLoc, Id, PrevDecl);
  return R;
}

//===----------------------------------------------------------------------===//
// EnumConstantDecl Implementation
//===----------------------------------------------------------------------===//

EnumConstantDecl *EnumConstantDecl::Create(ASTContext &C, DeclContext *DC,
                                           SourceLocation L, IdentifierInfo *Id,
                                           QualType T, Expr *E,
                                           const llvm::APSInt &V) {
  return new (C) EnumConstantDecl(DC, L, Id, T, E, V);
}

SourceRange EnumConstantDecl::getSourceRange() const {
  SourceLocation End = getLocation();
  if (Init)
    End = Init->getLocation();  // FIXME: getLocEnd() ?
  return SourceRange(getLocation(), End);
}

//===----------------------------------------------------------------------===//
// FieldDecl Implementation
//===----------------------------------------------------------------------===//

FieldDecl *FieldDecl::Create(const ASTContext &C, DeclContext *DC,
                             /*SourceLocation StartLoc,*/ SourceLocation IdLoc,
                             const IdentifierInfo *Id, QualType T) {
  return new (C) FieldDecl(Decl::Field, DC, IdLoc, Id, T);
}

//===----------------------------------------------------------------------===//
// IntrinsicFunctionDecl Implementation
//===----------------------------------------------------------------------===//

IntrinsicFunctionDecl *IntrinsicFunctionDecl::Create(ASTContext &C,
                                                     DeclContext *DC,
                                                     SourceLocation IDLoc,
                                                     const IdentifierInfo *ID,
                                                     QualType T,
                                                     intrinsic::FunctionKind Function) {
  return new(C) IntrinsicFunctionDecl(Decl::IntrinsicFunction, DC, IDLoc, ID,
                                      T, Function);
}

//===----------------------------------------------------------------------===//
// VarDecl Implementation
//===----------------------------------------------------------------------===//

VarDecl *VarDecl::Create(ASTContext &C, DeclContext *DC,
                         SourceLocation IdLoc, const IdentifierInfo *Id,
                         QualType T) {
  return new (C) VarDecl(Var, DC, IdLoc, Id, T);
}

VarDecl *VarDecl::CreateArgument(ASTContext &C, DeclContext *DC,
                                 SourceLocation IDLoc, const IdentifierInfo *ID) {
  auto Result = new (C) VarDecl(Var, DC, IDLoc, ID, QualType());
  Result->SubDeclKind = FunctionArgument;
  return Result;
}

VarDecl *VarDecl::CreateFunctionResult(ASTContext &C, DeclContext *DC,
                                       SourceLocation IDLoc, const IdentifierInfo *ID) {
  auto Result = new (C) VarDecl(Var, DC, IDLoc, ID, QualType());
  Result->SubDeclKind = FunctionResult;
  return Result;
}

void VarDecl::setInit(Expr *E) const {
  Init = E;
}

void VarDecl::MutateIntoParameter(Expr *Value) {
  assert(!isParameter());
  assert(!Init);
  Init = Value;
  SubDeclKind = ParameterVariable;
}

void VarDecl::MarkUsedAsVariable (SourceLocation Loc) {
  if(SubDeclKind == UnusedSymbol)
    SubDeclKind = LocalVariable;
}

//===----------------------------------------------------------------------===//
// CommonBlockDecl Implementation
//===----------------------------------------------------------------------===//

CommonBlockDecl * CommonBlockDecl::Create(ASTContext &C, DeclContext *DC,
                                          SourceLocation IDLoc,
                                          const IdentifierInfo *IDInfo) {
  return new(C) CommonBlockDecl(DC, IDLoc, IDInfo);
}

//===----------------------------------------------------------------------===//
// Creation and Destruction of StoredDeclsMaps
//===----------------------------------------------------------------------===//

StoredDeclsMap *DeclContext::CreateStoredDeclsMap(ASTContext &C) const {
  assert(!LookupPtr && "Context already has a decls map");
  StoredDeclsMap *M = new StoredDeclsMap();
  M->Previous = C.LastSDM;
  C.LastSDM = M;
  LookupPtr = M;
  return M;
}

void ASTContext::ReleaseDeclContextMaps() {
  // It's okay to delete DependentStoredDeclsMaps via a StoredDeclsMap
  // pointer because the subclass doesn't add anything that needs to
  // be deleted.
  StoredDeclsMap::DestroyAll(LastSDM);
}

void StoredDeclsMap::DestroyAll(StoredDeclsMap *Map) {
  while (Map) {
    // Advance the iteration before we invalidate memory.
    StoredDeclsMap *Next = Map->Previous;
    delete Map;
    Map = Next;
  }
}

} //namespace flang
