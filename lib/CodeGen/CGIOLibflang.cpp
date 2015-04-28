//===----- CGIOLibflang.cpp - Interface to Libflang IO Runtime -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides a class for IO statements code generation for the Libflang
// runtime library.
//
//===----------------------------------------------------------------------===//

#include "CGIORuntime.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"

namespace flang {
namespace CodeGen {

class CGLibflangIORuntime : public CGIORuntime {

  llvm::StructType *WriteControllerType;
  uint64_t WriteControllerTypeSize;
public:
  CGLibflangIORuntime(CodeGenModule &CGM)
    : CGIORuntime(CGM) {
    WriteControllerType = nullptr;
  }

  void EmitWriteStmt(CodeGenFunction &CGF, const WriteStmt *S);
  void EmitPrintStmt(CodeGenFunction &CGF, const PrintStmt *S);

  llvm::StructType *GetWriteControllerType();
};


llvm::StructType *CGLibflangIORuntime::GetWriteControllerType() {
  if(WriteControllerType)
    return WriteControllerType;
  llvm::Type *Types[] = {
    CGM.Int8PtrTy, //FormatPtr
    CGM.SizeTy,    //FormatLength
    CGM.Int32Ty,   //unit
    CGM.Int32Ty    //flags
  };
  WriteControllerType = llvm::StructType::get(CGM.getLLVMContext(),
                                              llvm::makeArrayRef(Types,4));
  WriteControllerTypeSize = CGM.getDataLayout().getStructLayout(WriteControllerType)->getSizeInBytes();
  return WriteControllerType;
}

class CGLibflangWriteEmitter {
  CodeGenModule &CGM;
  CodeGenFunction &CGF;
  llvm::Value *ControllerPtr;
  LibflangTransferABI ABI;
public:
  CGLibflangWriteEmitter(CodeGenModule &cgm,
                         CodeGenFunction &cgf,
                         llvm::Value *Controller)
    : CGM(cgm), CGF(cgf) {
    ControllerPtr = Controller;
  }

  FortranABI *getTransferABI() {
    return &ABI;
  }

  void EmitStart();
  void EmitEnd();
  void EmitWriteUnformattedList(ArrayRef<Expr*> Values);
  void EmitWriteUnformattedBuiltin(const BuiltinType *BTy,
                                   const Expr *E);
  void EmitWriteUnformattedChar(const Expr *E);
};

void CGLibflangWriteEmitter::EmitStart() {
  auto Func = CGM.GetRuntimeFunction1("write_start", ControllerPtr->getType());
  CGF.EmitCall1(Func, ControllerPtr);
}

void CGLibflangWriteEmitter::EmitEnd() {
  auto Func = CGM.GetRuntimeFunction1("write_end", ControllerPtr->getType());
  CGF.EmitCall1(Func, ControllerPtr);
}

void CGLibflangWriteEmitter::EmitWriteUnformattedList(ArrayRef<Expr*> Values) {
  for(auto E : Values) {
    auto EType = E->getType();
    if(EType->isCharacterType())
      EmitWriteUnformattedChar(E);
    if(auto BTy = dyn_cast<BuiltinType>(EType.getTypePtr()))
      EmitWriteUnformattedBuiltin(BTy, E);
  }
}

void CGLibflangWriteEmitter::EmitWriteUnformattedBuiltin(const BuiltinType *BTy,
                                                         const Expr *E) {
  CGFunction Func;
  switch(BTy->getTypeSpec()) {
  case BuiltinType::Integer:
    Func = CGM.GetRuntimeFunction2("write_integer", ControllerPtr->getType(),
                                   E->getType(), CGType(), getTransferABI());
    break;
  case BuiltinType::Real:
    Func = CGM.GetRuntimeFunction2("write_real", ControllerPtr->getType(),
                                   E->getType(), CGType(), getTransferABI());
    break;
  case BuiltinType::Complex:
    Func = CGM.GetRuntimeFunction2("write_complex", ControllerPtr->getType(),
                                   E->getType(), CGType(), getTransferABI());
    break;
  case BuiltinType::Logical:
    Func = CGM.GetRuntimeFunction2("write_logical", ControllerPtr->getType(),
                                   E->getType(), CGType(), getTransferABI());
    break;
  }
  CallArgList ArgList;
  CGF.EmitCallArg(ArgList, ControllerPtr, Func.getInfo()->getArguments()[0]);
  CGF.EmitCallArg(ArgList, E, Func.getInfo()->getArguments()[1]);
  CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList, ArrayRef<Expr*>(), true);
}

void CGLibflangWriteEmitter::EmitWriteUnformattedChar(const Expr *E) {
  auto Func = CGM.GetRuntimeFunction2("write_character", ControllerPtr->getType(),
                                      E->getType());
  CGF.EmitCall2(Func, ControllerPtr, CGF.EmitCharacterExpr(E));
}

void CGLibflangIORuntime::EmitWriteStmt(CodeGenFunction &CGF, const WriteStmt *S) {
  // FIXME
  auto ControllerPtr = CGF.CreateTempAlloca(GetWriteControllerType(), "write");
  CGF.getBuilder().CreateMemSet(ControllerPtr, CGF.getBuilder().getInt8(0),
                                WriteControllerTypeSize, 0);
  CGLibflangWriteEmitter Writer(CGM, CGF, ControllerPtr);

  Writer.EmitStart();
  Writer.EmitWriteUnformattedList(S->getOutputList());
  Writer.EmitEnd();
}

void CGLibflangIORuntime::EmitPrintStmt(CodeGenFunction &CGF, const PrintStmt *S) {
  // FIXME
  auto ControllerPtr = CGF.CreateTempAlloca(GetWriteControllerType(), "print");
  CGF.getBuilder().CreateMemSet(ControllerPtr, CGF.getBuilder().getInt8(0),
                                WriteControllerTypeSize, 0);
  CGLibflangWriteEmitter Writer(CGM, CGF, ControllerPtr);

  Writer.EmitStart();
  Writer.EmitWriteUnformattedList(S->getOutputList());
  Writer.EmitEnd();
}

CGIORuntime *CreateLibflangIORuntime(CodeGenModule &CGM) {
  return new CGLibflangIORuntime(CGM);
}

}
} // end namespace flang
