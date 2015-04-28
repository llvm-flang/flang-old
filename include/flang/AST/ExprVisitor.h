//===--- ExprVisitor.h - Visitor for Expr subclasses ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ExprVisitor and ConstExprVisitor interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_AST_EXPRVISITOR_H
#define LLVM_FLANG_AST_EXPRVISITOR_H

#include "flang/Basic/MakePtr.h"
#include "flang/AST/Expr.h"

namespace flang {

/// StmtVisitorBase - This class implements a simple visitor for Expr
/// subclasses.
template<template <typename> class Ptr, typename ImplClass, typename RetTy=void>
class ExprVisitorBase {
public:

#define PTR(CLASS) typename Ptr<CLASS>::type
#define DISPATCH(NAME, CLASS) \
 return static_cast<ImplClass*>(this)->Visit ## NAME(static_cast<PTR(CLASS)>(E))

  RetTy Visit(PTR(Expr) E) {

    // If we have a binary expr, dispatch to the subcode of the binop.  A smart
    // optimizer (e.g. LLVM) will fold this comparison into the switch stmt
    // below.
    if (PTR(BinaryExpr) BinOp = dyn_cast<BinaryExpr>(E)) {
      switch (BinOp->getOperator()) {
      case BinaryExpr::Eqv: DISPATCH(BinaryExprEqv, BinaryExpr);
      case BinaryExpr::Neqv: DISPATCH(BinaryExprNeqv, BinaryExpr);
      case BinaryExpr::Or: DISPATCH(BinaryExprOr, BinaryExpr);
      case BinaryExpr::And: DISPATCH(BinaryExprAnd, BinaryExpr);
      case BinaryExpr::Equal: DISPATCH(BinaryExprEQ, BinaryExpr);
      case BinaryExpr::NotEqual: DISPATCH(BinaryExprNE, BinaryExpr);
      case BinaryExpr::LessThan: DISPATCH(BinaryExprLT, BinaryExpr);
      case BinaryExpr::LessThanEqual: DISPATCH(BinaryExprLE, BinaryExpr);
      case BinaryExpr::GreaterThan: DISPATCH(BinaryExprGT, BinaryExpr);
      case BinaryExpr::GreaterThanEqual: DISPATCH(BinaryExprGE, BinaryExpr);
      case BinaryExpr::Concat: DISPATCH(BinaryExprConcat, BinaryExpr);
      case BinaryExpr::Plus: DISPATCH(BinaryExprAdd, BinaryExpr);
      case BinaryExpr::Minus: DISPATCH(BinaryExprSub, BinaryExpr);
      case BinaryExpr::Multiply: DISPATCH(BinaryExprMul, BinaryExpr);
      case BinaryExpr::Divide: DISPATCH(BinaryExprDiv, BinaryExpr);
      case BinaryExpr::Power: DISPATCH(BinaryExprPow, BinaryExpr);
      }
    } else if (PTR(UnaryExpr) UnOp = dyn_cast<UnaryExpr>(E)) {
      switch (UnOp->getOperator()) {
      case UnaryExpr::Not: DISPATCH(UnaryExprNot, UnaryExpr);
      case UnaryExpr::Plus: DISPATCH(UnaryExprPlus, UnaryExpr);
      case UnaryExpr::Minus: DISPATCH(UnaryExprMinus, UnaryExpr);
      }
    }

    // Top switch expr: dispatch to VisitFooExpr for each FooExpr.
    switch (E->getExprClass()) {
    default: llvm_unreachable("Unknown expr kind!");
#define ABSTRACT_EXPR(EXPR)
#define EXPR(CLASS, PARENT)                              \
    case Expr::CLASS ## Class: DISPATCH(CLASS, CLASS);
#include "flang/AST/ExprNodes.inc"
    }
  }

  // If the implementation chooses not to implement a certain visit method, fall
  // back on VisitExpr or whatever else is the superclass.
#define ABSTRACT_EXPR(E) E
#define EXPR(CLASS, PARENT)                                   \
  RetTy Visit ## CLASS(PTR(CLASS) E) { DISPATCH(PARENT, PARENT); }
#include "flang/AST/ExprNodes.inc"

  // If the implementation doesn't implement binary operator methods, fall back
  // on VisitBinaryOperator.
#define BINOP_FALLBACK(NAME) \
  RetTy VisitBinaryExpr ## NAME(PTR(BinaryExpr) E) { \
    DISPATCH(BinaryExpr, BinaryExpr); \
  }
  BINOP_FALLBACK(Eqv) BINOP_FALLBACK(Neqv)
  BINOP_FALLBACK(Or) BINOP_FALLBACK(And)
  BINOP_FALLBACK(EQ) BINOP_FALLBACK(NE)
  BINOP_FALLBACK(LT) BINOP_FALLBACK(LE)
  BINOP_FALLBACK(GT) BINOP_FALLBACK(GE)
  BINOP_FALLBACK(Concat)
  BINOP_FALLBACK(Add) BINOP_FALLBACK(Sub)
  BINOP_FALLBACK(Mul) BINOP_FALLBACK(Div) BINOP_FALLBACK(Pow)

#undef BINOP_FALLBACK

#define UNARYOP_FALLBACK(NAME) \
  RetTy VisitUnaryExpr ## NAME(PTR(UnaryExpr) E) { \
    DISPATCH(UnaryExpr, UnaryExpr); \
  }
  UNARYOP_FALLBACK(Not)
  UNARYOP_FALLBACK(Plus) UNARYOP_FALLBACK(Minus)

#undef UNARYOP_FALLBACK

  // Base case, ignore it. :)
  RetTy VisitExpr(PTR(Expr) Node) { return RetTy(); }

#undef PTR
#undef DISPATCH
};

/// ExprVisitor - This class implements a simple visitor for Expr subclasses.
///
/// This class does not preserve constness of Stmt pointers (see also
/// ConstStmtVisitor).
template<typename ImplClass, typename RetTy=void>
class ExprVisitor
 : public ExprVisitorBase<make_ptr, ImplClass, RetTy> {};

/// ConstExprVisitor - This class implements a simple visitor for Expr
/// subclasses.
///
/// This class preserves constness of Stmt pointers (see also StmtVisitor).
template<typename ImplClass, typename RetTy=void>
class ConstExprVisitor
 : public ExprVisitorBase<make_const_ptr, ImplClass, RetTy> {};

}  // end namespace flang

#endif
