//===--- TypeVisitor.h - Visitor for Type subclasses ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the TypeVisitor interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_AST_TYPEVISITOR_H
#define LLVM_FLANG_AST_TYPEVISITOR_H

#include "flang/AST/Type.h"

namespace flang {

/// \brief An operation on a type.
///
/// \tparam ImplClass Class implementing the operation. Must be inherited from
///         TypeVisitor.
/// \tparam RetTy %Type of result produced by the operation.
///
/// The class implements polymorphic operation on an object of type derived
/// from Type. The operation is performed by calling method Visit. It then
/// dispatches the call to function \c VisitFooType, if actual argument type
/// is \c FooType.
template<typename ImplClass, typename RetTy=void>
class TypeVisitor {
public:

#define DISPATCH(NAME, CLASS) \
  return static_cast<ImplClass*>(this)->Visit ## NAME(static_cast<const CLASS*>(Split.first), Split.second)

  /// \brief Performs the operation associated with this visitor object.
  RetTy Visit(QualType T) {
    auto Split = T.split();
    // Top switch stmt: dispatch to VisitFooType for each FooType.
    switch (T->getTypeClass()) {
#define TYPE(Class, Parent) case Type::Class: DISPATCH(Class##Type, Class##Type);
#include "flang/AST/TypeNodes.def"
    }
    llvm_unreachable("Unknown type class!");
  }

  // If the implementation chooses not to implement a certain visit method, fall
  // back on superclass.
#define TYPE(Class, Parent) RetTy Visit##Class##Type(const Class##Type *T, Qualifiers QS) { \
  return static_cast<ImplClass*>(this)->Visit ## Parent(static_cast<const Parent*>(T), QS);                                                       \
}
#include "flang/AST/TypeNodes.def"

  /// \brief Method called if \c ImpClass doesn't provide specific handler
  /// for some type class.
  RetTy VisitType(const Type*, Qualifiers) { return RetTy(); }
};

#undef DISPATCH

}  // end namespace flang

#endif
