//===--- SemaDataStmt.cpp - AST Builder and Checker for the DATA statement ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the checking and AST construction for the DATA
// statement.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/AST/ExprConstant.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/ADT/SmallString.h"

namespace flang {

/// Iterates over values in a DATA statement, taking repetitions into account.
class DataValueIterator {
  ArrayRef<Expr*> Values;
  Expr *Value;
  size_t   ValueOffset;
  uint64_t CurRepeatOffset;
  uint64_t CurRepeatCount;

  void InitItem();
public:
  DataValueIterator(ArrayRef<Expr*> Vals)
    : Values(Vals), ValueOffset(0) {
    InitItem();
  }

  QualType getType() const {
    return Value->getType();
  }

  Expr *getValue() const {
    return Value;
  }

  Expr *getActualValue() const {
    return Values[ValueOffset];
  }

  Expr *getActualLastValue() const {
    return Values.back();
  }

  bool isEmpty() const {
    return ValueOffset >= Values.size();
  }

  void advance();
};

void DataValueIterator::InitItem() {
  Value = Values[ValueOffset];
  if(auto RepeatedValue = dyn_cast<RepeatedConstantExpr>(Value)) {
    CurRepeatCount = RepeatedValue->getRepeatCount().getLimitedValue();
    Value = RepeatedValue->getExpression();
  } else CurRepeatCount = 1;
  CurRepeatOffset = 0;
}

void DataValueIterator::advance() {
  CurRepeatOffset++;
  if(CurRepeatOffset >= CurRepeatCount) {
    ValueOffset++;
    if(ValueOffset < Values.size())
      InitItem();
  }
}

/// Iterates over the items in a DATA statent, verifies the
/// initialization action and creates or modifies initilization
/// expressions.
class DataStmtEngine : public ExprVisitor<DataStmtEngine> {
  DataValueIterator &Values;
  flang::Sema &Sem;
  ASTContext &Context;
  DiagnosticsEngine &Diags;
  SourceLocation DataStmtLoc;

  ExprEvalScope ImpliedDoEvaluator;

  bool Done;

  ExprResult getAndCheckValue(QualType LHSType, const Expr *LHS);
  ExprResult getAndCheckAnyValue(QualType LHSType, const Expr *LHS);
  void getValueOnError();
public:
  DataStmtEngine(DataValueIterator &Vals, flang::Sema &S,
                 DiagnosticsEngine &Diag, SourceLocation Loc)
    : Values(Vals), Sem(S), Context(S.getContext()),
      Diags(Diag), DataStmtLoc(Loc), Done(false),
      ImpliedDoEvaluator(S.getContext()) {
  }

  bool HasValues(const Expr *Where);

  bool IsDone() const {
    return Done;
  }

  void VisitExpr(Expr *E);
  void VisitVarExpr(VarExpr *E);
  void CreateArrayElementExprInitializer(ArrayElementExpr *E, Expr *Parent = nullptr);
  void VisitArrayElementExpr(ArrayElementExpr *E);
  ExprResult CreateSubstringExprInitializer(SubstringExpr *E, QualType CharTy);
  void VisitSubstringExpr(SubstringExpr *E);
  ExprResult CreateMemberExprInitializer(const MemberExpr *E,
                                         const TypeConstructorExpr *Init = nullptr);
  void VisitMemberExpr(MemberExpr *E);
  void VisitImpliedDoExpr(ImpliedDoExpr *E);

  bool CheckVar(VarExpr *E);
};

bool DataStmtEngine::HasValues(const Expr *Where) {
  if(Values.isEmpty()) {
    // more items than values.
    Diags.Report(DataStmtLoc, diag::err_data_stmt_not_enough_values)
      << Where->getSourceRange();
    Done = true;
    return false;
  }
  return true;
}

void DataStmtEngine::VisitExpr(Expr *E) {
  Diags.Report(E->getLocation(), diag::err_data_stmt_invalid_item)
    << E->getSourceRange();
  getValueOnError();
}

bool DataStmtEngine::CheckVar(VarExpr *E) {
  auto VD = E->getVarDecl();
  if(VD->isArgument() || VD->isParameter() ||
     VD->isFunctionResult()) {
    Diags.Report(E->getLocation(), diag::err_data_stmt_invalid_var)
      << (VD->isParameter()? 0 : VD->isArgument()? 1 : 2)
      << VD->getIdentifier() << E->getSourceRange();
    getValueOnError();
    return true;
  }
  if(VD->isUnusedSymbol())
    const_cast<VarDecl*>(VD)->MarkUsedAsVariable(E->getLocation());
  return false;
}

ExprResult DataStmtEngine::getAndCheckValue(QualType LHSType,
                                            const Expr *LHS) {
  if(!HasValues(LHS)) return ExprResult(true);
  auto Value = Values.getValue();
  Values.advance();
  return Sem.CheckAndApplyAssignmentConstraints(Value->getLocation(),
                                                LHSType, Value,
                                                Sema::AssignmentAction::Initializing,
                                                LHS);
}

ExprResult DataStmtEngine::getAndCheckAnyValue(QualType LHSType, const Expr *LHS) {
  auto Val = getAndCheckValue(LHSType, LHS);
  auto ET = LHSType.getSelfOrArrayElementType();
  if(ET->isCharacterType() && Val.isUsable()) {
    assert(isa<CharacterConstantExpr>(Val.get()));
    return cast<CharacterConstantExpr>(Val.get())->CreateCopyWithCompatibleLength(Context,
                                                                                  ET);
  }
  return Val;
}

void DataStmtEngine::getValueOnError() {
  if(!Values.isEmpty())
    Values.advance();
}

void DataStmtEngine::VisitVarExpr(VarExpr *E) {
  if(CheckVar(E))
    return;
  auto VD = E->getVarDecl();
  auto Type = VD->getType();
  if(auto ATy = Type->asArrayType()) {
    uint64_t ArraySize;
    if(!ATy->EvaluateSize(ArraySize, Context)) {
      VisitExpr(E);
      return;
    }

    // Construct an array constructor expression for initializer
    SmallVector<Expr*, 32> Items(ArraySize);
    bool IsUsable = true;
    SourceLocation Loc;
    auto ElementType = ATy->getElementType();
    for(uint64_t I = 0; I < ArraySize; ++I) {
      if(!HasValues(E)) return;
      auto Val = getAndCheckAnyValue(ElementType, E);
      if(Val.isUsable()) {
        Items[I] = Val.get();
        if(!Loc.isValid())
          Loc = Val.get()->getLocation();
      }
      else IsUsable = false;
    }

    if(IsUsable) {
      VD->setInit(ArrayConstructorExpr::Create(Context, Loc,
                                               Items, Type));
    }
    return;
  }

  // single item
  auto Val = getAndCheckAnyValue(Type, E);
  if(Val.isUsable())
    VD->setInit(Val.get());
}

void DataStmtEngine::CreateArrayElementExprInitializer(ArrayElementExpr *E,
                                                       Expr *Parent) {
  auto Target = dyn_cast<VarExpr>(E->getTarget());
  if(!Target)
    return VisitExpr(E);
  if(CheckVar(Target))
    return;

  auto VD = Target->getVarDecl();
  auto ATy = VD->getType()->asArrayType();
  auto ElementType = ATy->getElementType();

  uint64_t ArraySize;
  if(!ATy->EvaluateSize(ArraySize, Context))
    return VisitExpr(E);

  SmallVector<Expr*, 32> Items(ArraySize);
  if(VD->hasInit()) {
    assert(isa<ArrayConstructorExpr>(VD->getInit()));
    auto InsertPoint = cast<ArrayConstructorExpr>(VD->getInit())->getItems();
    for(uint64_t I = 0; I < ArraySize; ++I)
      Items[I] = InsertPoint[I];
  } else {
    for(uint64_t I = 0; I < ArraySize; ++I)
      Items[I] = nullptr;
  }

  uint64_t Offset;
  if(!E->EvaluateOffset(Context, Offset, &ImpliedDoEvaluator))
    return VisitExpr(E);

  ExprResult Val;
  if(Parent) {
    if(auto SE = dyn_cast<SubstringExpr>(Parent)) {
       Val = CreateSubstringExprInitializer(SE, ElementType);
    } else if(auto ME = dyn_cast<MemberExpr>(Parent)) {
      if(Offset < Items.size()) {
        const TypeConstructorExpr *Init = Items[Offset]? cast<TypeConstructorExpr>(Items[Offset]) : nullptr;
        Val = CreateMemberExprInitializer(ME, Init);
      }
    } else llvm_unreachable("invalid expression");
  } else Val = getAndCheckAnyValue(ElementType, E);

  if(Val.isUsable() && Offset < Items.size()) {
    Items[Offset] = Val.get();
    VD->setInit(ArrayConstructorExpr::Create(Context, Val.get()->getLocation(),
                                             Items, VD->getType()));
  }
}

void DataStmtEngine::VisitArrayElementExpr(ArrayElementExpr *E) {
  CreateArrayElementExprInitializer(E);
}

ExprResult DataStmtEngine::CreateSubstringExprInitializer(SubstringExpr *E,
                                                          QualType CharTy) {
  auto CTy = CharTy.getSelfOrArrayElementType()->asCharacterType();
  uint64_t Len = CTy->hasLength()? CTy->getLength() : 1;

  uint64_t Begin, End;
  if(!E->EvaluateRange(Context, Len, Begin, End, &ImpliedDoEvaluator)) {
    VisitExpr(E);
    return Sem.ExprError();
  }

  auto Val = getAndCheckValue(E->getType(), E);
  if(!Val.isUsable())
    return Sem.ExprError();
  auto StrVal = StringRef(cast<CharacterConstantExpr>(Val.get())->getValue());
  llvm::SmallString<64> Str;
  Str.resize(Len, ' ');
  uint64_t I;
  for(I = Begin; I < End; ++I) {
    if((I - Begin) >= StrVal.size())
      break;
    Str[I] = StrVal[I - Begin];
  }
  for(; I < End; ++I) Str[I] = ' ';
  return CharacterConstantExpr::Create(Context, Val.get()->getSourceRange(),
                                       Str, Context.CharacterTy);
}

void DataStmtEngine::VisitSubstringExpr(SubstringExpr *E) {
  if(auto AE = dyn_cast<ArrayElementExpr>(E->getTarget())) {
    CreateArrayElementExprInitializer(AE, E);
    return;
  }

  auto Target = dyn_cast<VarExpr>(E->getTarget());
  if(!Target)
    return VisitExpr(E);
  if(CheckVar(Target))
    return;

  auto VD = Target->getVarDecl();
  auto CharTy = VD->getType().getSelfOrArrayElementType();
  auto Val = CreateSubstringExprInitializer(E, CharTy);
  if(Val.isUsable())
    VD->setInit(Val.get());
}

ExprResult DataStmtEngine::CreateMemberExprInitializer(const MemberExpr *E,
                                                       const TypeConstructorExpr *Init) {
  auto Field = E->getField();
  auto Val = getAndCheckAnyValue(E->getType(), E);
  if(!Val.isUsable())
    return Sem.ExprError();

  auto FieldCount = E->getTarget()->getType().getSelfOrArrayElementType()->asRecordType()->getElementCount();
  SmallVector<Expr*, 16> Items(FieldCount);
  if(Init) {
    auto Args = Init->getArguments();
    for(unsigned I = 0; I < FieldCount; ++I) Items[I] = Args[I];
  } else {
    for(unsigned I = 0; I < FieldCount; ++I) Items[I] = nullptr;
  }
  Items[Field->getIndex()] = Val.get();
  return TypeConstructorExpr::Create(Context, Val.get()->getLocation(),
                                     Field->getParent(), Items);
}

void DataStmtEngine::VisitMemberExpr(MemberExpr *E) {
  if(auto AE = dyn_cast<ArrayElementExpr>(E->getTarget())) {
    CreateArrayElementExprInitializer(AE, E);
    return;
  }

  auto Target = dyn_cast<VarExpr>(E->getTarget());
  if(!Target)
    return VisitExpr(E);
  if(CheckVar(Target))
    return;

  auto VD = Target->getVarDecl();
  const TypeConstructorExpr *Init = VD->hasInit()? cast<TypeConstructorExpr>(VD->getInit()) : nullptr;
  auto Val = CreateMemberExprInitializer(E, Init);
  if(Val.isUsable())
    VD->setInit(Val.get());
}

void DataStmtEngine::VisitImpliedDoExpr(ImpliedDoExpr *E) {
  auto Start = Sem.EvalAndCheckIntExpr(E->getInitialParameter(), 1);
  auto End = Sem.EvalAndCheckIntExpr(E->getTerminalParameter(), 1);
  int64_t Inc = 1;
  if(E->hasIncrementationParameter())
    Inc = Sem.EvalAndCheckIntExpr(E->getIncrementationParameter(), 1);
  for(; Start <= End; Start+=Inc) {
    ImpliedDoEvaluator.Assign(E->getVarDecl(), Start);
    for(auto I : E->getBody())
      Visit(I);
  }
}

StmtResult Sema::ActOnDATA(ASTContext &C, SourceLocation Loc,
                           ArrayRef<Expr*> Objects,
                           ArrayRef<Expr*> Values,
                           Expr *StmtLabel) {
  DataValueIterator ValuesIt(Values);
  DataStmtEngine LHSVisitor(ValuesIt, *this, Diags, Loc);
  for(auto I : Objects) {
    LHSVisitor.Visit(I);
    if(LHSVisitor.IsDone()) break;
  }

  if(!ValuesIt.isEmpty()) {
    // more items than values
    auto FirstVal = ValuesIt.getActualValue();
    auto LastVal = ValuesIt.getActualLastValue();
    Diags.Report(FirstVal->getLocation(), diag::err_data_stmt_too_many_values)
      << SourceRange(FirstVal->getLocStart(), LastVal->getLocEnd());
  }

  auto Result = DataStmt::Create(C, Loc, Objects, Values,
                                 StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

ExprResult Sema::ActOnDATAConstantExpr(ASTContext &C,
                                       SourceLocation RepeatLoc,
                                       ExprResult RepeatCount,
                                       ExprResult Value) {
  IntegerConstantExpr *RepeatExpr = nullptr;
  bool HasErrors = false;

  if(RepeatCount.isUsable()) {
    RepeatExpr = dyn_cast<IntegerConstantExpr>(RepeatCount.get());
    if(!RepeatExpr ||
       !RepeatExpr->getValue().isStrictlyPositive()) {
      Diags.Report(RepeatCount.get()->getLocation(),
                   diag::err_expected_integer_gt_0)
        << RepeatCount.get()->getSourceRange();
      HasErrors = true;
      RepeatExpr = nullptr;
    }
  }

  if(!CheckConstantExpression(Value.get()))
    HasErrors = true;

  if(HasErrors) return ExprError();
  return RepeatExpr? RepeatedConstantExpr::Create(C, RepeatLoc,
                                                  RepeatExpr, Value.take())
                   : Value;
}

} // end namespace flang
