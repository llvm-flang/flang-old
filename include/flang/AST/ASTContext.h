//===--- ASTContext.h - Context to hold long-lived AST nodes ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTContext interface.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_ASTCONTEXT_H__
#define FLANG_AST_ASTCONTEXT_H__

#include "flang/AST/Decl.h"
#include "flang/AST/Type.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/SourceMgr.h"
#include <new>
#include <set>
#include <vector>

namespace llvm {
  template <typename T> class ArrayRef;
  struct fltSemantics;
} // end llvm namespace

namespace flang {

class StoredDeclsMap;

// Decls
class DeclContext;
class Decl;
class RecordDecl;
class TypeDecl;
class TargetInfo;

class ASTContext : public llvm::RefCountedBase<ASTContext> {
  ASTContext &this_() { return *this; }

  mutable std::vector<Type*>            Types;
  mutable llvm::FoldingSet<ExtQuals>    ExtQualNodes;
  mutable llvm::FoldingSet<BuiltinType> BuiltinTypes;
  mutable llvm::FoldingSet<CharacterType> CharTypes;
  mutable llvm::FoldingSet<PointerType> PointerTypes;
  mutable llvm::FoldingSet<ArrayType>   ArrayTypes;
  mutable llvm::FoldingSet<RecordType>  RecordTypes;
  mutable llvm::FoldingSet<FunctionType> FunctionTypes;

  /// \brief The allocator used to create AST objects.
  ///
  /// AST objects are never destructed; rather, all memory associated with the
  /// AST objects will be released when the ASTContext itself is destroyed.
  mutable llvm::BumpPtrAllocator BumpAlloc;

  TranslationUnitDecl *TUDecl;

  /// SrcMgr - The associated SourceMgr object.
  llvm::SourceMgr &SrcMgr;

  LangOptions LanguageOptions;

  QualType getTypeDeclTypeSlow(const TypeDecl *Decl);

  void InitBuiltinTypes();

  const TargetInfo *Target;

public:
  ASTContext(llvm::SourceMgr &SM, LangOptions LangOpts);
  ~ASTContext();

  const TargetInfo &getTargetInfo() const { return *Target; }

  TranslationUnitDecl *getTranslationUnitDecl() const { return TUDecl; }

  llvm::SourceMgr &getSourceManager() { return SrcMgr; }
  const llvm::SourceMgr &getSourceManager() const { return SrcMgr; }

  void *Allocate(unsigned Size, unsigned Align = 8) const {
    return BumpAlloc.Allocate(Size, Align);
  }
  void Deallocate(void *Ptr, size_t size) const {
    BumpAlloc.Deallocate((const void*) Ptr, size);
  }

  const LangOptions& getLangOpts() const { return LanguageOptions; }

  // Builtin Types: [R404]
  QualType VoidTy;
  QualType IntegerTy;
  QualType RealTy;
  QualType DoublePrecisionTy;
  QualType LogicalTy;
  QualType ComplexTy;
  QualType DoubleComplexTy;
  QualType CharacterTy;

  /// \brief This is a character type with a '*' length specification
  QualType NoLengthCharacterTy;

  bool isTypeDoublePrecision(QualType T) const {
    return T->getBuiltinTypeKind() == DoublePrecisionTy->getBuiltinTypeKind();
  }

  bool isTypeDoubleComplex(QualType T) const {
    return T->getBuiltinTypeKind() == DoubleComplexTy->getBuiltinTypeKind();
  }

  //===--------------------------------------------------------------------===//
  //                           Type Constructors
  //===--------------------------------------------------------------------===//

  /// getExtQualType - Return a type with extended qualifiers.
  QualType getExtQualType(const Type *Base, Qualifiers Quals) const;
  QualType getQualTypeOtherKind(QualType Type, QualType KindType);
  QualType getTypeWithQualifers(QualType Type, Qualifiers Quals);

  /// getBuiltinType - Return the unique reference for the specified builtin type.
  BuiltinType *getBuiltinType(BuiltinType::TypeSpec TS,
                              BuiltinType::TypeKind Kind,
                              bool IsKindExplicitlySpecified = false,
                              bool IsDoublePrecisionKindSpecified = false);

  /// getComplexTypeElementType - Returns the type of an element in a complex pair.
  QualType getComplexTypeElementType(QualType Type);

  /// getComplexType - Returns the unique reference for a complex type
  /// with a given element type.
  QualType getComplexType(QualType ElementType);

  /// getCharacterType - Return the unique reference for the specified character type.
  CharacterType *getCharacterType(uint64_t Length) const;

  /// getPointerType - Return the uniqued reference to the type for a pointer to
  /// the specified type.
  PointerType *getPointerType(const Type *Ty, unsigned NumDims);

  /// getArrayType - Return a non-unique reference to the type for an array of the
  /// specified element type.
  QualType getArrayType(QualType EltTy, ArrayRef<ArraySpec*> Dims);

  /// getFunctionType - Return the unique reference to the type for an array of
  /// the specified function protototype.
  QualType getFunctionType(QualType ResultType, const FunctionDecl *Prototype);

  QualType getFunctionType(const FunctionDecl *Prototype) {
    return getFunctionType(QualType(), Prototype);
  }

  /// getRecordType - Return the uniqued reference to the type for a structure
  /// of the specified type.
  QualType getRecordType(const RecordDecl *Record);

  /// getTypeDeclType - Return the unique reference to the type for
  /// the specified type declaration.
  QualType getTypeDeclType(const TypeDecl *Decl,
                           const TypeDecl *PrevDecl = 0) {
    assert(Decl && "Passed null for Decl param");
    if (Decl->TypeForDecl) return QualType(Decl->TypeForDecl, 0);

    if (PrevDecl) {
      assert(PrevDecl->TypeForDecl && "previous decl has no TypeForDecl");
      Decl->TypeForDecl = PrevDecl->TypeForDecl;
      return QualType(PrevDecl->TypeForDecl, 0);
    }

    return getTypeDeclTypeSlow(Decl);
  }

  const std::vector<Type*> &getTypes() const { return Types; }

  /// getQualifiedType - Returns a type with additional qualifiers.
  QualType getQualifiedType(QualType T, Qualifiers Qs) const {
    QualifierCollector Qc(Qs);
    const Type *Ptr = Qc.strip(T);
    return getExtQualType(Ptr, Qc);
  }

  /// getQualifiedType - Returns a type with additional qualifiers.
  QualType getQualifiedType(const Type *T, Qualifiers Qs) const {
    return getExtQualType(T, Qs);
  }

  //===--------------------------------------------------------------------===//
  //                            Type Operators
  //===--------------------------------------------------------------------===//

  /// getCanonicalType - Return the canonical (structural) type corresponding to
  /// the specified potentially non-canonical type.
  QualType getCanonicalType(QualType T) const {
    return T.getCanonicalType();
  }

  const Type *getCanonicalType(const Type *T) const {
    return T->getCanonicalTypeInternal().getTypePtr();
  }

  /// \brief Returns the floating point semantics for the given real type.
  const llvm::fltSemantics& getFPTypeSemantics(QualType Type);

  /// \brief Returns the amount of bits that an arithmetic type kind occupies.
  unsigned getTypeKindBitWidth(BuiltinType::TypeKind Kind) const;

  /// \brief Returns a type kind big enough to store a value
  /// ranging from -10^Range to 10^Range, or
  /// NoKind if such value can't be stored.
  BuiltinType::TypeKind getSelectedIntKind(int64_t Range) const;

private:
  // FIXME: This currently contains the set of StoredDeclMaps used
  // by DeclContext objects. This probably should not be in ASTContext,
  // but we include it here so that ASTContext can quickly deallocate them.
  StoredDeclsMap *LastSDM;

  void ReleaseDeclContextMaps();
  friend class DeclContext;
};

} // end flang namespace

// operator new and delete aren't allowed inside namespaces. The throw
// specifications are mandated by the standard.

/// @brief Placement new for using the ASTContext's allocator.
///
/// This placement form of operator new uses the ASTContext's allocator for
/// obtaining memory. It is a non-throwing new, which means that it returns null
/// on error. (If that is what the allocator does. The current does, so if this
/// ever changes, this operator will have to be changed, too.)
///
/// Usage looks like this (assuming there's an ASTContext 'Context' in scope):
/// 
/// @code
/// // Default alignment (8)
/// IntegerLiteral *Ex = new (Context) IntegerLiteral(arguments);
/// // Specific alignment
/// IntegerLiteral *Ex2 = new (Context, 4) IntegerLiteral(arguments);
/// @endcode
/// 
/// Please note that you cannot use delete on the pointer; it must be
/// deallocated using an explicit destructor call followed by
/// @c Context.Deallocate(Ptr).
///
/// @param Bytes The number of bytes to allocate. Calculated by the compiler.
/// @param C The ASTContext that provides the allocator.
/// @param Alignment The alignment of the allocated memory (if the underlying
///                  allocator supports it).
/// @return The allocated memory. Could be NULL.
inline void *operator new(size_t Bytes, const flang::ASTContext &C,
                          size_t Alignment = 8) throw () {
  return C.Allocate(Bytes, Alignment);
}

/// @brief Placement delete companion to the new above.
///
/// This operator is just a companion to the new above. There is no way of
/// invoking it directly; see the new operator for more details. This operator
/// is called implicitly by the compiler if a placement new expression using the
/// ASTContext throws in the object constructor.
inline void operator delete(void *Ptr, const flang::ASTContext &C, size_t size)
              throw () {
  C.Deallocate(Ptr, size);
}

/// This placement form of operator new[] uses the ASTContext's allocator for
/// obtaining memory. It is a non-throwing new[], which means that it returns
/// null on error.
///
/// Usage looks like this (assuming there's an ASTContext 'Context' in scope):
/// 
/// @code
/// // Default alignment (8)
/// char *data = new (Context) char[10];
/// // Specific alignment
/// char *data = new (Context, 4) char[10];
/// @endcode
/// 
/// Please note that you cannot use delete on the pointer; it must be
/// deallocated using an explicit destructor call followed by
/// @c Context.Deallocate(Ptr).
///
/// @param Bytes The number of bytes to allocate. Calculated by the compiler.
/// @param C The ASTContext that provides the allocator.
/// @param Alignment The alignment of the allocated memory (if the underlying
///                  allocator supports it).
/// @return The allocated memory. Could be NULL.
inline void *operator new[](size_t Bytes, const flang::ASTContext& C,
                            size_t Alignment = 8) throw () {
  return C.Allocate(Bytes, Alignment);
}

/// @brief Placement delete[] companion to the new[] above.
///
/// This operator is just a companion to the new[] above. There is no way of
/// invoking it directly; see the new[] operator for more details. This operator
/// is called implicitly by the compiler if a placement new[] expression using
/// the ASTContext throws in the object constructor.
inline void operator delete[](void *Ptr, const flang::ASTContext &C, size_t size)
              throw () {
  C.Deallocate(Ptr, size);
}

#endif
