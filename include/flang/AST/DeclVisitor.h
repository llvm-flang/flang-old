//===--- DeclVisitor.h - Visitor for Decl subclasses ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the DeclVisitor interface.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_FLANG_AST_DECLVISITOR_H
#define LLVM_FLANG_AST_DECLVISITOR_H

#include "flang/Basic/MakePtr.h"
#include "flang/AST/Decl.h"

namespace flang {

/// \brief A simple visitor class that helps create declaration visitors.
template<template <typename> class Ptr, typename ImplClass, typename RetTy=void>
class DeclVisitorBase {
public:

#define PTR(CLASS) typename Ptr<CLASS>::type
#define DISPATCH(NAME, CLASS) \
  return static_cast<ImplClass*>(this)->Visit##NAME(static_cast<PTR(CLASS)>(D))

  RetTy Visit(PTR(Decl) D) {
    switch (D->getKind()) {
#define DECL(DERIVED, BASE) \
      case Decl::DERIVED: DISPATCH(DERIVED##Decl, DERIVED##Decl);
#define ABSTRACT_DECL(DECL)
#include "flang/AST/DeclNodes.inc"
    }
    llvm_unreachable("Decl that isn't part of DeclNodes.inc!");
  }

  // If the implementation chooses not to implement a certain visit
  // method, fall back to the parent.
#define DECL(DERIVED, BASE) \
  RetTy Visit##DERIVED##Decl(PTR(DERIVED##Decl) D) { DISPATCH(BASE, BASE); }
#include "flang/AST/DeclNodes.inc"

  RetTy VisitDecl(PTR(Decl) D) { return RetTy(); }

  RetTy Visit(PTR(DeclContext) D) {
    DISPATCH(DeclContext, DeclContext);
  }

  RetTy VisitDeclContext(PTR(DeclContext) Ctx) {
    auto I = Ctx->decls_begin();
    for(auto E = Ctx->decls_end(); I!=E; ++I) {
      if((*I)->getDeclContext() == Ctx)
        Visit(*I);
    }
    return RetTy();
  }

#undef PTR
#undef DISPATCH
};

/// \brief A simple visitor class that helps create declaration visitors.
///
/// This class does not preserve constness of Decl pointers (see also
/// ConstDeclVisitor).
template<typename ImplClass, typename RetTy=void>
class DeclVisitor
 : public DeclVisitorBase<make_ptr, ImplClass, RetTy> {};

/// \brief A simple visitor class that helps create declaration visitors.
///
/// This class preserves constness of Decl pointers (see also DeclVisitor).
template<typename ImplClass, typename RetTy=void>
class ConstDeclVisitor
 : public DeclVisitorBase<make_const_ptr, ImplClass, RetTy> {};

}  // end namespace flang

#endif
