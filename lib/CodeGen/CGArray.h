//===--- CGArray.cpp - Emit LLVM Code for Array operations and Expr -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGARRAY_H
#define FLANG_CODEGEN_CGARRAY_H

#include "CGValue.h"
#include "CodeGenFunction.h"
#include "flang/AST/ExprVisitor.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"

namespace flang {
namespace CodeGen {

/// ArrayValueExprEmitter - Emits the information about an array.
class ArrayValueExprEmitter
  : public ConstExprVisitor<ArrayValueExprEmitter> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;

  llvm::Value *Offset;
  llvm::Value *Ptr;
  bool GetPointer;
  SmallVector<ArrayDimensionValueTy, 8> Dims;

  void EmitSections();
  void IncrementOffset(llvm::Value *OffsetDelta);
public:

  ArrayValueExprEmitter(CodeGenFunction &cgf, bool getPointer = true);

  void EmitExpr(const Expr *E);
  void VisitVarExpr(const VarExpr *E);
  void VisitArrayConstructorExpr(const ArrayConstructorExpr *E);
  void VisitArraySectionExpr(const ArraySectionExpr *E);

  ArrayRef<ArrayDimensionValueTy> getDimensions() const {
    return Dims;
  }
  llvm::Value *getPointer() const {
    return Ptr;
  }
  ArrayValueRef getResult() const {
    return ArrayValueRef(Dims, Ptr, Offset);
  }
};

/// ArrayOperation - Represents an array expression / statement.
/// Stores the array sections and scalars used in the array operation.
class ArrayOperation {
  struct StoredArrayValue {
    size_t DataOffset;
    llvm::Value *Ptr;
    llvm::Value *Offset;
  };
  llvm::SmallDenseMap<const Expr*, StoredArrayValue, 8> Arrays;
  llvm::SmallDenseMap<const Expr*, RValueTy, 8> Scalars;

  SmallVector<ArrayDimensionValueTy, 32> Dims;

protected:

  /// \brief Emits a scalar value used for the given scalar expression.
  void EmitScalarValue(CodeGenFunction &CGF, const Expr *E);

  /// \brief Emits the array sections used for the given expression.
  void EmitArraySections(CodeGenFunction &CGF, const Expr *E);

  friend class ScalarEmitterAndSectionGatherer;
public:

  /// \brief Returns the array value used for the given expression.
  ArrayValueRef getArrayValue(const Expr *E);

  /// \brief Returns the value used for the given scalar expression.
  RValueTy getScalarValue(const Expr *E);

  /// \brief Emits the array section used on the left side of an assignment
  /// in a multidimensional loop.
  ArrayValueRef EmitArrayExpr(CodeGenFunction &CGF, const Expr *E);

  /// EmitAllScalarValuesAndArraySections - walks the given expression,
  /// and prepares the array operation for a multidimensional loop by emitting
  /// scalar expressions, so that they are only executed once in an array operation,
  /// and also by emmitting the array sections which are used to access the array
  /// elements inside the operation's loop.
  void EmitAllScalarValuesAndArraySections(CodeGenFunction &CGF, const Expr *E);
};

/// StandaloneArrayValueSectionGatherer - Gathers the array sections
/// which are needed for a standalone array expression.
class StandaloneArrayValueSectionGatherer
  : public ConstExprVisitor<StandaloneArrayValueSectionGatherer> {
  CodeGenFunction &CGF;
  ArrayOperation &Operation;
  bool Gathered;
  SmallVector<ArrayDimensionValueTy, 8> Dims;

  void GatherSections(const Expr *E);
public:

  StandaloneArrayValueSectionGatherer(CodeGenFunction &cgf,
                                      ArrayOperation &Op);
  void EmitExpr(const Expr *E);

  void VisitVarExpr(const VarExpr *E);
  void VisitArrayConstructorExpr(const ArrayConstructorExpr *E);
  void VisitBinaryExpr(const BinaryExpr *E);
  void VisitUnaryExpr(const UnaryExpr *E);
  void VisitImplicitCastExpr(const ImplicitCastExpr *E);
  void VisitIntrinsicCallExpr(const IntrinsicCallExpr *E);
  void VisitArraySectionExpr(const ArraySectionExpr *E);

  ArrayValueRef getResult() const {
    return ArrayValueRef(Dims, nullptr);
  }
};

/// ArrayLoopEmitter - Emits the multidimensional loop which
/// is used to iterate over array sections in an array expression.
class ArrayLoopEmitter {
private:
  /// Loop - stores some information about the generated loops.
  struct Loop {
    llvm::BasicBlock *EndBlock;
    llvm::BasicBlock *TestBlock;
    llvm::Value *Counter;
  };

  CodeGenFunction &CGF;
  CGBuilderTy &Builder;

  /// ElementInfo - stores the current loop index for all
  /// dimensions, or null if the loop index doesn't apply
  /// (i.e. element section).
  SmallVector<llvm::Value *, 8> Elements;
  SmallVector<Loop, 8> Loops;
public:

  ArrayLoopEmitter(CodeGenFunction &cgf);

  /// EmitSectionIndex - computes the index of the element during
  /// the current iteration of the multidimensional loop
  /// for the given dimension.
  llvm::Value *EmitSectionOffset(const ArrayValueRef &Array,
                                int I);

  /// EmitElementOffset - computes the offset of the
  /// current element in the given array.
  llvm::Value *EmitElementOffset(const ArrayValueRef &Array);

  /// EmitElementOneDimensionalIndex - computes the
  /// index (which starts with 1) for the current element in the given array.
  llvm::Value *EmitElementOneDimensionalIndex(const ArrayValueRef &Array);

  /// EmitElelementPointer - returns the pointer to the current
  /// element in the given array.
  llvm::Value *EmitElementPointer(const ArrayValueRef &Array);

  /// EmitArrayIterationBegin - Emits the beginning of a
  /// multidimensional loop which iterates over the given array section.
  void EmitArrayIterationBegin(const ArrayValueRef &Array);

  /// EmitArrayIterationEnd - Emits the end of a
  /// multidimensional loop which iterates over the given array section.
  void EmitArrayIterationEnd();
};

/// ArrayOperationEmitter - Emits the array expression for the current
/// iteration of the multidimensional array loop.
class ArrayOperationEmitter : public ConstExprVisitor<ArrayOperationEmitter, RValueTy> {
  CodeGenFunction   &CGF;
  CGBuilderTy       &Builder;
  ArrayOperation    &Operation;
  ArrayLoopEmitter &Looper;
public:

  ArrayOperationEmitter(CodeGenFunction &cgf, ArrayOperation &Op,
                         ArrayLoopEmitter &Loop);

  RValueTy Emit(const Expr *E);
  RValueTy VisitVarExpr(const VarExpr *E);
  RValueTy VisitImplicitCastExpr(const ImplicitCastExpr *E);
  RValueTy VisitUnaryExpr(const UnaryExpr *E);
  RValueTy VisitBinaryExpr(const BinaryExpr *E);
  RValueTy VisitArrayConstructorExpr(const ArrayConstructorExpr *E);
  RValueTy VisitArraySectionExpr(const ArraySectionExpr *E);
  RValueTy VisitIntrinsicCallExpr(const IntrinsicCallExpr *E);

  static QualType ElementType(const Expr *E) {
    return cast<ArrayType>(E->getType().getTypePtr())->getElementType();
  }

  LValueTy EmitLValue(const Expr *E);
};

}
}  // end namespace flang

#endif
