//===-- ParseExec.cpp - Parse Executable Construct ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Functions to parse the executable construct (R213).
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/FixedForm.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Sema.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

void Parser::CheckStmtOrder(SourceLocation Loc, StmtResult SR) {
  auto S = SR.get();
  if(SR.isUsable()) {
    if(Actions.InsideWhereConstruct(S))
      Actions.CheckValidWhereStmtPart(S);
  }

  if(PrevStmtWasSelectCase) {
    PrevStmtWasSelectCase = false;
    if(SR.isUsable() && (isa<SelectionCase>(S) ||
       (isa<ConstructPartStmt>(S) &&
        cast<ConstructPartStmt>(S)->getConstructStmtClass() == ConstructPartStmt::EndSelectStmtClass)))
      return;
    Diag.Report(SR.isUsable()? S->getLocation() : Loc,
                diag::err_expected_case_or_end_select);
  }
  if(SR.isUsable() && isa<SelectCaseStmt>(S))
    PrevStmtWasSelectCase = true;
}

/// ParseExecutableConstruct - Parse the executable construct.
///
///   [R213]:
///     executable-construct :=
///         action-stmt
///      or associate-construct
///      or block-construct
///      or case-construct
///      or critical-construct
///      or do-construct
///      or forall-construct
///      or if-construct
///      or select-type-construct
///      or where-construct
StmtResult Parser::ParseExecutableConstruct() {
  ParseStatementLabel();
  ParseConstructNameLabel();
  LookForExecutableStmtKeyword(StmtLabel || StmtConstructName.isUsable()?
                               false : true);

  auto Loc = Tok.getLocation();
  StmtResult SR = ParseActionStmt();
  CheckStmtOrder(Loc, SR);
  if (SR.isInvalid()) return StmtError();
  if (!SR.isUsable()) return StmtResult();

  return SR;
}

/// ParseActionStmt - Parse an action statement.
///
///   [R214]:
///     action-stmt :=
///         allocate-stmt
///      or assignment-stmt
///      or backspace-stmt
///      or call-stmt
///      or close-stmt
///      or continue-stmt
///      or cycle-stmt
///      or deallocate-stmt
///      or end-function-stmt
///      or end-mp-subprogram-stmt
///      or end-program-stmt
///      or end-subroutine-stmt
///      or endfile-stmt
///      or error-stop-stmt
///      or exit-stmt
///      or flush-stmt
///      or forall-stmt
///      or goto-stmt
///      or if-stmt
///      or inquire-stmt
///      or lock-stmt
///      or nullify-stmt
///      or open-stmt
///      or pointer-assignment-stmt
///      or print-stmt
///      or read-stmt
///      or return-stmt
///      or rewind-stmt
///      or stop-stmt
///      or sync-all-stmt
///      or sync-images-stmt
///      or sync-memory-stmt
///      or unlock-stmt
///      or wait-stmt
///      or where-stmt
///      or write-stmt
///[obs] or arithmetic-if-stmt
///[obs] or computed-goto-stmt
Parser::StmtResult Parser::ParseActionStmt() {
  StmtResult SR;
  switch (Tok.getKind()) {
  default:
    return ParseAssignmentStmt();
  case tok::kw_ASSIGN:
    return ParseAssignStmt();
  case tok::kw_GOTO:
    return ParseGotoStmt();
  case tok::kw_IF:
    return ParseIfStmt();
  case tok::kw_ELSEIF:
    return ParseElseIfStmt();
  case tok::kw_ELSE:
    return ParseElseStmt();
  case tok::kw_ENDIF:
    return ParseEndIfStmt();
  case tok::kw_DO:
    return ParseDoStmt();
  case tok::kw_DOWHILE:
    if(Features.FixedForm) {
      if(!IsNextToken(tok::l_paren))
        return ReparseAmbiguousDoWhileStatement();
    }
    return ParseDoWhileStmt(false);
  case tok::kw_ENDDO:
    return ParseEndDoStmt();
  case tok::kw_CYCLE:
    return ParseCycleStmt();
  case tok::kw_SELECTCASE:
    return ParseSelectCaseStmt();
  case tok::kw_CASE:
    return ParseCaseStmt();
  case tok::kw_ENDSELECT:
    return ParseEndSelectStmt();
  case tok::kw_EXIT:
    return ParseExitStmt();
  case tok::kw_CONTINUE:
    return ParseContinueStmt();
  case tok::kw_STOP:
    return ParseStopStmt();
  case tok::kw_PRINT:
    return ParsePrintStmt();
  case tok::kw_WRITE:
    return ParseWriteStmt();
  case tok::kw_FORMAT:
    return ParseFORMATStmt();
  case tok::kw_RETURN:
    return ParseReturnStmt();
  case tok::kw_CALL:
    return ParseCallStmt();
  case tok::kw_WHERE:
    return ParseWhereStmt();
  case tok::kw_ELSEWHERE:
    return ParseElseWhereStmt();
  case tok::kw_ENDWHERE:
    return ParseEndWhereStmt();
  case tok::kw_READ:
  case tok::kw_OPEN:
  case tok::kw_CLOSE:
  case tok::kw_INQUIRE:
  case tok::kw_BACKSPACE:
  case tok::kw_ENDFILE:
  case tok::kw_REWIND:
    Diag.Report(ConsumeToken(), diag::err_unsupported_stmt);
    return StmtError();

  case tok::kw_END:
    // TODO: All of the end-* stmts.
  case tok::kw_ENDFUNCTION:
  case tok::kw_ENDPROGRAM:
  case tok::kw_ENDSUBPROGRAM:
  case tok::kw_ENDSUBROUTINE:
    if(Features.FixedForm) {
      auto StmtPoint = LocFirstStmtToken;
      auto RetPoint = ConsumeToken();
      ConsumeIfPresent(tok::identifier);
      if(IsPresent(tok::l_paren) || IsPresent(tok::equal))
         return ReparseAmbiguousAssignmentStatement(StmtPoint);
      StartStatementReparse(RetPoint);
      LookForExecutableStmtKeyword(StmtLabel? false : true);
    }
    // Handle in parent.
    return StmtResult();

  case tok::eof:
    return StmtResult();
  }

  return SR;
}

/// DoWhile's fixedForm ambiguity is special as it can be reparsed
/// as DO statement or as an assignment statement.
Parser::StmtResult Parser::ReparseAmbiguousDoWhileStatement() {
  StartStatementReparse();
  ParseStatementLabel();
  ParseConstructNameLabel();
  ReLexAmbiguousIdentifier(fixedForm::KeywordMatcher(fixedForm::KeywordFilter(tok::kw_DO)));
  return ParseActionStmt();
}

Parser::StmtResult Parser::ReparseAmbiguousAssignmentStatement(SourceLocation Where) {
  StartStatementReparse(Where);
  return ParseAmbiguousAssignmentStmt();
}

Parser::StmtResult Parser::ParseAssignStmt() {
  SourceLocation Loc = ConsumeToken();

  auto Value = ParseStatementLabelReference(false);
  if(Value.isInvalid()) {
    Diag.Report(getExpectedLoc(), diag::err_expected_stmt_label_after)
        << "ASSIGN";
    return StmtError();
  }
  ConsumeToken();
  if(!ExpectAndConsumeFixedFormAmbiguous(tok::kw_TO, diag::err_expected_kw, "to"))
    return StmtError();

  auto IDInfo = Tok.getIdentifierInfo();
  auto IDRange = getTokenRange();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier))
    return StmtError();
  auto VD = Actions.ExpectVarRefOrDeclImplicitVar(IDLoc, IDInfo);
  if(!VD)
    return StmtError();
  auto Var = VarExpr::Create(Context, IDRange, VD);

  return Actions.ActOnAssignStmt(Context, Loc, Value, Var, StmtLabel);
}

Parser::StmtResult Parser::ParseGotoStmt() {
  SourceLocation Loc = ConsumeToken();
  if(ConsumeIfPresent(tok::l_paren)) {
    // computed goto.
    SmallVector<Expr*, 4> Targets;
    do {
      auto E = ParseStatementLabelReference();
      if(E.isInvalid()) break;
      Targets.append(1, E.get());
    } while(ConsumeIfPresent(tok::comma));
    ExprResult Operand;
    bool ParseOperand = true;
    if(!ExpectAndConsume(tok::r_paren)) {
      if(!SkipUntil(tok::r_paren)) ParseOperand = false;
    }
    if(ParseOperand) Operand = ParseExpectedExpression();
    return Actions.ActOnComputedGotoStmt(Context, Loc, Targets, Operand, StmtLabel);
  }

  auto Destination = ParseStatementLabelReference();
  if(Destination.isInvalid()) {
    if(!IsPresent(tok::identifier)) {
      Diag.Report(getExpectedLoc(), diag::err_expected_stmt_label_after)
          << "GO TO";
      return StmtError();
    }
    auto IDInfo = Tok.getIdentifierInfo();
    auto IDLoc = ConsumeToken();
    auto VD = Actions.ExpectVarRef(IDLoc, IDInfo);
    if(!VD) return StmtError();
    auto Var = VarExpr::Create(Context, IDLoc, VD);

    // Assigned goto
    SmallVector<Expr*, 4> AllowedValues;
    if(ConsumeIfPresent(tok::l_paren)) {
      do {
        auto E = ParseStatementLabelReference();
        if(E.isInvalid()) {
          Diag.Report(getExpectedLoc(), diag::err_expected_stmt_label);
          SkipUntilNextStatement();
          return Actions.ActOnAssignedGotoStmt(Context, Loc, Var, AllowedValues, StmtLabel);
        }
        AllowedValues.append(1, E.get());
      } while(ConsumeIfPresent(tok::comma));
      ExpectAndConsume(tok::r_paren);
    }
    return Actions.ActOnAssignedGotoStmt(Context, Loc, Var, AllowedValues, StmtLabel);
  }
  // Uncoditional goto
  return Actions.ActOnGotoStmt(Context, Loc, Destination, StmtLabel);
}

/// ParseIfStmt
///   [R802]:
///     if-construct :=
///       if-then-stmt
///         block
///       [else-if-stmt
///          block
///       ]
///       ...
///       [
///       else-stmt
///          block
///       ]
///       end-if-stmt
///   [R803]:
///     if-then-stmt :=
///       [ if-construct-name : ]
///       IF (scalar-logical-expr) THEN
///   [R804]:
///     else-if-stmt :=
///       ELSE IF (scalar-logical-expr) THEN
///       [ if-construct-name ]
///   [R805]:
///     else-stmt :=
///       ELSE
///       [ if-construct-name ]
///   [R806]:
///     end-if-stmt :=
///       END IF
///       [ if-construct-name ]
///
///   [R807]:
///     if-stmt :=
///       IF(scalar-logic-expr) action-stmt

ExprResult Parser::ParseExpectedConditionExpression(const char *DiagAfter) {
  if (!ExpectAndConsume(tok::l_paren, diag::err_expected_lparen_after,
      DiagAfter))
    return ExprError();
  ExprResult Condition = ParseExpectedFollowupExpression("(");
  if(Condition.isInvalid()) return Condition;
  if (!ExpectAndConsume(tok::r_paren))
    return ExprError();
  return Condition;
}

Parser::StmtResult Parser::ParseIfStmt() {
  auto Loc = ConsumeToken();

  ExprResult Condition;
  if (!ExpectAndConsume(tok::l_paren, diag::err_expected_lparen_after, "IF"))
    goto error;
  Condition = ParseExpectedFollowupExpression("(");
  if(Condition.isInvalid()) {
    if(!SkipUntil(tok::r_paren, true, true))
      goto error;
  }
  if (!ExpectAndConsume(tok::r_paren)) goto error;

  if(Features.FixedForm && !Tok.isAtStartOfStatement())
    ReLexAmbiguousIdentifier(FixedFormAmbiguities.getMatcherForKeywordsAfterIF());
  if (!ConsumeIfPresent(tok::kw_THEN)) {
    // if-stmt
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_executable_stmt);
      return StmtError();
    }
    auto Result = Actions.ActOnIfStmt(Context, Loc, Condition, StmtConstructName, StmtLabel);
    if(Result.isInvalid()) return Result;
    // NB: Don't give the action stmt my label
    StmtLabel = nullptr;
    auto Action = ParseActionStmt();
    Actions.ActOnEndIfStmt(Context, Loc, ConstructName(SourceLocation(), nullptr), nullptr);
    return Action.isInvalid()? StmtError() : Result;
  }

  // if-construct.
  return Actions.ActOnIfStmt(Context, Loc, Condition, StmtConstructName, StmtLabel);
error:
  SkipUntilNextStatement();
  return Actions.ActOnIfStmt(Context, Loc, Condition, StmtConstructName, StmtLabel);
}

// FIXME: fixed-form THENconstructname
Parser::StmtResult Parser::ParseElseIfStmt() {
  auto Loc = ConsumeToken();
  ExprResult Condition;
  if (!ExpectAndConsume(tok::l_paren, diag::err_expected_lparen_after, "ELSE IF"))
    goto error;
  Condition = ParseExpectedFollowupExpression("(");
  if(Condition.isInvalid()) {
    if(!SkipUntil(tok::r_paren, true, true))
      goto error;
  }
  if (!ExpectAndConsume(tok::r_paren)) goto error;
  if (!ExpectAndConsumeFixedFormAmbiguous(tok::kw_THEN, diag::err_expected_kw, "THEN"))
    goto error;
  ParseTrailingConstructName();
  return Actions.ActOnElseIfStmt(Context, Loc, Condition, StmtConstructName, StmtLabel);
error:
  SkipUntilNextStatement();
  return Actions.ActOnElseIfStmt(Context, Loc, Condition, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseElseStmt() {
  auto Loc = ConsumeToken();
  ParseTrailingConstructName();
  return Actions.ActOnElseStmt(Context, Loc, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseEndIfStmt() {
  auto Loc = ConsumeToken();
  ParseTrailingConstructName();
  return Actions.ActOnEndIfStmt(Context, Loc, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseDoStmt() {
  auto Loc = ConsumeToken();
  auto CurStmtLoc = LocFirstStmtToken;

  ExprResult TerminalStmt;
  VarExpr *DoVar = nullptr;
  ExprResult E1, E2, E3;
  auto EqLoc = Loc;

  if(IsPresent(tok::int_literal_constant)) {
    TerminalStmt = ParseStatementLabelReference();
    if(TerminalStmt.isInvalid()) return StmtError();
  }
  bool isDo = ConsumeIfPresent(tok::comma);
  if(isDo && IsPresent(tok::kw_WHILE))
    return ParseDoWhileStmt(isDo);

  // the do var
  auto IDInfo = Tok.getIdentifierInfo();
  auto IDRange = getTokenRange();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier))
    goto error;

  EqLoc = getMaxLocationOfCurrentToken();
  if(Features.FixedForm && !isDo && IsPresent(tok::l_paren))
    return ReparseAmbiguousAssignmentStatement();
  if(!ExpectAndConsume(tok::equal))
    goto error;
  E1 = ParseExpectedFollowupExpression("=");
  if(E1.isInvalid()) goto error;
  if(Features.FixedForm && !isDo && Tok.isAtStartOfStatement())
    return ReparseAmbiguousAssignmentStatement(CurStmtLoc);
  if(!ExpectAndConsume(tok::comma)) goto error;
  E2 = ParseExpectedFollowupExpression(",");
  if(E2.isInvalid()) goto error;
  if(ConsumeIfPresent(tok::comma)) {
    E3 = ParseExpectedFollowupExpression(",");
    if(E3.isInvalid()) goto error;
  }

  if(auto VD = Actions.ExpectVarRefOrDeclImplicitVar(IDLoc, IDInfo))
    DoVar = VarExpr::Create(Context, IDRange, VD);
  return Actions.ActOnDoStmt(Context, Loc, EqLoc, TerminalStmt,
                             DoVar, E1, E2, E3, StmtConstructName, StmtLabel);
error:
  if(IDInfo) {
    if(auto VD = Actions.ExpectVarRefOrDeclImplicitVar(IDLoc, IDInfo))
      DoVar = VarExpr::Create(Context, IDRange, VD);
  }
  SkipUntilNextStatement();
  return Actions.ActOnDoStmt(Context, Loc, EqLoc, TerminalStmt,
                             DoVar, E1, E2, E3, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseDoWhileStmt(bool isDo) {
  auto Loc = ConsumeToken();
  auto Condition = ParseExpectedConditionExpression("WHILE");
  return Actions.ActOnDoWhileStmt(Context, Loc, Condition, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseEndDoStmt() {
  auto Loc = ConsumeToken();
  ParseTrailingConstructName();
  return Actions.ActOnEndDoStmt(Context, Loc, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseCycleStmt() {
  auto Loc = ConsumeToken();
  ParseTrailingConstructName();
  return Actions.ActOnCycleStmt(Context, Loc, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseExitStmt() {
  auto Loc = ConsumeToken();
  ParseTrailingConstructName();
  return Actions.ActOnExitStmt(Context, Loc, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseSelectCaseStmt() {
  auto Loc = ConsumeToken();

  ExprResult Operand;
  if(ExpectAndConsume(tok::l_paren)) {
    Operand = ParseExpectedFollowupExpression("(");
    if(Operand.isUsable()) {
      if(!ExpectAndConsume(tok::r_paren))
        SkipUntilNextStatement();
    } else SkipUntilNextStatement();
  } else SkipUntilNextStatement();

  return Actions.ActOnSelectCaseStmt(Context, Loc, Operand, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseCaseStmt() {
  auto Loc = ConsumeToken();
  if(ConsumeIfPresent(tok::kw_DEFAULT)) {
    ParseTrailingConstructName();
    return Actions.ActOnCaseDefaultStmt(Context, Loc, StmtConstructName, StmtLabel);
  }

  SmallVector<Expr*, 8> Values;

  ExpectAndConsume(tok::l_paren);
  do {
    auto ColonLoc = Tok.getLocation();
    if(ConsumeIfPresent(tok::colon)) {
      auto E = ParseExpectedFollowupExpression(":");
      if(E.isInvalid()) goto error;
      if(E.isUsable())
        Values.push_back(RangeExpr::Create(Context, ColonLoc, nullptr, E.get()));
    } else {
      auto E = ParseExpectedExpression();
      if(E.isInvalid()) goto error;
      ColonLoc = Tok.getLocation();
      if(ConsumeIfPresent(tok::colon)) {
        if(!(IsPresent(tok::comma) || IsPresent(tok::r_paren))) {
          auto E2 = ParseExpectedFollowupExpression(":");
          if(E2.isInvalid()) goto error;
          if(E.isUsable() || E2.isUsable())
            Values.push_back(RangeExpr::Create(Context, ColonLoc, E.get(), E2.get()));
        } else {
          if(E.isUsable())
            Values.push_back(RangeExpr::Create(Context, ColonLoc, E.get(), nullptr));
        }
      } else {
        if(E.isUsable())
          Values.push_back(E.get());
      }
    }
  } while(ConsumeIfPresent(tok::comma));

  if(ExpectAndConsume(tok::r_paren)) {
    ParseTrailingConstructName();
  } else SkipUntilNextStatement();

  return Actions.ActOnCaseStmt(Context, Loc, Values, StmtConstructName, StmtLabel);

error:
  if(SkipUntil(tok::r_paren)) {
    ParseTrailingConstructName();
  } else SkipUntilNextStatement();
  return Actions.ActOnCaseStmt(Context, Loc, Values, StmtConstructName, StmtLabel);
}

Parser::StmtResult Parser::ParseEndSelectStmt() {
  auto Loc = ConsumeToken();
  ParseTrailingConstructName();
  return Actions.ActOnEndSelectStmt(Context, Loc, StmtConstructName, StmtLabel);
}

/// ParseContinueStmt
///   [R839]:
///     continue-stmt :=
///       CONTINUE
Parser::StmtResult Parser::ParseContinueStmt() {
  auto Loc = ConsumeToken();

  return Actions.ActOnContinueStmt(Context, Loc, StmtLabel);
}

/// ParseStopStmt
///   [R840]:
///     stop-stmt :=
///       STOP [ stop-code ]
///   [R841]:
///     stop-code :=
///       scalar-char-constant or
///       digit [ digit [ digit [ digit [ digit ] ] ] ]
Parser::StmtResult Parser::ParseStopStmt() {
  auto Loc = ConsumeToken();

  //FIXME: parse optional stop-code.
  return Actions.ActOnStopStmt(Context, Loc, ExprResult(), StmtLabel);
}

Parser::StmtResult Parser::ParseReturnStmt() {
  auto Loc = ConsumeToken();
  ExprResult E;
  if(!Tok.isAtStartOfStatement())
    E = ParseExpression();

  return Actions.ActOnReturnStmt(Context, Loc, E, StmtLabel);
}

Parser::StmtResult Parser::ParseCallStmt() {
  auto Loc = ConsumeToken();
  SourceLocation RParenLoc = getExpectedLoc();

  auto ID = Tok.getIdentifierInfo();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier))
    return StmtError();

  SmallVector<Expr*, 8> Arguments;
  if(!Tok.isAtStartOfStatement()) {
    if(ParseFunctionCallArgumentList(Arguments, RParenLoc).isInvalid())
      SkipUntilNextStatement();
  }

  return Actions.ActOnCallStmt(Context, Loc, RParenLoc, IDLoc, ID, Arguments, StmtLabel);
}

Parser::StmtResult Parser::ParseAmbiguousAssignmentStmt() {
  ParseStatementLabel();
  auto S = ParseAssignmentStmt();
  if(S.isInvalid()) SkipUntilNextStatement();
  else ExpectStatementEnd();
  return S;
}

/// ParseAssignmentStmt
///   [R732]:
///     assignment-stmt :=
///         variable = expr
Parser::StmtResult Parser::ParseAssignmentStmt() {
  ExprResult LHS = ParsePrimaryExpr(true);
  if(LHS.isInvalid()) return StmtError();

  SourceLocation Loc = Tok.getLocation();
  if(!ConsumeIfPresent(tok::equal)) {
    Diag.Report(getExpectedLoc(),diag::err_expected_equal);
    return StmtError();
  }

  ExprResult RHS = ParseExpectedFollowupExpression("=");
  if(RHS.isInvalid()) return StmtError();
  return Actions.ActOnAssignmentStmt(Context, Loc, LHS, RHS, StmtLabel);
}

/// ParseWHEREStmt - Parse the WHERE statement.
///
///   [R743]:
///     where-stmt :=
///         WHERE ( mask-expr ) where-assignment-stmt
Parser::StmtResult Parser::ParseWhereStmt() {
  auto Loc = ConsumeToken();
  ExpectAndConsume(tok::l_paren);
  auto Mask = ParseExpectedExpression();
  if(!Mask.isInvalid())
    ExpectAndConsume(tok::r_paren);
  else SkipUntil(tok::r_paren);
  if(!Tok.isAtStartOfStatement()) {
    auto Label = StmtLabel;
    StmtLabel = nullptr;
    auto Body = ParseActionStmt();
    if(Body.isInvalid())
      return Body;
    return Actions.ActOnWhereStmt(Context, Loc, Mask, Body, Label);
  }
  return Actions.ActOnWhereStmt(Context, Loc, Mask, StmtLabel);
}

StmtResult Parser::ParseElseWhereStmt() {
  auto Loc = ConsumeToken();
  return Actions.ActOnElseWhereStmt(Context, Loc, StmtLabel);
}

StmtResult Parser::ParseEndWhereStmt() {
  auto Loc = ConsumeToken();
  return Actions.ActOnEndWhereStmt(Context, Loc, StmtLabel);
}


/// ParsePrintStatement
///   [R912]:
///     print-stmt :=
///         PRINT format [, output-item-list]
///   [R915]:
///     format :=
///         default-char-expr
///      or label
///      or *

Parser::StmtResult Parser::ParsePrintStmt() {
  SourceLocation Loc = ConsumeToken();

  FormatSpec *FS = ParseFMTSpec(false);
  if(!ExpectAndConsume(tok::comma))
    return StmtError();

  SmallVector<ExprResult, 4> OutputItemList;
  ParseIOList(OutputItemList);

  return Actions.ActOnPrintStmt(Context, Loc, FS, OutputItemList, StmtLabel);
}

Parser::StmtResult Parser::ParseWriteStmt() {
  SourceLocation Loc = ConsumeToken();

  // clist
  if(!ExpectAndConsume(tok::l_paren))
    return StmtError();

  UnitSpec *US = nullptr;
  FormatSpec *FS = nullptr;

  US = ParseUNITSpec(false);
  if(ConsumeIfPresent(tok::comma)) {
    bool IsFormatLabeled = false;
    if(ConsumeIfPresent(tok::kw_FMT)) {
      if(!ExpectAndConsume(tok::equal))
        return StmtError();
      IsFormatLabeled = true;
    }
    FS = ParseFMTSpec(IsFormatLabeled);
  }

  if(!ExpectAndConsume(tok::r_paren))
    return StmtError();

  // iolist
  SmallVector<ExprResult, 4> OutputItemList;
  ParseIOList(OutputItemList);

  return Actions.ActOnWriteStmt(Context, Loc, US, FS, OutputItemList, StmtLabel);
}

UnitSpec *Parser::ParseUNITSpec(bool IsLabeled) {
  auto Loc = Tok.getLocation();
  if(!ConsumeIfPresent(tok::star)) {
    auto E = ParseExpression();
    if(!E.isInvalid())
      return Actions.ActOnUnitSpec(Context, E, Loc, IsLabeled);
  }
  return Actions.ActOnStarUnitSpec(Context, Loc, IsLabeled);
}

FormatSpec *Parser::ParseFMTSpec(bool IsLabeled) {
  auto Loc = Tok.getLocation();
  if(!ConsumeIfPresent(tok::star)) {
    if(Tok.is(tok::int_literal_constant)) {
      auto Destination = ParseStatementLabelReference();
      if(!Destination.isInvalid())
        return Actions.ActOnLabelFormatSpec(Context, Loc, Destination);
    }
    auto E = ParseExpression();
    if(E.isUsable())
      return Actions.ActOnExpressionFormatSpec(Context, Loc, E.get());
    // NB: return empty format string on error.
    return Actions.ActOnExpressionFormatSpec(Context, Loc,
                                             CharacterConstantExpr::Create(Context, Loc, "", Context.CharacterTy));
  }

  return Actions.ActOnStarFormatSpec(Context, Loc);
}

void Parser::ParseIOList(SmallVectorImpl<ExprResult> &List) {
  while(!Tok.isAtStartOfStatement()) {
    auto E = ParseExpression();
    if(E.isUsable()) List.push_back(E);
    if(!ConsumeIfPresent(tok::comma))
      break;
  }
}

} //namespace flang
