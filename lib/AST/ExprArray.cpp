//===--- ExprArray.cpp - Array expression classification ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/ExprVisitor.h"

namespace flang {

class ContiguousArrayChecker
  : public ConstExprVisitor<ContiguousArrayChecker, bool> {
public:

  bool VisitArraySectionExpr(const ArraySectionExpr *E) {
    if(!E->getTarget())
      return false;
    auto Subs = E->getSubscripts();

    size_t LastElementSection = 0;
    bool HasElements = false;
    bool LastSliced = false;

    for(size_t I = 0; I < Subs.size(); ++I) {
      auto Sub = Subs[I];
      if(isa<StridedRangeExpr>(Sub))
        return false;
      if(auto Range = dyn_cast<RangeExpr>(Sub)) {
        if(HasElements)
           return false;
        if(LastSliced)
          return false;
        if(!Range->hasFirstExpr() &&
           !Range->hasSecondExpr()) {
          // Ok.
        } else
          LastSliced = true;
      } else {
        if(HasElements) {
          if(I != LastElementSection)
            return false;
        }
        LastElementSection = I+1;
        HasElements = true;
        LastSliced = false;
      }
    }
    return true;
  }

  bool VisitBinaryExpr(const BinaryExpr *E) {
    return Visit(E->getLHS()) &&
           Visit(E->getRHS());
  }

  bool VisitUnaryExpr(const UnaryExpr *E) {
    return Visit(E->getExpression());
  }

  bool VisitImplicitCastExpr(const ImplicitCastExpr *E) {
    return Visit(E->getExpression());
  }

  bool VisitVarExpr(const VarExpr *E) {
    return true;
  }
};

bool Expr::IsArrayExprContiguous() const {
  assert(getType()->isArrayType());
  ContiguousArrayChecker EV;
  return EV.Visit(this);
}

} // end namespace flang
