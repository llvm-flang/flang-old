//===--- FormatItem.h - Fortran Format Items -------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the format item and descriptor class.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_FORMATITEM_H__
#define FLANG_AST_FORMATITEM_H__

#include "flang/Basic/SourceLocation.h"
#include "flang/AST/Expr.h"
#include "flang/Basic/LLVM.h"

namespace flang {

class ASTContext;

class FormatItem {
public:
  enum {
    fs_CharacterStringEditDesc = tok::NUM_TOKENS+1,
    fs_FormatItems
  };
private:
  unsigned Descriptor;
  SourceLocation Loc;
protected:
  FormatItem(unsigned Desc, SourceLocation L)
    : Descriptor(Desc), Loc(L) {}
  friend class ASTContext;
public:
  SourceLocation getLocation() const { return Loc; }

  unsigned getDescriptor() const { return Descriptor; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *) { return true; }
};

class FormatEditDesc : public FormatItem {
private:
  IntegerConstantExpr *RepeatCount;
protected:
  FormatEditDesc(tok::TokenKind Descriptor, SourceLocation Loc,
                 IntegerConstantExpr *Repeat)
    : FormatItem(Descriptor, Loc), RepeatCount(Repeat) {}
public:

  IntegerConstantExpr *getRepeatCount() const { return RepeatCount; }

  static bool classof(const FormatItem *S) {
   return S->getDescriptor() != fs_CharacterStringEditDesc &&
          S->getDescriptor() != fs_FormatItems;
  }
};

class DataEditDesc : public FormatEditDesc {
  IntegerConstantExpr *W;

protected:
  DataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                 IntegerConstantExpr *RepeatCount, IntegerConstantExpr *w)
   : FormatEditDesc(Descriptor, Loc, RepeatCount), W(w) {
  }
public:
  IntegerConstantExpr *getW() const { return W; }

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_I: case tok::fs_B: case tok::fs_O: case tok::fs_Z:
    case tok::fs_F: case tok::fs_E: case tok::fs_EN: case tok::fs_ES:
    case tok::fs_G: case tok::fs_L: case tok::fs_A: case tok::fs_D:
      return true;
    default: break;
    }
    return false;
  }
};

// I, B, O, Z
class IntegerDataEditDesc : public DataEditDesc {
  IntegerConstantExpr *M;

  IntegerDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                        IntegerConstantExpr *RepeatCount,
                        IntegerConstantExpr *w,
                        IntegerConstantExpr *m);

public:

  static IntegerDataEditDesc *Create(ASTContext &C, SourceLocation Loc,
                                       tok::TokenKind Descriptor,
                                       IntegerConstantExpr *RepeatCount,
                                       IntegerConstantExpr *W,
                                       IntegerConstantExpr *M);

  IntegerConstantExpr *getM() const { return M; }

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_I: case tok::fs_B: case tok::fs_O: case tok::fs_Z:
      return true;
    default: break;
    }
    return false;
  }
};

// F, E, EN, ES, G
class RealDataEditDesc : public DataEditDesc {
  IntegerConstantExpr *D;
  IntegerConstantExpr *E;

  RealDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                   IntegerConstantExpr *RepeatCount,
                   IntegerConstantExpr *w,
                   IntegerConstantExpr *d,
                   IntegerConstantExpr *e);

public:
  static RealDataEditDesc *Create(ASTContext &C, SourceLocation Loc,
                                  tok::TokenKind Descriptor,
                                  IntegerConstantExpr *RepeatCount,
                                  IntegerConstantExpr *W,
                                  IntegerConstantExpr *D,
                                  IntegerConstantExpr *E);

  IntegerConstantExpr *getD() const { return D; }
  IntegerConstantExpr *getE() const { return E; }

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_F: case tok::fs_E:
    case tok::fs_EN: case tok::fs_ES: case tok::fs_G:
      return true;
    default: break;
    }
    return false;
  }
};

// L
class LogicalDataEditDesc : public DataEditDesc {
  LogicalDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                      IntegerConstantExpr *RepeatCount,
                      IntegerConstantExpr *w);

public:
  static LogicalDataEditDesc *Create(ASTContext &C, SourceLocation Loc,
                                     tok::TokenKind Descriptor,
                                     IntegerConstantExpr *RepeatCount,
                                     IntegerConstantExpr *W);

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_L:
      return true;
    default: break;
    }
    return false;
  }
};

// A
class CharacterDataEditDesc : public DataEditDesc {
  CharacterDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                          IntegerConstantExpr *RepeatCount,
                          IntegerConstantExpr *w);

public:
  static CharacterDataEditDesc *Create(ASTContext &C, SourceLocation Loc,
                                       tok::TokenKind Descriptor,
                                       IntegerConstantExpr *RepeatCount,
                                       IntegerConstantExpr *W);

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_A:
      return true;
    default: break;
    }
    return false;
  }
};

class ControlEditDesc : public FormatEditDesc {
protected:
  ControlEditDesc(tok::TokenKind Descriptor, SourceLocation Loc,
                  IntegerConstantExpr *RepeatCount)
    : FormatEditDesc(Descriptor, Loc, RepeatCount) {
  }
public:

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_T: case tok::fs_TL:
    case tok::fs_TR: case tok::fs_X:
      return true;
    default: break;
    }
    return false;
  }
};

// T, TL, TR, X
// NB: store N in RepeatCount
class PositionEditDesc : public ControlEditDesc {
  PositionEditDesc(tok::TokenKind Descriptor, SourceLocation Loc,
                  IntegerConstantExpr *N);
public:
  static PositionEditDesc *Create(ASTContext &C, SourceLocation Loc,
                                  tok::TokenKind Descriptor,
                                  IntegerConstantExpr *N);

  IntegerConstantExpr *getN() const { return getRepeatCount(); }

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *D) {
    switch(D->getDescriptor()) {
    case tok::fs_T: case tok::fs_TL:
    case tok::fs_TR: case tok::fs_X:
      return true;
    default: break;
    }
    return false;
  }
};

class CharacterStringEditDesc : public FormatItem {
  CharacterConstantExpr *Str;

  CharacterStringEditDesc(CharacterConstantExpr *S);
public:
  static CharacterStringEditDesc *Create(ASTContext &C,
                                         CharacterConstantExpr *Str);

  const char *getValue() const { return Str->getValue(); }

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *S) {
    return S->getDescriptor() == fs_CharacterStringEditDesc;
  }
};

class FormatItemList : public FormatItem {
  IntegerConstantExpr *RepeatCount;
  FormatItem **ItemList;
  unsigned N;

  FormatItemList(ASTContext &C, SourceLocation Loc,
                 IntegerConstantExpr *Repeat,
                 ArrayRef<FormatItem*> Items);
public:
  static FormatItemList *Create(ASTContext &C,
                                SourceLocation Loc,
                                IntegerConstantExpr *RepeatCount,
                                ArrayRef<FormatItem*> Items);

  ArrayRef<FormatItem*> getItems() const {
    return ArrayRef<FormatItem*>(ItemList, N);
  }

  IntegerConstantExpr *getRepeatCount() const {
    return RepeatCount;
  }

  void print(llvm::raw_ostream&);

  static bool classof(const FormatItem *S) {
    return S->getDescriptor() == fs_FormatItems;
  }
};

} // end namespace flang

#endif
