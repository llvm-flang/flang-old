//===-- ParseSpecStmt.cpp - Parse Specification Statements ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Sema.h"

namespace flang {

/// ParseDIMENSIONStmt - Parse the DIMENSION statement.
///
///   [R535]:
///     dimension-stmt :=
///         DIMENSION [::] array-name ( array-spec ) #
///         # [ , array-name ( array-spec ) ] ...
Parser::StmtResult Parser::ParseDIMENSIONStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();
  ConsumeIfPresent(tok::coloncolon);

  SmallVector<Stmt*, 8> StmtList;
  SmallVector<ArraySpec*, 4> Dimensions;
  while (true) {
    auto IDLoc = Tok.getLocation();
    auto II = Tok.getIdentifierInfo();
    if(!ExpectAndConsume(tok::identifier)) {
      if(!SkipUntil(tok::comma, tok::identifier, true, true)) break;
      if(ConsumeIfPresent(tok::comma)) continue;
      else {
        IDLoc = Tok.getLocation();
        II = Tok.getIdentifierInfo();
        ConsumeToken();
      }
    }

    // FIXME: improve error recovery
    Dimensions.clear();
    if(ParseArraySpec(Dimensions)) return StmtError();

    auto Stmt = Actions.ActOnDIMENSION(Context, Loc, IDLoc, II,
                                       Dimensions, nullptr);
    if(Stmt.isUsable()) StmtList.push_back(Stmt.take());

    if(Tok.isAtStartOfStatement()) break;
    if(!ExpectAndConsume(tok::comma)) {
      if(!SkipUntil(tok::comma)) break;
    }
  }

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseEQUIVALENCEStmt - Parse the EQUIVALENCE statement.
///
///   [R554]:
///     equivalence-stmt :=
///         EQUIVALENCE equivalence-set-list
Parser::StmtResult Parser::ParseEQUIVALENCEStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();
  SmallVector<Stmt*, 8> StmtList;
  SmallVector<Expr*, 8> ObjectList;

  bool OuterError = false;
  while(true) {
    auto PartLoc = Tok.getLocation();
    if(!ExpectAndConsume(tok::l_paren)) {
      if(!SkipUntil(tok::l_paren)) {
        OuterError = true;
        break;
      }
    }

    ObjectList.clear();
    bool InnerError = false;
    do {
      auto E = ParseExpectedExpression();
      if(E.isInvalid()) {
        InnerError = true;
        break;
      } else if(E.isUsable())
        ObjectList.push_back(E.get());
    } while(ConsumeIfPresent(tok::comma));

    auto S = Actions.ActOnEQUIVALENCE(Context, Loc, PartLoc, ObjectList, nullptr);
    if(S.isUsable())
      StmtList.push_back(S.get());

    if(InnerError) {
      if(!SkipUntil(tok::r_paren, true, true)) {
        OuterError = true;
        break;
      }
    }
    if(!ExpectAndConsume(tok::r_paren)) {
      if(!SkipUntil(tok::r_paren)) {
        OuterError = true;
        break;
      }
    }

    if(ConsumeIfPresent(tok::comma)) continue;
    if(IsPresent(tok::l_paren)) {
      ExpectAndConsume(tok::comma);
      continue;
    }
    break;
  }

  if(OuterError) SkipUntilNextStatement();
  else ExpectStatementEnd();

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseCOMMONStmt - Parse the COMMON statement.
///
///   [5.5.2] R557:
///     common-stmt :=
///         COMMON #
///         # [ / [common-block-name] / ] common-block-object-list #
///         # [ [,] / [common-block-name / #
///         #   common-block-object-list ] ...
Parser::StmtResult Parser::ParseCOMMONStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();
  SmallVector<Stmt*, 8> StmtList;
  SmallVector<Expr*, 8> ObjectList;

  SourceLocation BlockIDLoc;
  const IdentifierInfo *BlockID = nullptr;

  bool Error = false;
  do {
    if(ConsumeIfPresent(tok::slash)) {
      if(ConsumeIfPresent(tok::slash))
        BlockID = nullptr;
      else {
        BlockIDLoc = Tok.getLocation();
        BlockID = Tok.getIdentifierInfo();
        if(!ExpectAndConsume(tok::identifier)) {
          Error = true;
          break;
        }
        if(!ExpectAndConsume(tok::slash)) {
          Error = true;
          break;
        }
      }
    } else if(ConsumeIfPresent(tok::slashslash))
      BlockID = nullptr;

    auto IDLoc = Tok.getLocation();
    auto IDInfo = Tok.getIdentifierInfo();
    SmallVector<ArraySpec*, 8> Dimensions;

    if(!ExpectAndConsume(tok::identifier)) {
      Error = true;
      break;
    }
    if(IsPresent(tok::l_paren)) {
      if(ParseArraySpec(Dimensions)) {
        Error = true;
        break;
      }
    }

    Actions.ActOnCOMMON(Context, Loc, BlockIDLoc,
                        IDLoc, BlockID, IDInfo,
                        Dimensions);
  } while(ConsumeIfPresent(tok::comma));


  if(Error) SkipUntilNextStatement();
  else ExpectStatementEnd();

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParsePARAMETERStmt - Parse the PARAMETER statement.
///
///   [R548]:
///     parameter-stmt :=
///         PARAMETER ( named-constant-def-list )
Parser::StmtResult Parser::ParsePARAMETERStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();

  SmallVector<Stmt*, 8> StmtList;

  if(!ExpectAndConsume(tok::l_paren)) {
    if(!SkipUntil(tok::l_paren))
      return StmtError();
  }

  while(true) {
    auto IDLoc = Tok.getLocation();
    auto II = Tok.getIdentifierInfo();
    if(!ExpectAndConsume(tok::identifier)) {
      if(!SkipUntil(tok::comma)) break;
      else continue;
    }

    auto EqualLoc = Tok.getLocation();
    if(!ExpectAndConsume(tok::equal)) {
      if(!SkipUntil(tok::comma)) break;
      else continue;
    }

    ExprResult ConstExpr = ParseExpression();
    if(ConstExpr.isUsable()) {
      auto Stmt = Actions.ActOnPARAMETER(Context, Loc, EqualLoc,
                                         IDLoc, II,
                                         ConstExpr, nullptr);
      if(Stmt.isUsable())
        StmtList.push_back(Stmt.take());
    }

    if(ConsumeIfPresent(tok::comma))
      continue;
    if(isTokenIdentifier() && !Tok.isAtStartOfStatement()) {
      ExpectAndConsume(tok::comma);
      continue;
    }
    break;
  }

  if(!ExpectAndConsume(tok::r_paren))
    SkipUntilNextStatement();

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseIMPLICITStmt - Parse the IMPLICIT statement.
///
///   [R560]:
///     implicit-stmt :=
///         IMPLICIT implicit-spec-list
///      or IMPLICIT NONE
Parser::StmtResult Parser::ParseIMPLICITStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();

  if (ConsumeIfPresent(tok::kw_NONE)) {
    auto Result = Actions.ActOnIMPLICIT(Context, Loc, StmtLabel);
    ExpectStatementEnd();
    return Result;
  }

  SmallVector<Stmt*, 8> StmtList;

  while(true) {
    // FIXME: improved error recovery
    DeclSpec DS;
    if (ParseDeclarationTypeSpec(DS, false))
      return StmtError();

    if(!ExpectAndConsume(tok::l_paren)) {
      if(!SkipUntil(tok::l_paren))
        break;
    }

    bool InnerError = false;
    while(true) {
      auto FirstLoc = Tok.getLocation();
      auto First = Tok.getIdentifierInfo();
      if(!ExpectAndConsume(tok::identifier, diag::err_expected_letter)) {
        if(!SkipUntil(tok::comma)) {
          InnerError = true;
          break;
        }
        continue;
      }
      if(First->getName().size() > 1) {
        Diag.Report(FirstLoc, diag::err_expected_letter);
      }

      const IdentifierInfo *Second = nullptr;
      if (ConsumeIfPresent(tok::minus)) {
        auto SecondLoc = Tok.getLocation();
        Second = Tok.getIdentifierInfo();
        if(!ExpectAndConsume(tok::identifier, diag::err_expected_letter)) {
          if(!SkipUntil(tok::comma)) {
            InnerError = true;
            break;
          }
          continue;
        }
        if(Second->getName().size() > 1) {
          Diag.Report(SecondLoc, diag::err_expected_letter);
        }
      }

      auto Stmt = Actions.ActOnIMPLICIT(Context, Loc, DS,
                                        std::make_pair(First, Second), nullptr);
      if(Stmt.isUsable())
        StmtList.push_back(Stmt.take());

      if(ConsumeIfPresent(tok::comma))
        continue;
      break;
    }

    if(InnerError && Tok.isAtStartOfStatement())
      break;
    if(!ExpectAndConsume(tok::r_paren)) {
      if(!SkipUntil(tok::r_paren))
        break;
    }

    if(Tok.isAtStartOfStatement()) break;
    if(!ExpectAndConsume(tok::comma)) {
      if(!SkipUntil(tok::comma))
        break;
    }
  }

  ExpectStatementEnd();
  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseEXTERNALStmt - Parse the EXTERNAL statement.
///
///   [R1210]:
///     external-stmt :=
///         EXTERNAL [::] external-name-list
Parser::StmtResult Parser::ParseEXTERNALStmt() {
  return ParseINTRINSICStmt(/*IsActuallyExternal=*/ true);
}

/// ParseINTRINSICStmt - Parse the INTRINSIC statement.
///
///   [R1216]:
///     intrinsic-stmt :=
///         INTRINSIC [::] intrinsic-procedure-name-list
Parser::StmtResult Parser::ParseINTRINSICStmt(bool IsActuallyExternal) {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();
  ConsumeIfPresent(tok::coloncolon);

  SmallVector<Stmt *,8> StmtList;

  while(true) {
    auto IDLoc = Tok.getLocation();
    auto II = Tok.getIdentifierInfo();
    if(!ExpectAndConsume(tok::identifier)) {
      if(!SkipUntil(tok::comma, tok::identifier, true, true)) break;
      if(ConsumeIfPresent(tok::comma)) continue;
      else {
        IDLoc = Tok.getLocation();
        II = Tok.getIdentifierInfo();
        ConsumeToken();
      }
    }

    auto Stmt = IsActuallyExternal?
                  Actions.ActOnEXTERNAL(Context, Loc, IDLoc,
                                        II, nullptr):
                  Actions.ActOnINTRINSIC(Context, Loc, IDLoc,
                                         II, nullptr);
    if(Stmt.isUsable())
      StmtList.push_back(Stmt.take());

    if(Tok.isAtStartOfStatement()) break;
    if(!ExpectAndConsume(tok::comma)) {
      if(!SkipUntil(tok::comma)) break;
    }
  }

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseSAVEStmt - Parse the SAVE statement.
///
///   [R543]:
///     save-stmt :=
///         SAVE [ [::] saved-entity-list ]
Parser::StmtResult Parser::ParseSAVEStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  auto Loc = ConsumeToken();
  if(Tok.isAtStartOfStatement())
    return Actions.ActOnSAVE(Context, Loc, StmtLabel);

  bool IsSaveStmt = ConsumeIfPresent(tok::coloncolon);
  SmallVector<Stmt *,8> StmtList;
  bool ListParsedOk = true;

  auto IDLoc = Tok.getLocation();
  auto II = Tok.getIdentifierInfo();
  StmtResult Stmt;
  if(ConsumeIfPresent(tok::slash)) {
    IDLoc = Tok.getLocation();
    II = Tok.getIdentifierInfo();
    if(ExpectAndConsume(tok::identifier)) {
      if(!ExpectAndConsume(tok::slash))
        ListParsedOk = false;
      Stmt = Actions.ActOnSAVECommonBlock(Context, Loc, IDLoc, II);
    }
    else ListParsedOk = false;
  }
  else if(ExpectAndConsume(tok::identifier)) {
    if(!IsSaveStmt && Features.FixedForm && (IsPresent(tok::equal) || IsPresent(tok::l_paren)))
      return ReparseAmbiguousAssignmentStatement();
    Stmt = Actions.ActOnSAVE(Context, Loc, IDLoc, II, nullptr);
  } else ListParsedOk = false;

  if(Stmt.isUsable())
    StmtList.push_back(Stmt.get());
  if(ListParsedOk) {
    while(ConsumeIfPresent(tok::comma)) {
      IDLoc = Tok.getLocation();
      II = Tok.getIdentifierInfo();
      if(ConsumeIfPresent(tok::slash)) {
        IDLoc = Tok.getLocation();
        II = Tok.getIdentifierInfo();
        if(!ExpectAndConsume(tok::identifier)) {
          ListParsedOk = false;
          break;
        }
        if(!ExpectAndConsume(tok::slash)) {
          ListParsedOk = false;
          break;
        }
        Stmt = Actions.ActOnSAVECommonBlock(Context, Loc, IDLoc, II);
      }
      else if(ExpectAndConsume(tok::identifier))
        Stmt = Actions.ActOnSAVE(Context, Loc, IDLoc, II, nullptr);
      else {
        ListParsedOk = false;
        break;
      }

      if(Stmt.isUsable())
        StmtList.push_back(Stmt.get());
    }
  }

  if(ListParsedOk) ExpectStatementEnd();
  else SkipUntilNextStatement();

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

} // end namespace flang
