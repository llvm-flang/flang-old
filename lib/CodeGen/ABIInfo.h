//===----- ABIInfo.h - ABI information access & encapsulation ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_ABIINFO_H
#define FLANG_CODEGEN_ABIINFO_H

#include "flang/AST/Type.h"

namespace llvm {

class Type;

}

namespace flang {

/// ABIArgInfo - Helper class to encapsulate information about how a
/// specific Fortran type should be passed to a function.
class ABIArgInfo {
public:
  enum Kind {
    /// Passes by value.
    /// scalar - simple value.
    /// complex - aggregate value (real, im)
    /// character - aggregate value (ptr, len)
    Value,

    /// Passes a scalar/complex by reference
    /// Passes an array without shape info as a
    /// pointer to the elements
    Reference,

    /// Passes a scalar/complex as two arguments
    /// pointer argument - void*
    /// size argument - int32 (sizeof type)
    ReferenceAsVoidExtraSize,

    /// Passes an aggregate as separate arguments
    /// complex - two arguments (real, im)
    /// character - two arguments (ptr, len)
    Expand,

    /// Passes a character aggregate as two separate
    /// arguments. The pointer is passed in place of
    /// the actual argument, and the length is passed
    /// in the additional arguments as an integer.
    ExpandCharacterPutLengthToAdditionalArgsAsInt,

    /// Passes a complex by value using a vector type.
    ComplexValueAsVector
  };

private:
  Kind TheKind;
public:

  ABIArgInfo(Kind K) :
    TheKind(K) {}

  Kind getKind() const { return TheKind; }
};

/// ABIRetInfo - Helper class to encapsulate information about how a
/// specific Fortran type should be returned from a function.
class ABIRetInfo {
public:
  enum Kind {
    /// Returns void
    Nothing,

    /// Returns a value
    /// scalar - simple value
    /// complex - aggregate value (real, im)
    Value,

    /// Returns a character value using an argument
    CharacterValueAsArg,

    /// Returns an aggregate value(complex or struct) using an argument
    AggregateValueAsArg
  };
private:
  Kind  TheKind;
  llvm::Type *AggT;
public:

  ABIRetInfo(Kind K = Nothing) :
    TheKind(K), AggT(nullptr) {}
  ABIRetInfo(Kind K, llvm::Type *T) :
    TheKind(K), AggT(T) {}

  Kind getKind() const { return TheKind; }

  llvm::Type *getAggregateReturnType() const {
    return AggT;
  }

  bool hasAggregateReturnType() const {
    return AggT != nullptr;
  }
};

/// ABIInfo - Target specific hooks for defining how a type should be
/// passed or returned from functions.
class ABIInfo {
public:
  virtual ~ABIInfo() {}
  virtual void computeReturnTypeInfo(QualType T, ABIRetInfo &Info) const = 0;
};

}  // end namespace flang

#endif
