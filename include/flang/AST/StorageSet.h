//===--- StorageSet.h - A set of objects in a specific storage unit ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines EquivalenceSet and CommonBlockSet.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_STORAGESET_H
#define FLANG_AST_STORAGESET_H

#include "flang/Basic/LLVM.h"

namespace flang {

class ASTContext;
class VarDecl;
class CommonBlockDecl;
class Expr;

/// StorageSet - A
class StorageSet {
public:
  enum StorageSetClass {
    NoStorageSetClass = 0,
    EquivalenceSetClass,
    CommonBlockSetClass
  };

private:
  StorageSetClass ID;
protected:
  StorageSet(StorageSetClass Class) : ID(Class) {}
public:

  StorageSetClass getStorageSetClass() const { return ID; }
  static bool classof(const StorageSet*) { return true; }
};

/// EquivalenceSet - Contains a set of objects which share the same memory block.
class EquivalenceSet : public StorageSet {
public:
  class Object {
  public:
    VarDecl *Var;
    const Expr *E;

    Object() {}
    Object(VarDecl *var, const Expr *e)
      : Var(var), E(e) {}
  };

private:
  Object *Objects;
  unsigned ObjectCount;

  EquivalenceSet(ASTContext &C, ArrayRef<Object> objects);
public:

  static EquivalenceSet *Create(ASTContext &C, ArrayRef<Object> Objects);

  ArrayRef<Object> getObjects() const {
    return ArrayRef<Object>(Objects, ObjectCount);
  }

  static bool classof(const StorageSet* S) {
    return S->getStorageSetClass() == EquivalenceSetClass;
  }
};

/// CommonBlockSet - Contains a set of objects that are shared in the program
/// using one common block.
class CommonBlockSet : public StorageSet {
public:
  class Object {
  public:
    VarDecl *Var;
    EquivalenceSet *Equiv;

    Object() {}
    Object(VarDecl *V)
      : Var(V), Equiv(nullptr) {}
  };

private:
  Object *Objects;
  unsigned ObjectCount;
  CommonBlockDecl *Decl;

  CommonBlockSet(CommonBlockDecl *CBDecl);
public:

  static CommonBlockSet *Create(flang::ASTContext &C, CommonBlockDecl *CBDecl);

  void setObjects(ASTContext &C, ArrayRef<Object> objects);

  ArrayRef<Object> getObjects() const {
    return ArrayRef<Object>(Objects, ObjectCount);
  }

  CommonBlockDecl *getDecl() const {
    return Decl;
  }

  static bool classof(const StorageSet* S) {
    return S->getStorageSetClass() == CommonBlockSetClass;
  }
};

} // end flang namespace

#endif
