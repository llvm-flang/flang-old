//===-- FixedForm.cpp - Fortran Fixed-form Parsing Utilities ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/FixedForm.h"
#include "flang/Parse/Parser.h"

namespace flang {
namespace fixedForm {

KeywordFilter::KeywordFilter(tok::TokenKind K1, tok::TokenKind K2,
                             tok::TokenKind K3) {
  SmallArray[0] = K1; SmallArray[1] = K2;
  SmallArray[2] = K3;
  Keywords = ArrayRef<tok::TokenKind>(SmallArray, K3 == tok::unknown? 2 : 3);
}

KeywordMatcher::KeywordMatcher(ArrayRef<KeywordFilter> Filters) {
  for(auto Filter : Filters) {
    for(auto Keyword : Filter.getKeywords())
      Register(Keyword);
  }
}

void KeywordMatcher::operator=(ArrayRef<KeywordFilter> Filters) {
  for(auto Filter : Filters) {
    for(auto Keyword : Filter.getKeywords())
      Register(Keyword);
  }
}

void KeywordMatcher::Register(tok::TokenKind Keyword) {
  auto Identifier = getTokenName(Keyword);
  std::string Name(Identifier);
  for (size_t I = 0, E = Name.size(); I != E; ++I)
    Name[I] = ::tolower(Name[I]);
  Keywords.insert(Name);
}

bool KeywordMatcher::Matches(StringRef Identifier) const {
  std::string Name(Identifier);
  for (size_t I = 0, E = Name.size(); I != E; ++I)
    Name[I] = ::tolower(Name[I]);
 return Keywords.find(Name) != Keywords.end();
}

static const tok::TokenKind AmbiguousExecKeywords[] = {
  // ASSIGN10TOI
  tok::kw_ASSIGN,
  // DOI=1,10
  tok::kw_DO,
  tok::kw_DOWHILE,
  // ENDDOconstructname
  tok::kw_ENDDO,
  // ELSEconstructname
  tok::kw_ELSE,
  // ELSEIF
  tok::kw_ELSEIF,
  // ENDIFconstructname
  tok::kw_ENDIF,
  // ENDSELECTconstructname
  tok::kw_ENDSELECT,
  // GOTOI
  tok::kw_GOTO,
  // CALLfoo
  tok::kw_CALL,
  // STOP1
  tok::kw_STOP,
  // ENTRYwhat
  tok::kw_ENTRY,
  // RETURN1
  tok::kw_RETURN,
  // CYCLE/EXITconstructname
  tok::kw_CYCLE,
  tok::kw_EXIT,
  // PRINTfmt
  tok::kw_PRINT,
  // READfmt
  tok::kw_READ,

  tok::kw_DATA
};

AmbiguousExecutableStatements::AmbiguousExecutableStatements()
  : KeywordFilter(AmbiguousExecKeywords) {}

static const tok::TokenKind AmbiguousEndKeywords[] = {
  // END<...>name
  tok::kw_END,
  tok::kw_ENDPROGRAM,
  tok::kw_ENDSUBROUTINE,
  tok::kw_ENDFUNCTION
};

AmbiguousEndStatements::AmbiguousEndStatements()
  : KeywordFilter(AmbiguousEndKeywords) {}

static const tok::TokenKind AmbiguousTypeKeywords[] = {
  // INTEGERvar
  tok::kw_INTEGER,
  tok::kw_REAL,
  tok::kw_COMPLEX,
  tok::kw_DOUBLEPRECISION,
  tok::kw_DOUBLECOMPLEX,
  tok::kw_LOGICAL,
  tok::kw_CHARACTER
};

AmbiguousTypeStatements::AmbiguousTypeStatements()
  : KeywordFilter(AmbiguousTypeKeywords) {}

static const tok::TokenKind AmbiguousSpecKeywords[] = {
  // IMPLICITREAL
  tok::kw_IMPLICIT,
  // DIMENSIONI(10)
  tok::kw_DIMENSION,
  // EXTERNALfoo
  tok::kw_EXTERNAL,
  // INTRINSICfoo
  tok::kw_INTRINSIC,
  // COMMONi
  tok::kw_COMMON,
  // DATAa/1/
  tok::kw_DATA,
  // SAVEi
  tok::kw_SAVE
};

AmbiguousSpecificationStatements::AmbiguousSpecificationStatements()
  : KeywordFilter(AmbiguousSpecKeywords) {}

static const tok::TokenKind AmbiguousTopLvlDeclKeywords[] = {
  // PROGRAMname
  tok::kw_PROGRAM,
  tok::kw_SUBROUTINE,
  tok::kw_FUNCTION,
  // RECURSIVEfunctionFOO
  tok::kw_RECURSIVE
};

AmbiguousTopLevelDeclarationStatements::AmbiguousTopLevelDeclarationStatements()
  : KeywordFilter(AmbiguousTopLvlDeclKeywords){}

CommonAmbiguities::CommonAmbiguities() {
  {
    const KeywordFilter Filters[] = {
      AmbiguousTypeStatements(),
      KeywordFilter(tok::kw_FUNCTION, tok::kw_SUBROUTINE)
    };
    MatcherForKeywordsAfterRECURSIVE = llvm::makeArrayRef(Filters);
  }
  {
    const KeywordFilter Filters[] = {
      AmbiguousExecutableStatements(),
      KeywordFilter(tok::kw_THEN)
    };
    MatcherForKeywordsAfterIF = llvm::makeArrayRef(Filters);
  }
  {
    const KeywordFilter Filters[] = {
      AmbiguousTopLevelDeclarationStatements(),
      AmbiguousTypeStatements(),
      AmbiguousSpecificationStatements(),
      AmbiguousExecutableStatements(),
      AmbiguousEndStatements()
    };
    MatcherForTopLevel = llvm::makeArrayRef(Filters);
  }
  {
    const KeywordFilter Filters[] = {
      AmbiguousTypeStatements(),
      AmbiguousSpecificationStatements(),
      AmbiguousExecutableStatements(),
      AmbiguousEndStatements()
    };
    MatcherForSpecStmts = llvm::makeArrayRef(Filters);
  }
  {
    const KeywordFilter Filters[] = {
      AmbiguousExecutableStatements(),
      AmbiguousEndStatements()
    };
    MatcherForExecStmts = llvm::makeArrayRef(Filters);
  }
}

} // end namespace fixedForm

void Parser::ReLexAmbiguousIdentifier(const fixedForm::KeywordMatcher &Matcher) {
  assert(Features.FixedForm);
  if(Matcher.Matches(Tok.getIdentifierInfo()->getName()))
    return;
  NextTok.setKind(tok::unknown);
  TheLexer.LexFixedFormIdentifierMatchLongestKeyword(Matcher, Tok);
  ClassifyToken(Tok);
}

bool Parser::ExpectAndConsumeFixedFormAmbiguous(tok::TokenKind ExpectedTok, unsigned Diag,
                                                const char *DiagMsg) {
  if(Features.FixedForm) {
    if(Tok.isAtStartOfStatement())
      return ExpectAndConsume(ExpectedTok, Diag, DiagMsg);
    ReLexAmbiguousIdentifier(fixedForm::KeywordMatcher(fixedForm::KeywordFilter(ExpectedTok)));
  }
  return ExpectAndConsume(ExpectedTok, Diag, DiagMsg);
}

void Parser::StartStatementReparse(SourceLocation Where) {
  TheLexer.ReLexStatement(Where.isValid()? Where : LocFirstStmtToken);
  Tok.startToken();
  NextTok.setKind(tok::unknown);
  Lex();
}

void Parser::LookForTopLevelStmtKeyword() {
  bool AtStartOfStatement = StmtLabel? false : true;
  if(Features.FixedForm) {
    if(!AtStartOfStatement && Tok.isAtStartOfStatement()) return;
    ReLexAmbiguousIdentifier(FixedFormAmbiguities.getMatcherForTopLevelKeywords());
  }
}

void Parser::LookForSpecificationStmtKeyword() {
  bool AtStartOfStatement = StmtLabel? false : true;
  if(Features.FixedForm) {
    if(!AtStartOfStatement && Tok.isAtStartOfStatement()) return;
    ReLexAmbiguousIdentifier(FixedFormAmbiguities.getMatcherForSpecificationStmtKeywords());
  }
}

void Parser::LookForExecutableStmtKeyword(bool AtStartOfStatement) {
  if(Features.FixedForm) {
    if(!AtStartOfStatement && Tok.isAtStartOfStatement()) return;
    ReLexAmbiguousIdentifier(FixedFormAmbiguities.getMatcherForExecutableStmtKeywords());
  }
}

} // end namespace flang
