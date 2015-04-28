//===--- ASTContext.cpp - Context to hold long-lived AST nodes ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the ASTContext interface.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/ASTContext.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/ErrorHandling.h"


namespace flang {

ASTContext::ASTContext(llvm::SourceMgr &SM, LangOptions LangOpts)
  : SrcMgr(SM), LastSDM(0), LanguageOptions(LangOpts) {
  TUDecl = TranslationUnitDecl::Create(*this);
  InitBuiltinTypes();
}

ASTContext::~ASTContext() {
  // Release the DenseMaps associated with DeclContext objects.
  // FIXME: Is this the ideal solution?
  ReleaseDeclContextMaps();
}

void ASTContext::InitBuiltinTypes() {
  // [R404]
  VoidTy = QualType(new (*this, TypeAlignment) VoidType(), 0);

  auto Opts = getLangOpts();
  auto IntKind = Opts.DefaultInt8? Type::Int8 : Type::Int4;
  auto RealKind = Opts.DefaultReal8? Type::Real8 : Type::Real4;
  auto DblKind = Type::Real8;
  if(Opts.DefaultReal8 && !Opts.DefaultDouble8)
    DblKind = Type::Real16;

  IntegerTy = QualType(getBuiltinType(BuiltinType::Integer, IntKind), 0);

  RealTy = QualType(getBuiltinType(BuiltinType::Real, RealKind), 0);
  DoublePrecisionTy = QualType(getBuiltinType(BuiltinType::Real, DblKind,
                                              false, true), 0);

  ComplexTy = QualType(getBuiltinType(BuiltinType::Complex, RealKind), 0);
  DoubleComplexTy = QualType(getBuiltinType(BuiltinType::Complex, DblKind,
                                            false, true), 0);

  LogicalTy = QualType(getBuiltinType(BuiltinType::Logical, IntKind), 0);

  CharacterTy = QualType(getCharacterType(1), 0);
  NoLengthCharacterTy = QualType(getCharacterType(0), 0);
}

const llvm::fltSemantics&  ASTContext::getFPTypeSemantics(QualType Type) {
  switch(Type->getBuiltinTypeKind()) {
  case BuiltinType::Real4:  return llvm::APFloat::IEEEsingle;
  case BuiltinType::Real8:  return llvm::APFloat::IEEEdouble;
  case BuiltinType::Real16: return llvm::APFloat::IEEEquad;
  default: break;
  }
  llvm_unreachable("invalid real type");
}

unsigned ASTContext::getTypeKindBitWidth(BuiltinType::TypeKind Kind) const {
  switch(Kind) {
  case BuiltinType::Int1: return 8;
  case BuiltinType::Int2: return 16;
  case BuiltinType::Int4: return 32;
  case BuiltinType::Int8: return 64;
  case BuiltinType::Real4: return 32;
  case BuiltinType::Real8: return 64;
  case BuiltinType::Real16: return 128;
  }
  llvm_unreachable("invalid built in type kind");
  return 0;
}

BuiltinType::TypeKind ASTContext::getSelectedIntKind(int64_t Range) const {
  if(Range <= 2)
    return BuiltinType::Int1;
  else if(Range <= 4)
    return BuiltinType::Int2;
  else if(Range <= 9)
    return BuiltinType::Int4;
  else if(Range <= 18)
    return BuiltinType::Int8;
  // NB: add Range <= 38 for Int16
  return BuiltinType::NoKind;
}

//===----------------------------------------------------------------------===//
//                   Type creation/memoization methods
//===----------------------------------------------------------------------===//

QualType ASTContext::getExtQualType(const Type *BaseType, Qualifiers Quals) const {
  // Check if we've already instantiated this type.
  llvm::FoldingSetNodeID ID;
  ExtQuals::Profile(ID, BaseType, Quals);
  void *InsertPos = 0;
  if (ExtQuals *EQ = ExtQualNodes.FindNodeOrInsertPos(ID, InsertPos)) {
    assert(EQ->getQualifiers() == Quals);
    return QualType(EQ, 0);
  }

  // If the base type is not canonical, make the appropriate canonical type.
  QualType Canon;
  if (!BaseType->isCanonicalUnqualified()) {
    SplitQualType CanonSplit = BaseType->getCanonicalTypeInternal().split();
    CanonSplit.second.addConsistentQualifiers(Quals);
    Canon = getExtQualType(CanonSplit.first, CanonSplit.second);

    // Re-find the insert position.
    (void) ExtQualNodes.FindNodeOrInsertPos(ID, InsertPos);
  }

  ExtQuals *EQ = new (*this, TypeAlignment) ExtQuals(BaseType, Canon, Quals);
  ExtQualNodes.InsertNode(EQ, InsertPos);
  return QualType(EQ, 0);
}

QualType ASTContext::getQualTypeOtherKind(QualType Type, QualType KindType) {
  auto BTy = Type->asBuiltinType();
  auto DesiredBTy = KindType->asBuiltinType();
  if(BTy->getBuiltinTypeKind() == DesiredBTy->getBuiltinTypeKind())
    return Type;
  auto Split = Type.split();
  return getExtQualType(getBuiltinType(BTy->getTypeSpec(), DesiredBTy->getBuiltinTypeKind(),
                                       DesiredBTy->isKindExplicitlySpecified(),
                                       DesiredBTy->isDoublePrecisionKindSpecified()), Split.second);
}

// NB: this assumes that real and complex have have the same default kind.
QualType ASTContext::getComplexTypeElementType(QualType Type) {
  assert(Type->isComplexType());
  if(Type->getBuiltinTypeKind() != RealTy->getBuiltinTypeKind())
    return getQualTypeOtherKind(RealTy, Type);
  return RealTy;
}

QualType ASTContext::getComplexType(QualType ElementType) {
  assert(ElementType->isRealType());
  if(ElementType->getBuiltinTypeKind() != ComplexTy->getBuiltinTypeKind())
    return getQualTypeOtherKind(ComplexTy, ElementType);
  return ComplexTy;
}

QualType ASTContext::getTypeWithQualifers(QualType Type, Qualifiers Quals) {
  auto Split = Type.split();
  return getExtQualType(Split.first, Quals);
}

BuiltinType *ASTContext::getBuiltinType(BuiltinType::TypeSpec TS,
                                        BuiltinType::TypeKind Kind,
                                        bool IsKindExplicitlySpecified,
                                        bool IsDoublePrecisionKindSpecified) {
  llvm::FoldingSetNodeID ID;
  BuiltinType::Profile(ID, TS, Kind, IsKindExplicitlySpecified,
                       IsDoublePrecisionKindSpecified);

  void *InsertPos = 0;
  if (auto BT = BuiltinTypes.FindNodeOrInsertPos(ID, InsertPos))
    return BT;

  auto BT = new (*this, TypeAlignment) BuiltinType(TS, Kind, IsKindExplicitlySpecified,
                                                   IsDoublePrecisionKindSpecified);
  Types.push_back(BT);
  BuiltinTypes.InsertNode(BT, InsertPos);
  return BT;
}

CharacterType *ASTContext::getCharacterType(uint64_t Length) const {
  llvm::FoldingSetNodeID ID;
  CharacterType::Profile(ID, Length);

  void *InsertPos = 0;
  if (auto CT = CharTypes.FindNodeOrInsertPos(ID, InsertPos))
    return CT;

  auto CT = new (*this, TypeAlignment) CharacterType(Length);
  Types.push_back(CT);
  CharTypes.InsertNode(CT, InsertPos);
  return CT;
}

/// getPointerType - Return the uniqued reference to the type for a pointer to
/// the specified type.
PointerType *ASTContext::getPointerType(const Type *Ty, unsigned NumDims) {
  // Unique pointers, to guarantee there is only one pointer of a particular
  // structure.
  llvm::FoldingSetNodeID ID;
  PointerType::Profile(ID, Ty, NumDims);

  void *InsertPos = 0;
  if (PointerType *PT = PointerTypes.FindNodeOrInsertPos(ID, InsertPos))
    return PT;

  PointerType *New = new (*this, TypeAlignment) PointerType(Ty, NumDims);
  Types.push_back(New);
  PointerTypes.InsertNode(New, InsertPos);
  return New;
}

/// getArrayType - Return the unique reference to the type for an array of the
/// specified element type.
QualType ASTContext::getArrayType(QualType EltTy,
                                  ArrayRef<ArraySpec*> Dims) {
  ArrayType *New = new (*this, TypeAlignment) ArrayType(*this, Type::Array, EltTy,
                                                        QualType(), Dims);
  Types.push_back(New);
  return QualType(New, 0);
}

QualType ASTContext::getFunctionType(QualType ResultType, const FunctionDecl *Prototype) {
  llvm::FoldingSetNodeID ID;
  FunctionType::Profile(ID, ResultType, Prototype);

  void *InsertPos = 0;
  FunctionType *Result = FunctionTypes.FindNodeOrInsertPos(ID, InsertPos);
  if(!Result) {
    Result = FunctionType::Create(*this, ResultType, Prototype);
    Types.push_back(Result);
    FunctionTypes.InsertNode(Result, InsertPos);
  }
  return QualType(Result, 0);
}

/// getTypeDeclTypeSlow - Return the unique reference to the type for the
/// specified type declaration.
QualType ASTContext::getTypeDeclTypeSlow(const TypeDecl *Decl) {
  assert(Decl && "Passed null for Decl param");
  assert(!Decl->TypeForDecl && "TypeForDecl present in slow case");

  const RecordDecl *Record = dyn_cast<RecordDecl>(Decl);
  if (!Record) {
    llvm_unreachable("TypeDecl without a type?");
    return QualType(Decl->TypeForDecl, 0);
  }

  return getRecordType(Record);
}

QualType ASTContext::getRecordType(const RecordDecl *Record) {
  if (Record->TypeForDecl) return QualType(Record->TypeForDecl, 0);

  SmallVector<FieldDecl*, 8> Fields;
  unsigned Idx = 0;
  for(auto I = Record->decls_begin(), End = Record->decls_end(); I != End; ++I, ++Idx) {
    if(auto Field = dyn_cast<FieldDecl>(*I)) {
      Fields.push_back(Field);
      Field->setIndex(Idx);
    }
  }
  RecordType *newType = new (*this, TypeAlignment) RecordType(*this, Record, Fields);
  Record->TypeForDecl = newType;
  Types.push_back(newType);
  return QualType(newType, 0);
}

} //namespace flang
