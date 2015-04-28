//===-- CodeGenFunction.h - Per-Function state for LLVM CodeGen -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-function state used for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_CODEGENFUNCTION_H
#define CLANG_CODEGEN_CODEGENFUNCTION_H

#include "CGBuilder.h"
#include "CGValue.h"
#include "CodeGenModule.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Type.h"
//#include "flang/Basic/TargetInfo.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/ValueHandle.h"

namespace llvm {
  class BasicBlock;
  class LLVMContext;
  class MDNode;
  class Module;
  class SwitchInst;
  class Twine;
  class Value;
  class CallSite;
  class Type;
}

namespace flang {
  class ASTContext;
  class Decl;

namespace CodeGen {
  class CodeGenTypes;

  class LoopScope;
  class StatementFunctionInliningScope;

/// CodeGenFunction - This class organizes the per-function state that is used
/// while generating LLVM code.
class CodeGenFunction {
  CodeGenFunction(const CodeGenFunction &) = delete;
  void operator=(const CodeGenFunction &) = delete;

  CodeGenModule &CGM;  // Per-module state.
  //const TargetInfo &Target;

  CGBuilderTy Builder;

  /// CurFuncDecl - Holds the Decl for the current outermost
  /// non-closure context.
  const Decl *CurFuncDecl;
  /// CurCodeDecl - This is the inner-most code context, which includes blocks.
  const Decl *CurCodeDecl;
  QualType FnRetTy;
  llvm::Function *CurFn;

  llvm::BasicBlock *UnreachableBlock;

  llvm::BasicBlock *ReturnBlock;

  ArrayRef<CGFunctionInfo::ArgInfo> ArgsInfo;
  ArrayRef<VarDecl*> ArgsList;

  struct ExpandedArg {
    const VarDecl *Decl;
    llvm::Function::arg_iterator A1;
    llvm::Function::arg_iterator A2;
  };
  llvm::SmallVector<ExpandedArg, 4> ExpandedArgs;

  ExpandedArg GetExpandedArg(const VarDecl *Arg) const {
    for(auto I : ExpandedArgs) {
      if(I.Decl == Arg) return I;
    }
    return ExpandedArg();
  }

  llvm::DenseMap<const VarDecl*, llvm::Value*>   LocalVariables;
  llvm::DenseMap<const VarDecl*, CharacterValueTy> CharacterArgs;
  struct EquivSet {
    llvm::Value *Ptr;
    int64_t LowestBound;
  };
  llvm::SmallDenseMap<const EquivalenceSet*, EquivSet, 4> EquivSets;
  llvm::SmallDenseMap<const VarDecl*, int64_t, 16> LocalVariablesInEquivSets;
  llvm::SmallDenseMap<const CommonBlockSet*, llvm::Value*, 4> CommonBlocks;
  llvm::Value *ReturnValuePtr;
  llvm::Instruction *AllocaInsertPt;

  bool HasSavedVariables;

  llvm::DenseMap<const Stmt*, llvm::BasicBlock*> GotoTargets;
  llvm::SmallVector<const Stmt*, 8> AssignedGotoTargets;
  llvm::Value *AssignedGotoVarPtr;
  llvm::BasicBlock *AssignedGotoDispatchBlock;

  llvm::SmallVector<llvm::Value*, 8> TempHeapAllocations;

  bool IsMainProgram;

protected:
  const LoopScope *CurLoopScope;
  friend class LoopScope;
  const StatementFunctionInliningScope *CurInlinedStmtFunc;
  friend class StatementFunctionInliningScope;

public:
  CodeGenFunction(CodeGenModule &cgm, llvm::Function *Fn);
  ~CodeGenFunction();

  CodeGenModule &getModule() const {
    return CGM;
  }

  CodeGenTypes &getTypes() const {
    return CGM.getTypes();
  }

  ASTContext &getContext() const {
    return CGM.getContext();
  }

  CGBuilderTy &getBuilder() {
    return Builder;
  }

  llvm::BasicBlock *getUnreachableBlock() {
    if (!UnreachableBlock) {
      UnreachableBlock = createBasicBlock("unreachable");
      new llvm::UnreachableInst(getLLVMContext(), UnreachableBlock);
    }
    return UnreachableBlock;
  }
  //const TargetInfo &getTarget() const { return Target; }
  llvm::LLVMContext &getLLVMContext() { return CGM.getLLVMContext(); }

  llvm::Function *getCurrentFunction() const {
    return CurFn;
  }

  llvm::Type *ConvertTypeForMem(QualType T) const;
  llvm::Type *ConvertType(QualType T) const;

  /// createBasicBlock - Create an LLVM basic block.
  llvm::BasicBlock *createBasicBlock(const Twine &name = "",
                                     llvm::Function *parent = 0,
                                     llvm::BasicBlock *before = 0) {
#ifdef NDEBUG
    return llvm::BasicBlock::Create(getLLVMContext(), "", parent, before);
#else
    return llvm::BasicBlock::Create(getLLVMContext(), name, parent, before);
#endif
  }

  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    ArrayRef<llvm::Type*> ArgTypes) const;
  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    llvm::Type *T1) const;
  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    llvm::Type *T1, llvm::Type *T2) const;
  inline
  llvm::Value *GetIntrinsicFunction(int FuncID, QualType T1) const {
    return GetIntrinsicFunction(FuncID, ConvertType(T1));
  }
  inline
  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    QualType T1, QualType T2) const {
    return GetIntrinsicFunction(FuncID, ConvertType(T1), ConvertType(T2));
  }

  llvm::Value *GetVarPtr(const VarDecl *D);
  llvm::Value *GetRetVarPtr();
  const VarDecl *GetExternalFunctionArgument(const FunctionDecl *Func);

  /// \brief Returns the argument info for the given arg.
  CGFunctionInfo::ArgInfo GetArgInfo(const VarDecl *Arg) const;

  /// \brief Returns the value of the given character argument.
  CharacterValueTy GetCharacterArg(const VarDecl *Arg);

  void EmitFunctionDecls(const DeclContext *DC);
  void EmitMainProgramBody(const DeclContext *DC, const Stmt *S);
  void EmitFunctionArguments(const FunctionDecl *Func,
                             const CGFunctionInfo *Info);
  void EmitFunctionPrologue(const FunctionDecl *Func,
                            const CGFunctionInfo *Info);
  void EmitFunctionBody(const DeclContext *DC, const Stmt *S);
  void EmitFunctionEpilogue(const FunctionDecl *Func,
                            const CGFunctionInfo *Info);
  void EmitAggregateReturn(const CGFunctionInfo::RetInfo &Info, llvm::Value *Ptr);
  void EmitCleanup();

  void EmitVarDecl(const VarDecl *D);
  void EmitVarInitializers(const DeclContext *DC);
  void EmitSavedVarInitializers(const DeclContext *DC);
  void EmitVarInitializer(const VarDecl *D);
  void EmitFirstInvocationBlock(const DeclContext *DC, const Stmt *S);

  std::pair<int64_t, int64_t> GetObjectBounds(const VarDecl *Var, const Expr *E);
  EquivSet EmitEquivalenceSet(const EquivalenceSet *S);
  llvm::Value *EmitEquivalenceSetObject(EquivSet Set, const VarDecl *Var);

  void EmitCommonBlock(const CommonBlockSet *S);


  llvm::Value *CreateArrayAlloca(QualType T,
                                 const llvm::Twine &Name = "",
                                 bool IsTemp = false);

  /// CreateTempAlloca - This creates a alloca and inserts it into the entry
  /// block. The caller is responsible for setting an appropriate alignment on
  /// the alloca.
  llvm::AllocaInst *CreateTempAlloca(llvm::Type *Ty,
                                     const llvm::Twine &Name = "tmp");

  /// CreateTempHeapAlloca - This allocates an object using the
  /// runtime provided dynamic allocation mechanism.
  llvm::Value *CreateTempHeapAlloca(llvm::Value *Size);

  llvm::Value *CreateTempHeapAlloca(llvm::Value *Size, llvm::Type *PtrType);

  llvm::Value *CreateTempHeapArrayAlloca(QualType T,
                                         llvm::Value *Size);

  llvm::Value *CreateTempHeapArrayAlloca(QualType T,
                                         const ArrayValueRef &Value);

  void EmitBlock(llvm::BasicBlock *BB);
  void EmitBranch(llvm::BasicBlock *Target);
  void EmitBranchOnLogicalExpr(const Expr *Condition, llvm::BasicBlock *ThenBB,
                               llvm::BasicBlock *ElseBB);

  void EmitStmt(const Stmt *S);
  void EmitStmtLabel(const Stmt *S);
  llvm::BasicBlock *GetGotoTarget(const Stmt *S);

  void EmitGotoStmt(const GotoStmt *S);
  void EmitAssignStmt(const AssignStmt *S);
  void EmitAssignedGotoStmt(const AssignedGotoStmt *S);
  void EmitAssignedGotoDispatcher();
  void EmitComputedGotoStmt(const ComputedGotoStmt *S);
  void EmitIfStmt(const IfStmt *S);
  void EmitDoStmt(const DoStmt *S);
  void EmitDoWhileStmt(const DoWhileStmt *S);
  void EmitCycleStmt(const CycleStmt *S);
  void EmitExitStmt(const ExitStmt *S);
  void EmitSelectCaseStmt(const SelectCaseStmt *S);
  void EmitWhereStmt(const WhereStmt *S);
  void EmitStopStmt(const StopStmt *S);
  void EmitReturnStmt(const ReturnStmt *S);
  void EmitCallStmt(const CallStmt *S);
  void EmitAssignmentStmt(const AssignmentStmt *S);
  void EmitAssignment(const Expr *LHS, const Expr *RHS);
  void EmitAssignment(LValueTy LHS, RValueTy RHS);

  RValueTy EmitRValue(const Expr *E);
  LValueTy EmitLValue(const Expr *E);

  /// Generic value operations for scalar/complex/character values.
  RValueTy EmitLoad (llvm::Value *Ptr, QualType T, bool IsVolatile = false);
  void     EmitStore(RValueTy Val, LValueTy Dest, QualType T);
  void     EmitStoreCharSameLength(RValueTy Val, LValueTy Dest, QualType T);
  RValueTy EmitBinaryExpr(BinaryExpr::Operator Op, RValueTy LHS, RValueTy RHS);
  RValueTy EmitUnaryExpr(UnaryExpr::Operator Op, RValueTy Val);
  RValueTy EmitImplicitConversion(RValueTy Val, QualType T);

  llvm::Constant *EmitConstantExpr(const Expr *E);

  // scalar expressions.
  llvm::Value *EmitSizeIntExpr(const Expr *E);
  llvm::Value *EmitScalarExpr(const Expr *E);
  llvm::Value *EmitScalarExprOrNull(const Expr *E) {
    if(!E) return nullptr;
    return EmitScalarExpr(E);
  }
  llvm::Value *EmitSizeIntExprOrNull(const Expr *E) {
    if(!E) return nullptr;
    return EmitSizeIntExpr(E);
  }
  llvm::Value *EmitScalarUnaryMinus(llvm::Value *Val);
  llvm::Value *EmitScalarUnaryNot(llvm::Value *Val);
  llvm::Value *EmitScalarBinaryExpr(BinaryExpr::Operator Op,
                                    llvm::Value *LHS,
                                    llvm::Value *RHS);
  llvm::Value *EmitScalarPowIntInt(llvm::Value *LHS, llvm::Value *RHS);
  llvm::Value *EmitIntToInt32Conversion(llvm::Value *Value);
  llvm::Value *EmitSizeIntToIntConversion(llvm::Value *Value);
  llvm::Value *EmitScalarToScalarConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitLogicalConditionExpr(const Expr *E);
  llvm::Value *EmitLogicalValueExpr(const Expr *E);
  llvm::Value *ConvertLogicalValueToInt1(llvm::Value *Val) ;
  llvm::Value *ConvertLogicalValueToLogicalMemoryValue(llvm::Value *Val, QualType T);
  llvm::Value *EmitIntegerConstantExpr(const IntegerConstantExpr *E);
  llvm::Value *EmitScalarRelationalExpr(BinaryExpr::Operator Op, llvm::Value *LHS,
                                        llvm::Value *RHS);
  llvm::Value *ConvertComparisonResultToRelationalOp(BinaryExpr::Operator Op,
                                                     llvm::Value *Result);

  llvm::Value *GetConstantZero(llvm::Type *T);
  llvm::Value *GetConstantZero(QualType T) {
    return GetConstantZero(ConvertType(T));
  }
  llvm::Value *GetConstantOne(QualType T);
  llvm::Value *EmitFunctionPointer(const FunctionDecl *F);
  llvm::Value *GetConstantScalarMaxValue(QualType T);
  llvm::Value *GetConstantScalarMinValue(QualType T);
  llvm::Value *EmitBitOperation(intrinsic::FunctionKind Op,
                                llvm::Value *A1, llvm::Value *A2,
                                llvm::Value *A3);

  // complex expressions.
  ComplexValueTy ExtractComplexValue(llvm::Value *Agg);
  ComplexValueTy ExtractComplexVectorValue(llvm::Value *Agg);
  llvm::Value   *CreateComplexAggregate(ComplexValueTy Value);
  llvm::Value   *CreateComplexVector(ComplexValueTy Value);
  llvm::Constant *CreateComplexConstant(ComplexValueTy Value);
  ComplexValueTy EmitComplexExpr(const Expr *E);
  ComplexValueTy EmitComplexLoad(llvm::Value *Ptr, bool IsVolatile = false);
  void EmitComplexStore(ComplexValueTy Value, llvm::Value *Ptr,
                        bool IsVolatile = false);
  ComplexValueTy EmitComplexUnaryMinus(ComplexValueTy Val);
  ComplexValueTy EmitComplexBinaryExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                       ComplexValueTy RHS);
  ComplexValueTy EmitComplexDivSmiths(ComplexValueTy LHS, ComplexValueTy RHS);
  ComplexValueTy EmitComplexPowi(ComplexValueTy LHS, llvm::Value *RHS);
  ComplexValueTy EmitComplexPow(ComplexValueTy LHS, ComplexValueTy RHS);
  ComplexValueTy EmitComplexToComplexConversion(ComplexValueTy Value, QualType Target);
  ComplexValueTy EmitScalarToComplexConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitComplexToScalarConversion(ComplexValueTy Value, QualType Target);
  llvm::Value *EmitComplexRelationalExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                         ComplexValueTy RHS);

  // character expressions
  CharacterValueTy ExtractCharacterValue(llvm::Value *Agg);
  llvm::Value   *CreateCharacterAggregate(CharacterValueTy Value);
  void EmitCharacterAssignment(const Expr *LHS, const Expr *RHS);
  void EmitCharacterAssignment(CharacterValueTy LHS, CharacterValueTy RHS);
  llvm::Value *GetCharacterTypeLength(QualType T);
  CharacterValueTy GetCharacterValueFromPtr(llvm::Value *Ptr,
                                               QualType StorageType);
  CharacterValueTy EmitCharacterExpr(const Expr *E);
  llvm::Value *EmitCharacterRelationalExpr(BinaryExpr::Operator Op, CharacterValueTy LHS,
                                           CharacterValueTy RHS);
  RValueTy EmitIntrinsicCallCharacter(intrinsic::FunctionKind Func,
                                      CharacterValueTy Value);
  RValueTy EmitIntrinsicCallCharacter(intrinsic::FunctionKind Func,
                                      CharacterValueTy A1, CharacterValueTy A2);
  llvm::Value *EmitCharacterDereference(CharacterValueTy Value);

  // aggregate expressions

  RValueTy EmitAggregateExpr(const Expr *E);
  void EmitAggregateAssignment(const Expr *LHS, const Expr *RHS);
  llvm::Value *EmitAggregateMember(llvm::Value *Agg, const FieldDecl *Field);
  RValueTy EmitAggregateMember(const Expr *E, const FieldDecl *Field);

  // intrinsic calls
  RValueTy EmitIntrinsicCall(const IntrinsicCallExpr *E);
  llvm::Value *EmitIntrinsicCallScalarTruncation(intrinsic::FunctionKind Func,
                                                 llvm::Value *Value,
                                                 QualType ResultType);
  RValueTy EmitIntrinsicCallComplex(intrinsic::FunctionKind Func, ComplexValueTy Value);
  llvm::Value *EmitIntrinsicCallScalarMath(intrinsic::FunctionKind Func,
                                           llvm::Value *A1, llvm::Value *A2 = nullptr);
  llvm::Value *EmitIntrinsicMinMax(intrinsic::FunctionKind Func,
                                   ArrayRef<Expr*> Arguments);
  llvm::Value *EmitIntrinsicScalarMinMax(intrinsic::FunctionKind Func,
                                         ArrayRef<llvm::Value*> Args);
  RValueTy EmitIntrinsicCallComplexMath(intrinsic::FunctionKind Function,
                                        ComplexValueTy Value);

  llvm::Value *EmitIntrinsicNumericInquiry(intrinsic::FunctionKind Func,
                                           QualType ArgType, QualType Result);

  RValueTy EmitSystemIntrinsic(intrinsic::FunctionKind Func,
                               ArrayRef<Expr*> Arguments);

  llvm::Value *EmitInquiryIntrinsic(intrinsic::FunctionKind Func,
                                    ArrayRef<Expr*> Arguments);

  RValueTy EmitArrayIntrinsic(intrinsic::FunctionKind Func,
                              ArrayRef<Expr*> Arguments);

  RValueTy EmitVectorDimReturningScalarArrayIntrinsic(intrinsic::FunctionKind Func,
                                                      Expr *Arr);


  // calls
  RValueTy EmitCall(const CallExpr *E);
  RValueTy EmitCall(const FunctionDecl *Function,
                    CallArgList &ArgList,
                    ArrayRef<Expr *> Arguments,
                    bool ReturnsNothing = false);
  RValueTy EmitCall(llvm::Value *Callee,
                    const CGFunctionInfo *FuncInfo,
                    CallArgList &ArgList,
                    ArrayRef<Expr*> Arguments = ArrayRef<Expr*>(),
                    bool ReturnsNothing = false);
  RValueTy EmitCall(CGFunction Func,
                    ArrayRef<RValueTy> Arguments);

  RValueTy EmitCall1(CGFunction Func,
                     RValueTy Arg) {
    return EmitCall(Func, Arg);
  }

  RValueTy EmitCall2(CGFunction Func,
                     RValueTy A1, RValueTy A2) {
    RValueTy Args[] = { A1, A2 };
    return EmitCall(Func, llvm::makeArrayRef(Args, 2));
  }

  RValueTy EmitCall3(CGFunction Func,
                     RValueTy A1, RValueTy A2, RValueTy A3) {
    RValueTy Args[] = { A1, A2, A3 };
    return EmitCall(Func, llvm::makeArrayRef(Args, 3));
  }

  void EmitCallArg(llvm::Type *T, CallArgList &Args,
                   const Expr *E, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(CallArgList &Args,
                   const Expr *E, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitArrayCallArg(CallArgList &ArgBuilder,
                        const Expr *E, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(CallArgList &Args,
                   llvm::Value *Value, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(CallArgList &Args,
                   ComplexValueTy Value, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(CallArgList &Args,
                   CharacterValueTy Value, CGFunctionInfo::ArgInfo ArgInfo);
  llvm::Value *EmitCallArgPtr(const Expr *E);

  llvm::CallInst *EmitRuntimeCall(llvm::Value *Func);
  llvm::CallInst *EmitRuntimeCall(llvm::Value *Func, llvm::ArrayRef<llvm::Value*> Args);
  llvm::CallInst *EmitRuntimeCall2(llvm::Value *Func, llvm::Value *A1, llvm::Value *A2);

  RValueTy EmitStatementFunctionCall(const FunctionDecl *Function,
                                     ArrayRef<Expr*> Arguments);
  bool IsInlinedArgument(const VarDecl *VD);
  RValueTy GetInlinedArgumentValue(const VarDecl *VD);

  // arrays
  llvm::Value *EmitArrayElementPtr(const Expr *Target,
                                   const ArrayRef<Expr*> Subscripts);
  llvm::Value *EmitArrayElementPtr(const ArrayElementExpr *E) {
    return EmitArrayElementPtr(E->getTarget(), E->getSubscripts());
  }



  llvm::Value *EmitDimSize(const ArrayDimensionValueTy &Dim);
  llvm::Value *EmitDimOffset(llvm::Value *Subscript,
                             const ArrayDimensionValueTy &Dim);

  /// \brief Computes the element offset from the array base for the given
  /// subscripts.
  llvm::Value *EmitArrayOffset(ArrayRef<llvm::Value*> Subscripts,
                               const ArrayValueRef &Value);

  /// \brief Returns the pointer to the element for the given subscripts.
  llvm::Value *EmitArrayElementPtr(ArrayRef<llvm::Value*> Subscripts,
                                   const ArrayValueRef &Value);

  /// EmitSectionSize - Emits the number of elements in a single
  /// section in the given given array.
  llvm::Value *EmitSectionSize(const ArrayValueRef &Value, int I);

  /// EmitArraySize - Emits the number of elements in the given array.
  llvm::Value *EmitArraySize(const ArrayValueRef &Value);

  ArrayDimensionValueTy EmitArrayRangeSection(const ArrayDimensionValueTy &Dim,
                                              llvm::Value *&Ptr, llvm::Value *&Offset,
                                              llvm::Value *LB, llvm::Value *UB,
                                              llvm::Value *Stride = nullptr);

  void EmitArrayElementSection(const ArrayDimensionValueTy &Dim,
                               llvm::Value *&Ptr, llvm::Value *&Offset,
                               llvm::Value *Index);

  ArrayDimensionValueTy GetVectorDimensionInfo(QualType T);

  void GetArrayDimensionsInfo(QualType T, SmallVectorImpl<ArrayDimensionValueTy> &Dims);

  llvm::Value *EmitArrayArgumentPointerValueABI(const Expr *E);
  llvm::Constant *EmitConstantArrayExpr(const ArrayConstructorExpr *E);
  llvm::Value *EmitConstantArrayConstructor(const ArrayConstructorExpr *E);
  ArrayVectorValueTy EmitTempArrayConstructor(const ArrayConstructorExpr *E);
  ArrayVectorValueTy EmitArrayConstructor(const ArrayConstructorExpr *E);
  void EmitArrayAssignment(const Expr *LHS, const Expr *RHS);
};

}  // end namespace CodeGen
}  // end namespace flang

#endif
