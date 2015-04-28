//===--- FormatDesc.cpp - Fortran Format Items and Descriptors ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/FormatItem.h"
#include "flang/AST/ASTContext.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

IntegerDataEditDesc::IntegerDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                                         IntegerConstantExpr *RepeatCount,
                                         IntegerConstantExpr *w,
                                         IntegerConstantExpr *m)
  : DataEditDesc(Loc, Descriptor, RepeatCount, w), M(m) {
}

IntegerDataEditDesc *IntegerDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                                 tok::TokenKind Descriptor,
                                                 IntegerConstantExpr *RepeatCount,
                                                 IntegerConstantExpr *W,
                                                 IntegerConstantExpr *M) {
  return new(C) IntegerDataEditDesc(Loc, Descriptor, RepeatCount, W, M);
}

RealDataEditDesc::RealDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                                   IntegerConstantExpr *RepeatCount,
                                   IntegerConstantExpr *w,
                                   IntegerConstantExpr *d,
                                   IntegerConstantExpr *e)
  : DataEditDesc(Loc, Descriptor, RepeatCount, w), D(d), E(e) {
}

RealDataEditDesc *RealDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                           tok::TokenKind Descriptor,
                                           IntegerConstantExpr *RepeatCount,
                                           IntegerConstantExpr *W,
                                           IntegerConstantExpr *D,
                                           IntegerConstantExpr *E) {

  return new(C) RealDataEditDesc(Loc, Descriptor, RepeatCount, W, D, E);
}

LogicalDataEditDesc::LogicalDataEditDesc(SourceLocation Loc,
                                         tok::TokenKind Descriptor,
                                         IntegerConstantExpr *RepeatCount,
                                         IntegerConstantExpr *W)
  : DataEditDesc(Loc, Descriptor, RepeatCount, W) {}

LogicalDataEditDesc *
LogicalDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                            tok::TokenKind Descriptor,
                            IntegerConstantExpr *RepeatCount,
                            IntegerConstantExpr *W) {
  return new(C) LogicalDataEditDesc(Loc, Descriptor, RepeatCount, W);
}

CharacterDataEditDesc::CharacterDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                                             IntegerConstantExpr *RepeatCount,
                                             IntegerConstantExpr *w)
  : DataEditDesc(Loc, Descriptor, RepeatCount, w) {
}

CharacterDataEditDesc *CharacterDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                                     tok::TokenKind Descriptor,
                                                     IntegerConstantExpr *RepeatCount,
                                                     IntegerConstantExpr *W) {
  return new(C) CharacterDataEditDesc(Loc, Descriptor, RepeatCount, W);
}

PositionEditDesc::PositionEditDesc(tok::TokenKind Descriptor, SourceLocation Loc,
                                   IntegerConstantExpr *N)
  : ControlEditDesc(Descriptor, Loc, N) {
}

PositionEditDesc *PositionEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                           tok::TokenKind Descriptor,
                                           IntegerConstantExpr *N) {
  return new(C) PositionEditDesc(Descriptor, Loc, N);
}

CharacterStringEditDesc::CharacterStringEditDesc(CharacterConstantExpr *S)
  : FormatItem(fs_CharacterStringEditDesc, S->getLocation()), Str(S) {
}

CharacterStringEditDesc *CharacterStringEditDesc::Create(ASTContext &C,
                                                         CharacterConstantExpr *Str) {
  return new(C) CharacterStringEditDesc(Str);
}

FormatItemList::FormatItemList(ASTContext &C, SourceLocation Loc,
                               IntegerConstantExpr *Repeat,
                               ArrayRef<FormatItem*> Items)
  : FormatItem(fs_FormatItems, Loc), RepeatCount(Repeat) {
  N = Items.size();
  if(!N) {
    ItemList = nullptr;
    return;
  }
  ItemList = new (C) FormatItem *[N];
  for (unsigned I = 0; I != N; ++I)
    ItemList[I] = Items[I];
}

FormatItemList *FormatItemList::Create(ASTContext &C,
                                       SourceLocation Loc,
                                       IntegerConstantExpr *RepeatCount,
                                       ArrayRef<FormatItem*> Items) {
  return new(C) FormatItemList(C, Loc, RepeatCount, Items);
}

void FormatItem::print(llvm::raw_ostream&) {
}


void IntegerDataEditDesc::print(llvm::raw_ostream &O) {
  if(getRepeatCount()) getRepeatCount()->dump(O);
  O << tok::getTokenName(tok::TokenKind(getDescriptor()));
  getW()->dump(O);
}
void RealDataEditDesc::print(llvm::raw_ostream &O) {
  if(getRepeatCount()) getRepeatCount()->dump(O);
  O << tok::getTokenName(tok::TokenKind(getDescriptor()));
  if(getW()) getW()->dump(O);
}
void LogicalDataEditDesc::print(llvm::raw_ostream &O) {
  if(getRepeatCount()) getRepeatCount()->dump(O);
  O << tok::getTokenName(tok::TokenKind(getDescriptor()));
  if(getW()) getW()->dump(O);
}
void CharacterDataEditDesc::print(llvm::raw_ostream &O) {
  if(getRepeatCount()) getRepeatCount()->dump(O);
  O << tok::getTokenName(tok::TokenKind(getDescriptor()));
  if(getW()) getW()->dump(O);
}
void PositionEditDesc::print(llvm::raw_ostream &O) {
  O << tok::getTokenName(tok::TokenKind(getDescriptor()));
  getN()->dump(O);
}
void CharacterStringEditDesc::print(llvm::raw_ostream &O) {
  Str->dump(O);
}
void FormatItemList::print(llvm::raw_ostream &O) {
  O << "(";
  auto Items = getItems();
  for(size_t I = 0; I< Items.size(); ++I) {
    if(I) O << ", ";
    Items[I]->print(O);
  }
  O << " )";
}


} // end namespace flang
