//===-- CGValue.h - LLVM CodeGen wrappers for llvm::Value* ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These classes implement wrappers around llvm::Value in order to
// fully represent the range of values for Complex, Character, Array and L/Rvalues.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGVALUE_H
#define FLANG_CODEGEN_CGVALUE_H

#include "flang/AST/ASTContext.h"
#include "flang/AST/Type.h"
#include "llvm/IR/Value.h"

namespace llvm {
  class Constant;
  class MDNode;
}

namespace flang {
namespace CodeGen {

class ComplexValueTy {
public:
  llvm::Value *Re, *Im;

  ComplexValueTy() {}
  ComplexValueTy(llvm::Value *Real, llvm::Value *Imaginary)
    : Re(Real), Im(Imaginary) {}
};

class CharacterValueTy {
public:
  llvm::Value *Ptr, *Len;

  CharacterValueTy() {}
  CharacterValueTy(llvm::Value *Pointer, llvm::Value *Length)
    : Ptr(Pointer), Len(Length) {}
};

/// ArrayDimensionValueTy - this is a single dimension
/// of an array. All values have SizeTy type.
class ArrayDimensionValueTy {
public:
  llvm::Value *LowerBound;
  llvm::Value *UpperBound;
  llvm::Value *Stride;

  ArrayDimensionValueTy() {}
  ArrayDimensionValueTy(llvm::Value *LB, llvm::Value *UB = nullptr,
                        llvm::Value *stride = nullptr)
    : LowerBound(LB), UpperBound(UB), Stride(stride) {}

  bool hasLowerBound() const {
    return LowerBound != nullptr;
  }
  bool hasUpperBound() const {
    return UpperBound != nullptr;
  }
  bool hasStride() const {
    return Stride != nullptr;
  }
};

/// ArrayValueRef - this a reference to an array
/// value.
class ArrayValueRef {
public:
  ArrayRef<ArrayDimensionValueTy> Dimensions;
  llvm::Value *Ptr;
  llvm::Value *Offset;

  ArrayValueRef(ArrayRef<ArrayDimensionValueTy> Dims,
               llvm::Value *P,
               llvm::Value *offset = nullptr)
    : Dimensions(Dims), Ptr(P),
      Offset(offset) {}

  bool hasOffset() const {
    return Offset != nullptr;
  }
};

/// ArrayVectorValueTy - this is a one dimensional
/// array value.
class ArrayVectorValueTy {
public:
  ArrayDimensionValueTy Dimension;
  llvm::Value *Ptr;

  ArrayVectorValueTy(ArrayDimensionValueTy Dim,
                     llvm::Value *P)
    : Dimension(Dim), Ptr(P) {}
};

class LValueTy {
public:
  llvm::Value *Ptr;
  QualType Type;

  LValueTy() {}
  LValueTy(llvm::Value *Dest)
    : Ptr(Dest) {}
  LValueTy(llvm::Value *Dest, QualType Ty)
    : Ptr(Dest), Type(Ty) {}

  llvm::Value *getPointer() const {
    return Ptr;
  }
  QualType getType() const {
    return Type;
  }

  bool isVolatileQualifier() const {
    return false;// NB: to be used in the future
  }
};

class RValueTy {
public:
  enum Kind {
    None,
    Scalar,
    Complex,
    Character,
    Aggregate
  };
private:
  Kind ValueType;
  llvm::Value *V1;
  llvm::Value *V2;

  RValueTy(llvm::Value *V, Kind Type)
    : V1(V), ValueType(Type) {}
public:

  RValueTy() : ValueType(None) {}
  RValueTy(llvm::Value *V)
    : V1(V), ValueType(Scalar) {}
  RValueTy(ComplexValueTy C)
    : V1(C.Re), V2(C.Im), ValueType(Complex) {}
  RValueTy(CharacterValueTy CharValue)
    : V1(CharValue.Ptr), V2(CharValue.Len), ValueType(Character) {}

  Kind getType() const {
    return ValueType;
  }
  bool isScalar() const {
    return getType() == Scalar;
  }
  bool isComplex() const {
    return getType() == Complex;
  }
  bool isCharacter() const {
    return getType() == Character;
  }
  bool isAggregate() const {
    return getType() == Aggregate;
  }
  bool isNothing() const {
    return getType() == None;
  }

  bool isVolatileQualifier() const {
    return false;// NB: to be used in the future
  }

  llvm::Value *asScalar() const {
    return V1;
  }
  ComplexValueTy asComplex() const {
    return ComplexValueTy(V1, V2);
  }
  CharacterValueTy asCharacter() const {
    return CharacterValueTy(V1, V2);
  }
  llvm::Value *getAggregateAddr() const {
    assert(isAggregate());
    return V1;
  }

  static RValueTy getAggregate(llvm::Value *Ptr, bool isVolatile = false) {
    return RValueTy(Ptr, Aggregate);
  }
};

}  // end namespace CodeGen
}  // end namespace flang

#endif
