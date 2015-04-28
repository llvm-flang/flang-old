//===-- ExprEvaluator.cpp - Unittests for expression evaluation methosds --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/ASTContext.h"
#include "flang/AST/Expr.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace flang;

bool CheckEvaluatable(ASTContext &C, const Expr *E, bool Value = true) {
  if(E->isEvaluatable(C) != Value) {
    llvm::errs() << "Expected an evaluatable expression\n";
    return true;
  }
  return false;
}

bool CheckIntValue(ASTContext &C, const Expr *E, int64_t Value) {
  int64_t X = 0;
  if(!E->EvaluateAsInt(X, C)) {
    llvm::errs() << "Failed to evaluate an integer expression \n";
    return true;
  }
  if(X != Value) {
    llvm::errs() << "Expected " << Value << " instead of " << X << "\n";
    return true;
  }
  return false;
}

bool CheckArrayElementOffset(ASTContext &C, const ArrayElementExpr *E, uint64_t Value) {
  uint64_t Offset = 0;
  if(!E->EvaluateOffset(C, Offset)) {
    llvm::errs() << "Failed to evaluate an array element offset\n";
    return true;
  }
  if(Offset != Value) {
    llvm::errs() << "Expected offset of " << Value << " instead of " << Offset << "\n";
    return true;
  }
  return false;
}

int test(ASTContext &C) {
  auto Int = IntegerConstantExpr::Create(C, 13);
  if(CheckEvaluatable(C, Int)) return 1;
  if(CheckIntValue(C, Int, 13)) return 1;
  auto Int2 = IntegerConstantExpr::Create(C, -11);
  if(CheckEvaluatable(C, Int2)) return 1;
  if(CheckIntValue(C, Int2, -11)) return 1;


  // unary operations
  auto Neg = UnaryExpr::Create(C, SourceLocation(), UnaryExpr::Minus, Int);
  if(CheckEvaluatable(C, Neg)) return 1;
  if(CheckIntValue(C, Neg, -13)) return 1;

  auto Pos = UnaryExpr::Create(C, SourceLocation(), UnaryExpr::Plus, Int);
  if(CheckEvaluatable(C, Pos)) return 1;
  if(CheckIntValue(C, Pos, 13)) return 1;


  // binary operations
  auto Sum = BinaryExpr::Create(C, SourceLocation(), BinaryExpr::Plus, Int->getType(), Int, Int2);
  if(CheckEvaluatable(C, Sum)) return 1;
  if(CheckIntValue(C, Sum, 2)) return 1;

  auto Diff = BinaryExpr::Create(C, SourceLocation(), BinaryExpr::Minus, Int->getType(), Neg, Int2);
  if(CheckEvaluatable(C, Diff)) return 1;
  if(CheckIntValue(C, Diff, -2)) return 1;

  auto Prod = BinaryExpr::Create(C, SourceLocation(), BinaryExpr::Multiply, Int->getType(), Int, Int2);
  if(CheckEvaluatable(C, Prod)) return 1;
  if(CheckIntValue(C, Prod, -143)) return 1;

  auto Div = BinaryExpr::Create(C, SourceLocation(), BinaryExpr::Divide, Int->getType(), Neg, Int2);
  if(CheckEvaluatable(C, Div)) return 1;
  if(CheckIntValue(C, Div, 1)) return 1;


  // array element offset
  Expr *Items[5] = { Int, Int, Int, Int, Int };
  auto Dim = ExplicitShapeSpec::Create(C, IntegerConstantExpr::Create(C, -2), IntegerConstantExpr::Create(C, 2));
  auto AVal = ArrayConstructorExpr::Create(C, SourceLocation(), Items, C.getArrayType(Int->getType(), Dim));
  if(CheckArrayElementOffset(C, ArrayElementExpr::Create(C, SourceLocation(), AVal, IntegerConstantExpr::Create(C, -2)), 0))
     return 1;
  if(CheckArrayElementOffset(C, ArrayElementExpr::Create(C, SourceLocation(), AVal, IntegerConstantExpr::Create(C, -1)), 1))
     return 1;
  if(CheckArrayElementOffset(C, ArrayElementExpr::Create(C, SourceLocation(), AVal, IntegerConstantExpr::Create(C, 0)), 2))
     return 1;
  if(CheckArrayElementOffset(C, ArrayElementExpr::Create(C, SourceLocation(), AVal, IntegerConstantExpr::Create(C, 1)), 3))
     return 1;
  if(CheckArrayElementOffset(C, ArrayElementExpr::Create(C, SourceLocation(), AVal, IntegerConstantExpr::Create(C, 2)), 4))
     return 1;

  return 0;
}

int main() {
  auto SM = new llvm::SourceMgr();
  auto Context = new ASTContext(*SM, LangOptions());
  auto Result = test(*Context);
  delete Context;
  delete SM;
  return Result;
}
