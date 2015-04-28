//===-- Type.h - Fortran Type Interface -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran type interface.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_TYPE_H__
#define FLANG_TYPE_H__

#include "flang/Basic/Specifiers.h"
#include "flang/Basic/Diagnostic.h"
#include "flang/Sema/Ownership.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"
#include "flang/Basic/LLVM.h"

namespace llvm {
  class raw_ostream;
} // end llvm namespace

namespace flang {
  enum {
    TypeAlignmentInBits = 4,
    TypeAlignment = 1 << TypeAlignmentInBits
  };

  class Type;
  class ExtQuals;
  class QualType;
  class BuiltinType;
  class CharacterType;
  class FunctionType;
  class RecordType;
}

namespace llvm {
  template <typename T>
  class PointerLikeTypeTraits;
  template<>
  class PointerLikeTypeTraits< ::flang::Type*> {
  public:
    static inline void *getAsVoidPointer(::flang::Type *P) { return P; }
    static inline ::flang::Type *getFromVoidPointer(void *P) {
      return static_cast< ::flang::Type*>(P);
    }
    enum { NumLowBitsAvailable = flang::TypeAlignmentInBits };
  };
  template<>
  class PointerLikeTypeTraits< ::flang::ExtQuals*> {
  public:
    static inline void *getAsVoidPointer(::flang::ExtQuals *P) { return P; }
    static inline ::flang::ExtQuals *getFromVoidPointer(void *P) {
      return static_cast< ::flang::ExtQuals*>(P);
    }
    enum { NumLowBitsAvailable = flang::TypeAlignmentInBits };
  };

  template <>
  struct isPodLike<flang::QualType> { static const bool value = true; };
}

namespace flang {

class ASTContext;
class Decl;
class RecordDecl;
class FieldDecl;
class Expr;
class ExtQualsTypeCommonBase;
class IdentifierInfo;
class QualifierCollector;
class FunctionDecl;

/// Qualifiers - The collection of all-type qualifiers we support.
class Qualifiers {
public:
  // Import attribute specifiers.
  typedef AttributeSpecifier AS;
  static const AS AS_unspecified = flang::AS_unspecified;
  static const AS AS_allocatable = flang::AS_allocatable;
  static const AS AS_asynchronous = flang::AS_asynchronous;
  static const AS AS_codimension = flang::AS_codimension;
  static const AS AS_contiguous = flang::AS_contiguous;
  static const AS AS_dimension = flang::AS_dimension;
  static const AS AS_external = flang::AS_external;
  static const AS AS_intrinsic = flang::AS_intrinsic;
  static const AS AS_optional = flang::AS_optional;
  static const AS AS_parameter = flang::AS_parameter;
  static const AS AS_pointer = flang::AS_pointer;
  static const AS AS_protected = flang::AS_protected;
  static const AS AS_save = flang::AS_save;
  static const AS AS_target = flang::AS_target;
  static const AS AS_value = flang::AS_value;
  static const AS AS_volatile = flang::AS_volatile;

  /// Import intent specifiers.
  typedef IntentSpecifier IS;
  static const IS IS_unspecified = flang::IS_unspecified;
  static const IS IS_in = flang::IS_in;
  static const IS IS_out = flang::IS_out;
  static const IS IS_inout = flang::IS_inout;

  /// Import access specifiers.
  typedef AccessSpecifier AC;
  static const AC AC_unspecified = flang::AC_unspecified;
  static const AC AC_public = flang::AC_public;
  static const AC AC_private = flang::AC_private;

  enum TQ { // NOTE: These flags must be kept in sync with DeclSpec::TQ.
    Allocatable = 1 << 0,
    Parameter   = 1 << 1,
    Volatile    = 1 << 2,
    APVMask     = (Allocatable | Parameter | Volatile)
  };

  enum {
    /// The maximum supported address space number.
    MaxAddressSpace = 0xFFFU,

    /// The width of the "fast" qualifier mask.
    FastWidth = 3,

    /// The fast qualifier mask.
    FastMask = (1 << FastWidth) - 1
  };
private:
  // bits: |0  ...  14|15..17|18..19|20   ..   31|
  //       |Attributes|Intent|Access|AddressSpace|
  uint32_t Mask;

  static const uint32_t AttrSpecShift = 0;
  static const uint32_t AttrSpecMask = 0x7FFF << AttrSpecShift;
  static const uint32_t IntentAttrShift = 15;
  static const uint32_t IntentAttrMask = 0x7 << IntentAttrShift;
  static const uint32_t AccessAttrShift = 18;
  static const uint32_t AccessAttrMask = 0x3 << AccessAttrShift;
  static const uint32_t AddressSpaceShift = 20;
  static const uint32_t AddressSpaceMask =
    ~(AttrSpecMask | IntentAttrMask | AccessAttrMask);
public:
  Qualifiers() : Mask(0) {}

  static Qualifiers fromFastMask(unsigned Mask) {
    Qualifiers Qs;
    Qs.addFastQualifiers(Mask);
    return Qs;
  }

  // Deserialize qualifiers from an opaque representation.
  static Qualifiers fromOpaqueValue(unsigned opaque) {
    Qualifiers Qs;
    Qs.Mask = opaque;
    return Qs;
  }

  // Serialize these qualifiers into an opaque representation.
  unsigned getAsOpaqueValue() const {
    return Mask;
  }

  /// General attributes.
  bool hasAttributeSpec(AS A) const {
    return (Mask & AttrSpecMask) & A;
  }
  bool hasAttributeSpecs() const { return Mask & AttrSpecMask; }
  unsigned getAttributeSpecs() const {
    return (Mask & AttrSpecMask) >> AttrSpecShift;
  }
  void setAttributeSpecs(unsigned type) {
    Mask = (Mask & ~AttrSpecMask) | (type << AttrSpecShift);
  }
  void addAttributeSpecs(unsigned type) {
    assert(type);
    setAttributeSpecs(type);
  }

  /// Intent attributes.
  bool hasIntentAttr() const { return Mask & IntentAttrMask; }
  IS getIntentAttr() const {
    return IS((Mask & IntentAttrMask) >> IntentAttrShift);
  }
  void setIntentAttr(IS type) {
    Mask = (Mask & ~IntentAttrMask) | (type << IntentAttrShift);
  }
  void removeIntentAttr() { setIntentAttr(IS_unspecified); }
  void addIntentAttr(IS type) {
    assert(type);
    setIntentAttr(type);
  }

  /// Access attributes.
  bool hasAccessAttr() const { return Mask & AccessAttrMask; }
  AC getAccessAttr() const {
    return AC((Mask & AccessAttrMask) >> AccessAttrShift);
  }
  void setAccessAttr(AC type) {
    Mask = (Mask & ~AccessAttrMask) | (type << AccessAttrShift);
  }
  void removeAccessAttr() { setAccessAttr(AC_unspecified); }
  void addAccessAttr(AC type) {
    assert(type);
    setAccessAttr(type);
  }

  /// Address space.
  bool hasAddressSpace() const { return Mask & AddressSpaceMask; }
  unsigned getAddressSpace() const { return Mask >> AddressSpaceShift; }
  void setAddressSpace(unsigned space) {
    assert(space <= MaxAddressSpace);
    Mask = (Mask & ~AddressSpaceMask)
         | (((uint32_t) space) << AddressSpaceShift);
  }
  void removeAddressSpace() { setAddressSpace(0); }
  void addAddressSpace(unsigned space) {
    assert(space);
    setAddressSpace(space);
  }

  void addFastQualifiers(unsigned mask) {
    assert(!(mask & ~FastMask) && "bitmask contains non-fast qualifier bits");
    Mask |= mask;
  }

  /// hasQualifiers - Return true if the set contains any qualifiers.
  bool hasQualifiers() const { return Mask; }
  bool empty() const { return !Mask; }

  /// \brief Add the qualifiers from the given set to this set.
  void addQualifiers(Qualifiers Q) {
    // If the other set doesn't have any non-boolean qualifiers, just
    // bit-or it in.
    if (Q.hasAttributeSpecs())
      addAttributeSpecs(Q.getAttributeSpecs());
    if (Q.hasIntentAttr())
      addIntentAttr(Q.getIntentAttr());
    if (Q.hasAddressSpace())
      addAddressSpace(Q.getAddressSpace());
  }

  /// \brief Add the qualifiers from the given set to this set, given that
  /// they don't conflict.
  void addConsistentQualifiers(Qualifiers qs) {
    assert(getAddressSpace() == qs.getAddressSpace() ||
           !hasAddressSpace() || !qs.hasAddressSpace());
    assert(getIntentAttr() == qs.getIntentAttr() ||
           !hasIntentAttr() || !qs.hasIntentAttr());
    assert(getAttributeSpecs() == qs.getAttributeSpecs() ||
           !hasAttributeSpecs() || !qs.hasAttributeSpecs());
    Mask |= qs.Mask;
  }

  bool operator==(Qualifiers Other) const { return Mask == Other.Mask; }
  bool operator!=(Qualifiers Other) const { return Mask != Other.Mask; }

  operator bool() const { return hasQualifiers(); }

  Qualifiers &operator+=(Qualifiers R) {
    addQualifiers(R);
    return *this;
  }

  // Union two qualifier sets. If an enumerated qualifier appears in both sets,
  // use the one from the right.
  friend Qualifiers operator+(Qualifiers L, Qualifiers R) {
    L += R;
    return L;
  }

  Qualifiers &operator-=(Qualifiers R) {
    Mask = Mask & ~(R.Mask);
    return *this;
  }

  /// \brief Compute the difference between two qualifier sets.
  friend Qualifiers operator-(Qualifiers L, Qualifiers R) {
    L -= R;
    return L;
  }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    ID.AddInteger(Mask);
  }
};

typedef std::pair<const Type*, Qualifiers> SplitQualType;

/// QualType - For efficiency, some of the more common attributes are stored as
/// part of the type.
class QualType {
  // Thankfully, these are efficiently composable.
  llvm::PointerIntPair<llvm::PointerUnion<const Type*, const ExtQuals*>,
                       Qualifiers::FastWidth> Value;

  const ExtQuals *getExtQualsUnsafe() const {
    return Value.getPointer().get<const ExtQuals*>();
  }

  const Type *getTypePtrUnsafe() const {
    return Value.getPointer().get<const Type*>();
  }

  const ExtQualsTypeCommonBase *getCommonPtr() const {
    assert(!isNull() && "Cannot retrieve a NULL type pointer");
    uintptr_t CommonPtrVal
      = reinterpret_cast<uintptr_t>(Value.getOpaqueValue());
    CommonPtrVal &= ~(uintptr_t)((1 << TypeAlignmentInBits) - 1);
    return reinterpret_cast<ExtQualsTypeCommonBase*>(CommonPtrVal);
  }

  friend class QualifierCollector;
public:
  QualType() {}

  explicit QualType(const Type *Ptr, unsigned Quals)
    : Value(Ptr, Quals) {}
  explicit QualType(const ExtQuals *Ptr, unsigned Quals)
    : Value(Ptr, Quals) {}

  unsigned getLocalFastQualifiers() const { return Value.getInt(); }
  void setLocalFastQualifiers(unsigned Quals) { Value.setInt(Quals); }

  /// \brief Retrieves a pointer to the underlying (unqualified) type. This
  /// function requires that the type not be NULL. If the type might be NULL,
  /// use the (slightly less efficient) \c getTypePtrOrNull().
  const Type *getTypePtr() const;
  const Type *getTypePtrOrNull() const;

  /// \brief Divides a QualType into its unqualified type and a set of local
  /// qualifiers.
  SplitQualType split() const;

  /// \brief Returns the qualifiers for this type has.
  Qualifiers getQualifiers() const {
    return split().second;
  }

  /// \brief Returns true if this type has the given attribute spec.
  bool hasAttributeSpec(Qualifiers::AS A) const {
    return split().second.hasAttributeSpec(A);
  }

  void *getAsOpaquePtr() const { return Value.getOpaqueValue(); }

  static QualType getFromOpaquePtr(const void *Ptr) {
    QualType T;
    T.Value.setFromOpaqueValue(const_cast<void*>(Ptr));
    return T;
  }

  void addFastQualifiers(unsigned TQs) {
    assert(!(TQs & ~Qualifiers::FastMask)
           && "Non-fast qualifier bits set in mask!");
    Value.setInt(Value.getInt() | TQs);
  }

  // Qualifiers already on this type.
  QualType withFastQualifiers(unsigned TQs) const {
    QualType T = *this;
    T.addFastQualifiers(TQs);
    return T;
  }

  QualType getCanonicalType() const;

  /// \brief If this QualType instance is an array type,
  /// return the element type of this array type,
  /// otherwise return this QualType instance.
  QualType getSelfOrArrayElementType() const;

  /// \brief Determine whether this particular QualType instance has any
  /// "non-fast" qualifiers, e.g., those that are stored in an ExtQualType
  /// instance.
  bool hasLocalNonFastQualifiers() const {
    return Value.getPointer().is<const ExtQuals*>();
  }

  bool isCanonical() const;

  /// isNull - Return true if this QualType doesn't point to a type yet.
  bool isNull() const {
    return Value.getPointer().isNull();
  }

  /// \brief Determine whether this particular QualType instance has any
  /// qualifiers, without looking through any typedefs that might add 
  /// qualifiers at a different level.
  bool hasLocalQualifiers() const {
    return getLocalFastQualifiers() || hasLocalNonFastQualifiers();
  }

  const Type &operator*() const {
    return *getTypePtr();
  }
  const Type *operator->() const {
    return getTypePtr();
  }

  /// operator==/!= - Indicate whether the specified types and qualifiers are
  /// identical.
  friend bool operator==(const QualType &LHS, const QualType &RHS) {
    return LHS.Value == RHS.Value;
  }
  friend bool operator!=(const QualType &LHS, const QualType &RHS) {
    return LHS.Value != RHS.Value;
  }

  void print(raw_ostream &OS) const;
  void dump() const;

  void Profile(llvm::FoldingSetNodeID &ID) const {
    ID.AddPointer(getAsOpaquePtr());
  }
};

} // end flang namespace

namespace llvm {

/// Implement simplify_type for QualType, so that we can dyn_cast from QualType
/// to a specific Type class.
template<> struct simplify_type<const ::flang::QualType> {
  typedef const ::flang::Type *SimpleType;
  static SimpleType getSimplifiedValue(const ::flang::QualType &Val) {
    return Val.getTypePtr();
  }
};
template<> struct simplify_type< ::flang::QualType>
  : public simplify_type<const ::flang::QualType> {};

// Teach SmallPtrSet that QualType is "basically a pointer".
template<>
class PointerLikeTypeTraits<flang::QualType> {
public:
  static inline void *getAsVoidPointer(flang::QualType P) {
    return P.getAsOpaquePtr();
  }
  static inline flang::QualType getFromVoidPointer(void *P) {
    return flang::QualType::getFromOpaquePtr(P);
  }
  // Various qualifiers go in low bits.
  enum { NumLowBitsAvailable = 0 };
};

} // end namespace llvm

namespace flang {

/// \brief Base class that is common to both the \c ExtQuals and \c Type 
/// classes, which allows \c QualType to access the common fields between the
/// two.
///
class ExtQualsTypeCommonBase {
  ExtQualsTypeCommonBase(const Type *BaseTy, QualType Canon)
    : BaseType(BaseTy), CanonicalType(Canon) {}

  /// \brief The "base" type of an extended qualifiers type (\c ExtQuals) or
  /// a self-referential pointer (for \c Type).
  ///
  /// This pointer allows an efficient mapping from a QualType to its 
  /// underlying type pointer.
  const Type *const BaseType;

  /// \brief The canonical type of this type.  A QualType.
  QualType CanonicalType;

  friend class QualType;
  friend class Type;
  friend class ExtQuals;
};

class ArrayType;

/// Type - This is the base class for the type hierarchy.
///
/// Types are immutable once created.
class Type : public ExtQualsTypeCommonBase {
public:
  /// TypeClass - The intrinsic Fortran type specifications. REAL is the default
  /// if "IMPLICIT NONE" isn't specified.
  enum TypeClass {
#define TYPE(Name, Parent) Name,
#include "flang/AST/TypeNodes.def"
    None
  };

  enum TypeKind {
#define INTEGER_KIND(NAME, VALUE) NAME,
#define FLOATING_POINT_KIND(NAME, VALUE) NAME,
#include "flang/AST/BuiltinTypeKinds.def"
    NoKind
  };

private:
  Type(const Type&);           // DO NOT IMPLEMENT.
  void operator=(const Type&); // DO NOT IMPLEMENT.

protected:
  /// TypeClass bitfield - Enum that specifies what subclass this belongs to.
  unsigned TypeBitsTC : 8;
  /// The spec (BuiltinType::TypeSpec) of builtin type this is.
  unsigned BuiltinTypeBitsSpec : 8;
  unsigned BuiltinTypeBitsKind : 8;
  unsigned BuiltinTypeKindSpecified : 1;
  unsigned BuiltinTypeDoublePrecisionKindSpecified : 1;

  Type *this_() { return this; }
  Type(TypeClass TC, QualType Canon)
    : ExtQualsTypeCommonBase(this,
                             Canon.isNull() ? QualType(this_(), 0) : Canon) {
    TypeBitsTC = TC;
    BuiltinTypeBitsKind = NoKind;
  }

  friend class ASTContext;
public:
  TypeClass getTypeClass() const { return TypeClass(TypeBitsTC); }

  /// \brief Determines if this type would be canonical if it had no further
  /// qualification.
  bool isCanonicalUnqualified() const {
    return CanonicalType == QualType(this, 0);
  }

  QualType getCanonicalTypeInternal() const {
    return CanonicalType;
  }

  /// Helper methods to distinguish type categories. All type predicates
  /// operate on the canonical type ignoring qualifiers.

  bool isVoidType() const;

  /// isBuiltinType - returns true if the type is a builtin type.
  bool isBuiltinType() const;
  bool isIntegerType() const;
  bool isRealType() const;
  bool isComplexType() const;
  bool isLogicalType() const;
  const BuiltinType *asBuiltinType() const;
  TypeKind getBuiltinTypeKind() const;

  bool isCharacterType() const;
  const CharacterType *asCharacterType() const;

  bool isArrayOfCharacterType() const;
  bool isArrayType() const;
  const ArrayType *asArrayType() const;

  bool isFunctionType() const;
  const FunctionType *asFunctionType() const;

  bool isRecordType() const;
  const RecordType *asRecordType() const;

  static bool classof(const Type *) { return true; }
};

/// VoidType
class VoidType : public Type {
protected:
  friend class ASTContext;      // ASTContext creates these.
  VoidType()
    : Type(Void, QualType()) {}

public:
  static bool classof(const Type *T) { return T->getTypeClass() == Void; }
  static bool classof(const VoidType *) { return true; }
};

/// BuiltinType - Intrinsic Fortran types.
class BuiltinType : public Type, public llvm::FoldingSetNode {
public:
  /// TypeSpec - The intrinsic Fortran type specifications. REAL is the default
  /// if "IMPLICIT NONE" isn't specified.
  enum TypeSpec {
#define BUILTIN_TYPE(Name) Name,
#include "flang/AST/TypeNodes.def"
    Invalid
  };

private:
  friend class ASTContext;      // ASTContext creates these.
  BuiltinType(TypeSpec TS, TypeKind K,
              bool IsKindSpecified,
              bool IsDoublePrecisionKindSpecified)
    : Type(Builtin, QualType()) {
    BuiltinTypeBitsSpec = TS;
    BuiltinTypeBitsKind = K;
    BuiltinTypeKindSpecified = IsKindSpecified? 1: 0;
    BuiltinTypeDoublePrecisionKindSpecified = IsDoublePrecisionKindSpecified? 1: 0;
  }
public:
  TypeSpec getTypeSpec() const { return TypeSpec(BuiltinTypeBitsSpec); }

  bool isKindExplicitlySpecified() const {
    return BuiltinTypeKindSpecified != 0;
  }

  bool isDoublePrecisionKindSpecified() const {
    return BuiltinTypeDoublePrecisionKindSpecified != 0;
  }

  bool isIntegerType() const {
    return getTypeSpec() == Integer;
  }
  bool isRealType() const {
    return getTypeSpec() == Real;
  }
  bool isComplexType() const {
    return getTypeSpec() == Complex;
  }
  bool isIntegerOrRealType() const {
    auto Spec = getTypeSpec();
    return Spec == Integer || Spec == Real;
  }
  bool isIntegerOrRealOrComplexType() const {
    auto Spec = getTypeSpec();
    return Spec == Integer || Spec == Real || Spec == Complex;
  }
  bool isRealOrComplexType() const {
    auto Spec = getTypeSpec();
    return Spec == Real || Spec == Complex;
  }

  static const char *getTypeKindString(TypeKind Kind);

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getTypeSpec(), getBuiltinTypeKind(),
            isKindExplicitlySpecified(),
            isDoublePrecisionKindSpecified());
  }
  static void Profile(llvm::FoldingSetNodeID &ID,
                      TypeSpec Spec, TypeKind Kind,
                      bool KindSpecified,
                      bool DoublePrecisionKindSpecified) {
    ID.AddInteger(Spec);
    ID.AddInteger(Kind);
    ID.AddBoolean(KindSpecified);
    ID.AddBoolean(DoublePrecisionKindSpecified);
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Builtin; }
  static bool classof(const BuiltinType *) { return true; }
};

/// CharacterType - Character types.
class CharacterType : public Type, public llvm::FoldingSetNode {
  uint64_t Length;

  friend class ASTContext;
  CharacterType(uint64_t Len)
    : Type(Character, QualType()), Length(Len) {}
public:

  bool hasLength() const {
    return Length != 0;
  }
  uint64_t getLength() const {
    return Length;
  }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, Length);
  }
  static void Profile(llvm::FoldingSetNodeID &ID,
                      uint64_t Length) {
    ID.AddInteger(Length);
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Character; }
  static bool classof(const CharacterType *) { return true; }
};

/// PointerType - Allocatable types.
class PointerType : public Type, public llvm::FoldingSetNode {
  const Type *BaseType;     //< The type of the object pointed to.
  unsigned NumDims;
  friend class ASTContext;  // ASTContext creates these.
  PointerType(const Type *BaseTy, unsigned Dims)
    : Type(Pointer, QualType()), BaseType(BaseTy), NumDims(Dims) {}
public:
  const Type *getPointeeType() const { return BaseType; }
  unsigned getNumDimensions() const { return NumDims; }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getPointeeType(), getNumDimensions());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, const Type *ElemTy,
                      unsigned NumDims) {
    ID.AddPointer(ElemTy);
    ID.AddInteger(NumDims);
  }

  void print(llvm::raw_ostream &O) const {} // FIXME

  static bool classof(const Type *T) { return T->getTypeClass() == Pointer; }
  static bool classof(const PointerType *) { return true; }
};

class ArraySpec;

/// ArrayType - Array types.
class ArrayType : public Type, public llvm::FoldingSetNode {
private:
  QualType ElementType;
  ArraySpec **Dims;
  unsigned DimCount;
protected:
  ArrayType(TypeClass tc, QualType et, QualType can)
    : Type(tc, can), ElementType(et),
      Dims(nullptr), DimCount(0) {}
  ArrayType(ASTContext &C, TypeClass tc,
            QualType et, QualType can,
            ArrayRef<ArraySpec*> dims);

  friend class ASTContext;  // ASTContext creates these.
public:

  static ArrayType *Create(ASTContext &C, QualType ElemTy,
                           ArrayRef<ArraySpec*> Dims);

  QualType getElementType() const { return ElementType; }

  typedef ArraySpec**       dim_iterator;

  dim_iterator begin() const { return Dims; }
  dim_iterator end()   const { return Dims + DimCount; }
  size_t getDimensionCount() const { return DimCount; }
  ArrayRef<ArraySpec*> getDimensions() const {
    return ArrayRef<ArraySpec*>(Dims, DimCount);
  }

  /// EvaluateSize - Return true if the size of this array is a constant.
  bool EvaluateSize(uint64_t &Result, const ASTContext &Ctx) const;

  void Profile(llvm::FoldingSetNodeID &ID) const {
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == Array;
  }
  static bool classof(const ArrayType *) { return true; }
};

/// FunctionType - Function types.
class FunctionType : public Type, public llvm::FoldingSetNode {
private:
  QualType ResultType;
  const FunctionDecl *Prototype;
  FunctionType(TypeClass tc, QualType can, QualType Result,
               const FunctionDecl *Proto)
    : Type(tc, can), ResultType(Result), Prototype(Proto) {}
public:

  static FunctionType *Create(ASTContext &C, QualType ResultType,
                              const FunctionDecl *Prototype = nullptr);

  QualType getReturnType() const {
    return ResultType;
  }

  const FunctionDecl *getPrototype() const {
    return Prototype;
  }

  bool hasPrototype() const {
    return Prototype != nullptr;
  }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    Profile(ID, ResultType, Prototype);
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType Result,
                      const FunctionDecl *Proto) {
    ID.AddPointer(Result.getAsOpaquePtr());
    ID.AddPointer(Proto);
  }


  static bool classof(const Type *T) {
    return T->getTypeClass() == Function;
  }
  static bool classof(const FunctionType *) { return true; }
};

/// RecordType - Record types.
class RecordType : public Type, public llvm::FoldingSetNode {
  const RecordDecl *RecDecl;
  FieldDecl** Elems;
  unsigned ElemCount;

protected:
  friend class ASTContext;  // ASTContext creates these.
  RecordType(ASTContext &C, const RecordDecl *RD, ArrayRef<FieldDecl*> Elements);
public:

  FieldDecl *getElement(unsigned Idx) const { return Elems[Idx]; }

  ArrayRef<FieldDecl*> getElements() const {
    return ArrayRef<FieldDecl*>(Elems,ElemCount);
  }

  unsigned getElementCount() const {
    return ElemCount;
  }

  const RecordDecl *getDecl() const {
    return RecDecl;
  }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, RecDecl, getElements());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, const RecordDecl *RecDecl,
                      ArrayRef<FieldDecl*> Elems) {
    ID.AddPointer(RecDecl);
    for (auto I : Elems)
      ID.AddPointer(I);
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Record; }
  static bool classof(const RecordType *) { return true; }
};

/// ExtQuals - We can encode up to four bits in the low bits of a type pointer,
/// but there are many more type qualifiers that we want to be able to apply to
/// an arbitrary type.  Therefore we have this struct, intended to be
/// heap-allocated and used by QualType to store qualifiers.
class ExtQuals : public ExtQualsTypeCommonBase, public llvm::FoldingSetNode {
  /// Quals - the immutable set of qualifiers applied by this node; always
  /// contains extended qualifiers.
  Qualifiers Quals;

  ExtQuals *this_() { return this; }

public:
  ExtQuals(const Type *BaseTy, QualType Canon, Qualifiers Quals)
    : ExtQualsTypeCommonBase(BaseTy,
                             Canon.isNull() ? QualType(this_(), 0) : Canon),
      Quals(Quals)
  {}

  Qualifiers getQualifiers() const { return Quals; }

  bool hasAttributeSpecs() const { return Quals.hasAttributeSpecs(); }
  unsigned getAttributeSpecs() const { return Quals.getAttributeSpecs(); }

  bool hasIntentAttr() const { return Quals.hasIntentAttr(); }
  unsigned getIntentAttr() const { return Quals.getIntentAttr(); }

  bool hasAddressSpace() const { return Quals.hasAddressSpace(); }
  unsigned getAddressSpace() const { return Quals.getAddressSpace(); }

  const Type *getBaseType() const { return BaseType; }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    Profile(ID, getBaseType(), Quals);
  }
  static void Profile(llvm::FoldingSetNodeID &ID,
                      const Type *BaseType, Qualifiers Quals) {
    ID.AddPointer(BaseType);
    Quals.Profile(ID);
  }
};

/// A qualifier set is used to build a set of qualifiers.
class QualifierCollector : public Qualifiers {
public:
  QualifierCollector(Qualifiers Qs = Qualifiers()) : Qualifiers(Qs) {}

  /// Collect any qualifiers on the given type and return an
  /// unqualified type.  The qualifiers are assumed to be consistent
  /// with those already in the type.
  const Type *strip(QualType type) {
    addFastQualifiers(type.getLocalFastQualifiers());
    if (!type.hasLocalNonFastQualifiers())
      return type.getTypePtrUnsafe();
      
    const ExtQuals *extQuals = type.getExtQualsUnsafe();
    addConsistentQualifiers(extQuals->getQualifiers());
    return extQuals->getBaseType();
  }

  /// Apply the collected qualifiers to the given type.
  QualType apply(const ASTContext &Context, QualType QT) const;

  /// Apply the collected qualifiers to the given type.
  QualType apply(const ASTContext &Context, const Type* T) const;
};

// Inline function definitions.

inline const Type *QualType::getTypePtr() const {
  return getCommonPtr()->BaseType;
}
inline const Type *QualType::getTypePtrOrNull() const {
  return (isNull() ? 0 : getCommonPtr()->BaseType);
}
inline bool QualType::isCanonical() const {
  return getTypePtr()->isCanonicalUnqualified();
}
inline SplitQualType QualType::split() const {
  if (!hasLocalNonFastQualifiers())
    return SplitQualType(getTypePtrUnsafe(),
                         Qualifiers::fromFastMask(getLocalFastQualifiers()));

  const ExtQuals *EQ = getExtQualsUnsafe();
  Qualifiers Qs = EQ->getQualifiers();
  Qs.addFastQualifiers(getLocalFastQualifiers());
  return SplitQualType(EQ->getBaseType(), Qs);
}
inline QualType QualType::getCanonicalType() const {
  QualType canon = getCommonPtr()->CanonicalType;
  return canon.withFastQualifiers(getLocalFastQualifiers());
}

inline QualType QualType::getSelfOrArrayElementType() const {
  if(auto ATy = getTypePtr()->asArrayType())
    return ATy->getElementType();
  return *this;
}

inline bool Type::isVoidType() const {
  return isa<VoidType>(CanonicalType);
}
inline bool Type::isBuiltinType() const {
  return isa<BuiltinType>(CanonicalType);
}
inline bool Type::isIntegerType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getTypeSpec() == BuiltinType::Integer;
  return false;
}
inline bool Type::isRealType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getTypeSpec() == BuiltinType::Real;
  return false;
}
inline bool Type::isLogicalType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getTypeSpec() == BuiltinType::Logical;
  return false;
}
inline bool Type::isComplexType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getTypeSpec() == BuiltinType::Complex;
  return false;
}
inline const BuiltinType *Type::asBuiltinType() const {
  return dyn_cast<BuiltinType>(CanonicalType);
}
inline BuiltinType::TypeKind Type::getBuiltinTypeKind() const {
  return TypeKind(BuiltinTypeBitsKind);
}
inline bool Type::isCharacterType() const {
  return isa<CharacterType>(CanonicalType);
}
inline const CharacterType *Type::asCharacterType() const {
  return dyn_cast<CharacterType>(CanonicalType);
}
inline bool Type::isArrayType() const {
  return isa<ArrayType>(CanonicalType);
}
inline bool Type::isArrayOfCharacterType() const {
  if(const ArrayType *AT = dyn_cast<ArrayType>(CanonicalType)) {
    return AT->getElementType()->isCharacterType();
  }
  return false;
}
inline const ArrayType *Type::asArrayType() const {
  if(const ArrayType *AT = dyn_cast<ArrayType>(CanonicalType)) {
    return AT;
  }
  return 0;
}

inline bool Type::isFunctionType() const {
  return isa<FunctionType>(CanonicalType);
}

inline const FunctionType *Type::asFunctionType() const {
  return dyn_cast<FunctionType>(CanonicalType);
}

inline bool Type::isRecordType() const {
  return isa<RecordType>(CanonicalType);
}

inline const RecordType *Type::asRecordType() const {
  return dyn_cast<RecordType>(CanonicalType);
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           QualType T) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(T.getAsOpaquePtr()),
                  DiagnosticsEngine::ak_qualtype);
  return DB;
}

} // end flang namespace

#endif
