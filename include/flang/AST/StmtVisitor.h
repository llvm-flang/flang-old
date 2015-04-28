//===--- StmtVisitor.h - Visitor for Stmt subclasses ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the StmtVisitor and ConstStmtVisitor interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_AST_STMTVISITOR_H
#define LLVM_FLANG_AST_STMTVISITOR_H

#include "flang/Basic/MakePtr.h"
#include "flang/AST/Stmt.h"

namespace flang {

/// StmtVisitorBase - This class implements a simple visitor for Stmt
/// subclasses.
template<template <typename> class Ptr, typename ImplClass, typename RetTy=void>
class StmtVisitorBase {
public:

#define PTR(CLASS) typename Ptr<CLASS>::type
#define DISPATCH(NAME, CLASS) \
 return static_cast<ImplClass*>(this)->Visit ## NAME(static_cast<PTR(CLASS)>(S))

  RetTy Visit(PTR(Stmt) S) {

    // Top switch stmt: dispatch to VisitFooStmt for each FooStmt.
    switch (S->getStmtClass()) {
    default: llvm_unreachable("Unknown stmt kind!");
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT)                              \
    case Stmt::CLASS ## Class: DISPATCH(CLASS, CLASS);
#include "flang/AST/StmtNodes.inc"
    }
  }

  // If the implementation chooses not to implement a certain visit method, fall
  // back on VisitExpr or whatever else is the superclass.

#define STMT(CLASS, PARENT)                                   \
  RetTy Visit ## CLASS(PTR(CLASS) S) { DISPATCH(PARENT, PARENT); }
#define ABSTRACT_STMT(STMT) STMT
#include "flang/AST/StmtNodes.inc"

  // Base case, ignore it. :)
  RetTy VisitStmt(PTR(Stmt) Node) { return RetTy(); }

#undef PTR
#undef DISPATCH
};

/// StmtVisitor - This class implements a simple visitor for Stmt subclasses.
///
/// This class does not preserve constness of Stmt pointers (see also
/// ConstStmtVisitor).
template<typename ImplClass, typename RetTy=void>
class StmtVisitor
 : public StmtVisitorBase<make_ptr, ImplClass, RetTy> {};

/// ConstStmtVisitor - This class implements a simple visitor for Stmt
/// subclasses.
///
/// This class preserves constness of Stmt pointers (see also StmtVisitor).
template<typename ImplClass, typename RetTy=void>
class ConstStmtVisitor
 : public StmtVisitorBase<make_const_ptr, ImplClass, RetTy> {};

}  // end namespace flang

#endif
