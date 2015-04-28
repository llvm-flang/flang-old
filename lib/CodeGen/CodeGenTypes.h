//===--- CodeGenTypes.h - Type translation for LLVM CodeGen -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the code that handles AST -> LLVM type lowering.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CODEGENTYPES_H
#define FLANG_CODEGEN_CODEGENTYPES_H

#include "CGCall.h"
#include "CGABI.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Type.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Module.h"
#include <vector>

namespace llvm {
  class FunctionType;
  class Module;
  class DataLayout;
  class Type;
  class LLVMContext;
  class StructType;
  class IntegerType;
}

namespace flang {
  class ASTContext;
  class TargetInfo;

namespace CodeGen {
  class CodeGenModule;

class CGType {
  QualType ASTType;
  llvm::Type *LLVMType;
public:
  CGType() : LLVMType(nullptr) {}
  CGType(QualType T) : ASTType(T), LLVMType(nullptr) {}
  CGType(llvm::Type *T) : LLVMType(T) {}

  bool isQualType() const { return LLVMType == nullptr; }
  QualType asQualType() const {
    return ASTType;
  }
  llvm::Type *asLLVMType() const {
    return LLVMType;
  }
};

/// CodeGenTypes - This class organizes the cross-module state that is used
/// while lowering AST types to LLVM types.
class CodeGenTypes {
public:
  CodeGenModule &CGM;
  ASTContext &Context;
  FortranABI DefaultABI;

public:
  CodeGenTypes(CodeGenModule &cgm);
  ~CodeGenTypes();

  /// ConvertType - Convert type T into a llvm::Type.
  llvm::Type *ConvertType(QualType T);

  /// ConvertTypeForMem - Convert type T into a llvm::Type.  This differs from
  /// ConvertType in that it is used to convert to the memory representation for
  /// a type.  For example, the scalar representation for _Bool is i1, but the
  /// memory representation is usually i8 or i32, depending on the target.
  llvm::Type *ConvertTypeForMem(QualType T);

  const CGFunctionInfo *GetFunctionType(const FunctionDecl *FD);

  const CGFunctionInfo *GetFunctionType(FortranABI &ABI,
                                        ArrayRef<CGType> Args,
                                        CGType ReturnType);

  llvm::Type *GetComplexType(llvm::Type *ElementType);
  llvm::Type *GetComplexTypeAsVector(llvm::Type *ElementType);

  llvm::Type *GetCharacterType(llvm::Type *PtrType);

  llvm::Type *ConvertBuiltInType(BuiltinType::TypeSpec Spec,
                                 BuiltinType::TypeKind Kind);

  llvm::Type *ConvertCharType(const CharacterType *T);
  llvm::Type *ConvertCharTypeForMem(const CharacterType *T);

  llvm::ArrayType *ConvertArrayTypeForMem(const ArrayType *T);

  llvm::Type *ConvertArrayType(const ArrayType *T);

  llvm::ArrayType *GetFixedSizeArrayType(const ArrayType *T,
                                         uint64_t Size);

  llvm::Type *ConvertRecordType(const RecordType *T);

  llvm::Type *ConvertFunctionType(const FunctionType *T);

  llvm::Type *ConvertReturnType(QualType T,
                                CGFunctionInfo::RetInfo &ReturnInfo);

  void ConvertArgumentType(SmallVectorImpl<llvm::Type *> &ArgTypes,
                           SmallVectorImpl<llvm::Type *> &AdditionalArgTypes,
                           QualType T,
                           const CGFunctionInfo::ArgInfo &ArgInfo);

  void ConvertArgumentTypeForReturnValue(SmallVectorImpl<CGFunctionInfo::ArgInfo> &ArgInfo,
                                         SmallVectorImpl<llvm::Type *> &ArgTypes,
                                         SmallVectorImpl<llvm::Type *> &AdditionalArgTypes,
                                         QualType T,
                                         const CGFunctionInfo::RetInfo &ReturnInfo);

};

}  // end namespace CodeGen
}  // end namespace flang

#endif
