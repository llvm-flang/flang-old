//===-- ParserFormat.cpp - Fortran FORMAT Parser -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Fortran FORMAT statement and specification parsing.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Ownership.h"
#include "flang/Sema/Sema.h"

namespace flang {

/// ParseFORMATStmt - Parse the FORMAT statement.
///
///   [R1001]:
///     format-stmt :=
///         FORMAT format-specification
Parser::StmtResult Parser::ParseFORMATStmt() {
  auto Loc = Tok.getLocation();
  LexFORMATTokens = true;
  Lex();


  auto Result =ParseFORMATSpec(Loc);
  LexFORMATTokens = false;

  return Result;
}

/// ParseFORMATSpec - Parses the FORMAT specification.
///   [R1002]:
///     format-specification :=
///         ( [ format-items ] )
///      or ( [ format-items, ] unlimited-format-item )
Parser::StmtResult Parser::ParseFORMATSpec(SourceLocation Loc) {
  if (!ExpectAndConsume(tok::l_paren))
    return StmtError();

  auto Items = ParseFORMATItems(true);
  FormatItemResult UnlimitedItems;
  if(Items.isInvalid()) return StmtError();
  if(!Items.isUsable()) {
    assert(Tok.is(tok::star));
    // FIXME: Act on unlimited
    // Parse unlimited-format-item
    Lex(); // eat '*'
    if (!ExpectAndConsume(tok::l_paren))
      return StmtError();
    UnlimitedItems = ParseFORMATItems(false);
  }
  return Actions.ActOnFORMAT(Context, Loc,
                             Items, UnlimitedItems, StmtLabel);
}

/// ParseFormatItems - Parses the FORMAT items.
Parser::FormatItemResult Parser::ParseFORMATItems(bool IsOuter) {
  SmallVector<FormatItem*, 8> FormatList;
  auto Loc = Tok.getLocation();

  do {
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_format_expected_desc);
      return FormatItemResult(true);
    }
    if(IsOuter && Tok.is(tok::star))
      // Go back to the part to parse the unlimited-format-item
      return FormatItemResult();

    auto Item = ParseFORMATItem();
    if(Item.isInvalid()) {
      // Fixme: add error recovery
    }
    if(Item.isUsable()) FormatList.push_back(Item.take());
  } while(ConsumeIfPresent(tok::comma));

  if (!ExpectAndConsume(tok::r_paren))
    return FormatItemResult(true);

  return Actions.ActOnFORMATFormatItemList(Context, Loc,
                                           nullptr, FormatList);
}


/// A helper class to parse FORMAT descriptor
class FormatDescriptorParser : public FormatDescriptorLexer {
  ASTContext &Context;
  DiagnosticsEngine &Diag;

public:
  FormatDescriptorParser(ASTContext &C, DiagnosticsEngine &D,
           const Lexer &Lex, const Token &FD)
   : Context(C), Diag(D), FormatDescriptorLexer(Lex, FD) {
  }

  IntegerConstantExpr *ParseIntExpr(const char *DiagAfter = nullptr) {
    llvm::StringRef Str;
    auto Loc = getCurrentLoc();
    if(!LexIntIfPresent(Str)){
      if(DiagAfter)
        Diag.Report(Loc, diag::err_expected_int_literal_constant_after)
          << DiagAfter;
      else
        Diag.Report(Loc, diag::err_expected_int_literal_constant);
      return nullptr;
    }
    return IntegerConstantExpr::Create(Context, SourceRange(Loc,
                                       getCurrentLoc()), Str);
  }

  void MustBeDone() {
    if(!IsDone()) {
      auto Loc = getCurrentLoc();
      while(!IsDone()) ++Offset;
      Diag.Report(Loc, diag::err_format_desc_with_unparsed_end)
        << SourceRange(Loc, getCurrentLoc());
    }
  }
};

/// ParseFORMATItem - Parses the FORMAT item.
Parser::FormatItemResult Parser::ParseFORMATItem() {
  if(ConsumeIfPresent(tok::l_paren))
    return ParseFORMATItems();

  // char-string-edit-desc
  if(Tok.is(tok::char_literal_constant)) {
    return Actions.ActOnFORMATCharacterStringDesc(Context, Tok.getLocation(),
                                                  ParsePrimaryExpr());
  }

  if(Tok.is(tok::l_paren)) {
    //FIXME: add the repeat count into account
    return ParseFORMATItems();
  }
  if(Tok.is(tok::slash) || Tok.is(tok::colon)) {
    // FIXME: allow preint repeat count before slash.
    auto Loc = Tok.getLocation();
    auto Desc = Tok.getKind();
    Lex();
    return Actions.ActOnFORMATControlEditDesc(Context, Loc, Desc);
  }
  if(Tok.isNot(tok::format_descriptor)) {
    Diag.Report(getExpectedLoc(), diag::err_format_expected_desc);
    return FormatItemResult(true);
  }

  // the identifier token could be something like I2,
  // so we need to split it into parts
  FormatDescriptorParser FDParser(Context, Diag,
                                  TheLexer, Tok);

  auto Loc = Tok.getLocation();
  Lex();

  // possible pre integer
  // R for data-edit-desc or
  // n for X in position-edit-desc
  IntegerConstantExpr *PreInt = nullptr;
  if(FDParser.IsIntPresent())
    PreInt = FDParser.ParseIntExpr();

  // descriptor identifier.
  llvm::StringRef DescriptorStr;
  auto DescriptorStrLoc = FDParser.getCurrentLoc();
  if(!FDParser.LexIdentIfPresent(DescriptorStr)) {
    Diag.Report(DescriptorStrLoc, diag::err_format_expected_desc);
    return FormatItemResult(true);
  }
  auto DescIdent = Identifiers.lookupFormatSpec(DescriptorStr);
  if(!DescIdent) {
    Diag.Report(DescriptorStrLoc, diag::err_format_invalid_desc)
      << DescriptorStr
      << SourceRange(DescriptorStrLoc,FDParser.getCurrentLoc());
    return FormatItemResult(true);
  }

  // data-edit-desc or control-edit-desc
  auto Desc = DescIdent->getTokenID();
  IntegerConstantExpr *W = nullptr,
                      *MD = nullptr,
                      *E = nullptr;
  switch(Desc) {
  // data-edit-desc
  case tok::fs_I: case tok::fs_B:
  case tok::fs_O: case tok::fs_Z:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    if(!W) break;
    if(FDParser.LexCharIfPresent('.')) {
      MD = FDParser.ParseIntExpr(".");
      if(!MD) break;
    }
    FDParser.MustBeDone();
    return Actions.ActOnFORMATIntegerDataEditDesc(Context, Loc, Desc, PreInt,
                                                  W, MD);

  case tok::fs_F:
  case tok::fs_E: case tok::fs_EN: case tok::fs_ES: case tok::fs_G:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    if(!W) break;
    if(!FDParser.LexCharIfPresent('.')) {
      if(Desc == tok::fs_G) {
        FDParser.MustBeDone();
        return Actions.ActOnFORMATRealDataEditDesc(Context, Loc, Desc, PreInt,
                                                   W, MD, E);

      }
      Diag.Report(FDParser.getCurrentLoc(), diag::err_expected_dot);
      break;
    }
    MD = FDParser.ParseIntExpr(".");
    if(!MD) break;
    if(Desc != tok::fs_F &&
       (FDParser.LexCharIfPresent('E') ||
        FDParser.LexCharIfPresent('e'))) {
      E = FDParser.ParseIntExpr("E");
    }
    FDParser.MustBeDone();
    return Actions.ActOnFORMATRealDataEditDesc(Context, Loc, Desc, PreInt,
                                               W, MD, E);


  case tok::fs_L:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    FDParser.MustBeDone();
    return Actions.ActOnFORMATLogicalDataEditDesc(Context, Loc, Desc,
                                                  PreInt, W);

  case tok::fs_A:
    if(!FDParser.IsDone())
      W = FDParser.ParseIntExpr();
    FDParser.MustBeDone();
    return Actions.ActOnFORMATCharacterDataEditDesc(Context, Loc, Desc,
                                                    PreInt, W);

  // position-edit-desc
  case tok::fs_T: case tok::fs_TL: case tok::fs_TR:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    if(!W) break;
    FDParser.MustBeDone();
    return Actions.ActOnFORMATPositionEditDesc(Context, Loc, Desc, W);

  case tok::fs_X:
    if(!PreInt) {
      Diag.Report(DescriptorStrLoc, diag::err_expected_int_literal_constant_before)
        << "X";
      break;
    }
    FDParser.MustBeDone();
    return Actions.ActOnFORMATPositionEditDesc(Context, Loc, Desc, PreInt);

  case tok::fs_SS: case tok::fs_SP: case tok::fs_S:
  case tok::fs_BN: case tok::fs_BZ:
  case tok::fs_RU: case tok::fs_RD: case tok::fs_RZ:
  case tok::fs_RN: case tok::fs_RC: case tok::fs_RP:
  case tok::fs_DC: case tok::fs_DP:
    if(PreInt) {
      // FIXME: proper error.
      Diag.Report(Loc, diag::err_expected_int_literal_constant);
    }
    FDParser.MustBeDone();
    return Actions.ActOnFORMATControlEditDesc(Context, Loc, Desc);

  // FIXME: add the rest..
  default:
    Diag.Report(DescriptorStrLoc, diag::err_format_invalid_desc)
      << DescriptorStr;
    break;
  }
  return FormatItemResult(true);
}

} // end namespace flang
