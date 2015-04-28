//===-- Parser.cpp - Fortran Parser Interface -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran parser interface.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/FixedForm.h"
#include "flang/Parse/LexDiagnostic.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Sema.h"
#include "flang/Sema/Scope.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"

namespace flang {

/// print - If a crash happens while the parser is active, print out a line
/// indicating what the current token is.
void PrettyStackTraceParserEntry::print(llvm::raw_ostream &OS) const {
  const Token &Tok = FP.getCurToken();
  if (Tok.is(tok::eof)) {
    OS << "<eof> parser at end of file\n";
    return;
  }

  if (!Tok.getLocation().isValid()) {
    OS << "<unknown> parser at unknown location\n";
    return;
  }

  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  FP.getLexer().getSpelling(Tok, Spelling);
  std::string Name = Tok.CleanLiteral(Spelling);
  FP.getLexer().getSourceManager()
    .PrintMessage(Tok.getLocation(), llvm::SourceMgr::DK_Error,
                  "current parser token '" + Name + "'");
}

//===----------------------------------------------------------------------===//
//                            Fortran Parsing
//===----------------------------------------------------------------------===//

Parser::Parser(llvm::SourceMgr &SM, const LangOptions &Opts, DiagnosticsEngine  &D,
               Sema &actions)
  : TheLexer(SM, Opts, D), Features(Opts), CrashInfo(*this), SrcMgr(SM),
    Context(actions.Context), Diag(D), Actions(actions),
    Identifiers(Opts), DontResolveIdentifiers(false),
    DontResolveIdentifiersInSubExpressions(false),
    LexFORMATTokens(false), StmtConstructName(SourceLocation(),nullptr) {
  CurBufferIndex.push_back(SrcMgr.getMainFileID());
  getLexer().setBuffer(SrcMgr.getMemoryBuffer(CurBufferIndex.back()));
  Tok.startToken();
  NextTok.startToken();

  PrevTokLocEnd = Tok.getLocation();
  ParenCount = ParenSlashCount = BraceCount = BracketCount = 0;
  PrevStmtWasSelectCase = false;
}

bool Parser::EnterIncludeFile(const std::string &Filename) {
  std::string IncludedFile;
  int NewBuf = SrcMgr.AddIncludeFile(Filename, getLexer().getLoc(),
                                     IncludedFile);
  if (NewBuf == -1)
    return true;

  CurBufferIndex.push_back(NewBuf);
  LexerBufferContext.push_back(getLexer().getBufferPtr());
  getLexer().setBuffer(SrcMgr.getMemoryBuffer(CurBufferIndex.back()));
  Diag.getClient()->BeginSourceFile(Features, &TheLexer);
  return false;
}

bool Parser::LeaveIncludeFile() {
  if(CurBufferIndex.size() == 1) return true;//No files included.
  Diag.getClient()->EndSourceFile();
  CurBufferIndex.pop_back();
  getLexer().setBuffer(SrcMgr.getMemoryBuffer(CurBufferIndex.back()),
                       LexerBufferContext.back());
  LexerBufferContext.pop_back();
  return false;
}

SourceLocation Parser::getExpectedLoc() const {
  if(Tok.isAtStartOfStatement())
    return PrevTokLocEnd;
  return Tok.getLocation();
}

SourceRange Parser::getTokenRange(SourceLocation Loc) const {
  Lexer L(TheLexer, Loc);
  Token T;
  L.Lex(T); L.Lex(T);
  return SourceRange(Loc, T.getLocation());
}

// FIXME:
SourceRange Parser::getTokenRange() const {
  return SourceRange(Tok.getLocation(), SourceLocation::getFromPointer(Tok.getLocation().getPointer() +
                                                                       Tok.getLength()));
}

bool Parser::IsNextToken(tok::TokenKind TokKind) {
  if (NextTok.is(tok::unknown))
    TheLexer.Lex(NextTok, true);
  return NextTok.is(TokKind) && !NextTok.isAtStartOfStatement();
}

/// Lex - Get the next token.
void Parser::Lex() {
  /// Reset paren count
  if(Tok.isAtStartOfStatement()) {
    ParenCount = 0;
    ParenSlashCount = 0;
  }

  PrevTokLocation = Tok.getLocation();
  PrevTokLocEnd = getMaxLocationOfCurrentToken();
  if(LexFORMATTokens) {
    if (NextTok.isNot(tok::unknown))
      Tok = NextTok;
    else
      TheLexer.LexFORMATToken(Tok);
    NextTok.setKind(tok::unknown);

    if (!Tok.is(tok::eof))
      return;
    else
      LexFORMATTokens = false;
  }

  if (NextTok.isNot(tok::unknown)) {
    Tok = NextTok;
    NextTok.setKind(tok::unknown);
  } else {
    TheLexer.Lex(Tok);
    ClassifyToken(Tok);
  }

  if(Tok.isAtStartOfStatement())
    LocFirstStmtToken = Tok.getLocation();

  if (Tok.is(tok::eof)){
    if(!LeaveIncludeFile()){
      NextTok.setKind(tok::unknown);
      Lex();
    }
    return;
  }

  // No need to merge when identifiers can already have
  // spaces in between
  if(Features.FixedForm)
    return;

  TheLexer.Lex(NextTok);
  ClassifyToken(NextTok);

#define MERGE_TOKENS(A, B)                      \
  if (!NextTok.isAtStartOfStatement() && NextTok.is(tok::kw_ ## B)) {              \
    Tok.setKind(tok::kw_ ## A ## B);            \
    Tok.setLength((NextTok.getLocation().getPointer() + \
                  NextTok.getLength()) - (Tok.getLocation().getPointer())); \
    break;                                      \
  }                                             \

  // [3.3.1]p4
  switch (Tok.getKind()) {
  default: return;
  case tok::kw_INCLUDE:{
    bool hadErrors = ParseInclude();
    Tok = NextTok;
    NextTok.setKind(tok::unknown);
    if(hadErrors)
      SkipUntilNextStatement();
    else Lex();
    return;
  }
  case tok::kw_BLOCK:
    MERGE_TOKENS(BLOCK, DATA);
    return;
  case tok::kw_ELSE:
    MERGE_TOKENS(ELSE, IF);
    MERGE_TOKENS(ELSE, WHERE);
    return;
  case tok::kw_END: {
    MERGE_TOKENS(END, IF);
    MERGE_TOKENS(END, DO);
    MERGE_TOKENS(END, FUNCTION);
    MERGE_TOKENS(END, SUBROUTINE);
    MERGE_TOKENS(END, FORALL);
    MERGE_TOKENS(END, WHERE);
    MERGE_TOKENS(END, ENUM);
    MERGE_TOKENS(END, SELECT);
    MERGE_TOKENS(END, TYPE);
    MERGE_TOKENS(END, MODULE);
    MERGE_TOKENS(END, PROGRAM);
    MERGE_TOKENS(END, ASSOCIATE);
    MERGE_TOKENS(END, FILE);
    MERGE_TOKENS(END, INTERFACE);
    MERGE_TOKENS(END, BLOCKDATA);

    if (NextTok.is(tok::kw_BLOCK)) {
      Tok = NextTok;
      TheLexer.Lex(NextTok);
      ClassifyToken(NextTok);

      if (!NextTok.is(tok::kw_DATA)) {
        Diag.ReportError(NextTok.getLocation(),
                         "expected 'DATA' after 'BLOCK' keyword");
        return;
      }

      Tok.setKind(tok::kw_ENDBLOCKDATA);
      break;
    }

    return;
  }
  case tok::kw_ENDBLOCK:
    MERGE_TOKENS(ENDBLOCK, DATA);
    return;
  case tok::kw_DO:
    MERGE_TOKENS(DO, WHILE);
    return;
  case tok::kw_GO:
    MERGE_TOKENS(GO, TO);
    return;
  case tok::kw_SELECT:
    MERGE_TOKENS(SELECT, CASE);
    MERGE_TOKENS(SELECT, TYPE);
    return;
  case tok::kw_IN:
    MERGE_TOKENS(IN, OUT);
    return;
  case tok::kw_DOUBLE:
    MERGE_TOKENS(DOUBLE, PRECISION);
    MERGE_TOKENS(DOUBLE, COMPLEX);
    return;
  }

  if (NextTok.is(tok::eof)) return;

  TheLexer.Lex(NextTok);
  ClassifyToken(NextTok);
}

void Parser::ClassifyToken(Token &T) {
  if (T.isNot(tok::identifier))
    return;

  // Set the identifier info for this token.
  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  TheLexer.getSpelling(T, Spelling);
  std::string NameStr = T.CleanLiteral(Spelling);

  // We assume that the "common case" is that if an identifier is also a
  // keyword, it will most likely be used as a keyword. I.e., most programs are
  // sane, and won't use keywords for variable names. We mark it as a keyword
  // for ease in parsing. But it's weak and can change into an identifier or
  // builtin depending upon the context.
  if (IdentifierInfo *KW = Identifiers.lookupKeyword(NameStr)) {
    T.setIdentifierInfo(KW);
    T.setKind(KW->getTokenID());
  } else {
    IdentifierInfo *II = getIdentifierInfo(NameStr);
    T.setIdentifierInfo(II);
    T.setKind(II->getTokenID());
  }
}

/// CleanLiteral - Cleans up a literal if it needs cleaning. It removes the
/// continuation contexts and comments. Cleaning a dirty literal is SLOW!
void Parser::CleanLiteral(Token T, std::string &NameStr) {
  assert(T.isLiteral() && "Trying to clean a non-literal!");
  if (!T.needsCleaning()) {
    // This should be the common case.
    if(T.isNot(tok::char_literal_constant)) {
      NameStr = llvm::StringRef(T.getLiteralData(),
                                T.getLength()).str();
    } else {
      NameStr = llvm::StringRef(T.getLiteralData()+1,
                                T.getLength()-2).str();
    }
    return;
  }

  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  TheLexer.getSpelling(T, Spelling);
  NameStr = T.CleanLiteral(Spelling);
  if(T.is(tok::char_literal_constant))
    NameStr = std::string(NameStr,1,NameStr.length()-2);
}

/// \brief Returns true if the check flag is set,
/// and tok is at start of a new statement
static inline bool CheckIsAtStartOfStatement(const Token &Tok, bool DoCheck) {
  if(DoCheck)
    return Tok.isAtStartOfStatement();
  return false;
}

bool Parser::IsPresent(tok::TokenKind TokKind, bool InSameStatement) {
  if(!CheckIsAtStartOfStatement(Tok, InSameStatement) &&
     (TokKind == tok::identifier? isTokenIdentifier() : Tok.is(TokKind)))
    return true;
  return false;
}

bool Parser::ConsumeIfPresent(tok::TokenKind OptionalTok, bool InSameStatement) {
  if(!CheckIsAtStartOfStatement(Tok, InSameStatement)  &&
     (OptionalTok == tok::identifier? isTokenIdentifier() : Tok.is(OptionalTok))) {
    ConsumeAnyToken();
    return true;
  }
  return false;
}

bool Parser::ExpectAndConsume(tok::TokenKind ExpectedTok, unsigned Diag,
                              const char *DiagMsg,
                              tok::TokenKind SkipToTok,
                              bool InSameStatement) {
  if(ConsumeIfPresent(ExpectedTok, InSameStatement))
    return true;

  if(Diag == 0) {
    switch(ExpectedTok) {
    case tok::l_paren:
      Diag = diag::err_expected_lparen;
      break;
    case tok::r_paren:
      Diag = diag::err_expected_rparen;
      break;
    case tok::l_parenslash:
      Diag = diag::err_expected_lparenslash;
      break;
    case tok::slashr_paren:
      Diag = diag::err_expected_slashrparen;
      break;
    case tok::equal:
      Diag = diag::err_expected_equal;
      break;
    case tok::comma:
      Diag = diag::err_expected_comma;
      break;
    case tok::colon:
      Diag = diag::err_expected_colon;
      break;
    case tok::coloncolon:
      Diag = diag::err_expected_coloncolon;
      break;
    case tok::slash:
      Diag = diag::err_expected_slash;
      break;
    case tok::identifier:
      Diag = diag::err_expected_ident;
      break;
    default:
      assert(false && "Couldn't select a default "
             "diagnostic for the given token type.");
    }
  }

  if(auto Spelling = tok::getTokenSimpleSpelling(ExpectedTok)) {
    // Show what code to insert to fix this problem.
    this->Diag.Report(getExpectedLoc(), Diag)
      << DiagMsg
      << FixItHint(getExpectedLocForFixIt(), Spelling);
  } else {
    this->Diag.Report(getExpectedLoc(), Diag)
      << DiagMsg;
  }

  if(SkipToTok != tok::unknown)
    SkipUntil(SkipToTok);
  return false;
}

bool Parser::SkipUntil(ArrayRef<tok::TokenKind> Toks, bool StopAtNextStatement,
                       bool DontConsume) {
  // We always want this function to skip at least one token if the first token
  // isn't T and if not at EOF.
  bool isFirstTokenSkipped = true;
  while(true) {
    if(CheckIsAtStartOfStatement(Tok, StopAtNextStatement))
      return false;

    // If we found one of the tokens, stop and return true.
    for (unsigned i = 0, NumToks = Toks.size(); i != NumToks; ++i) {
      if (Toks[i] == tok::identifier? isTokenIdentifier() :
                                      Tok.is(Toks[i])) {
        if (DontConsume) {
          // Noop, don't consume the token.
        } else {
          ConsumeAnyToken();
        }
        return true;
      }
    }

    switch(Tok.getKind()) {
    case tok::eof:
      // Ran out of tokens.
      return false;

    case tok::l_paren:
       // Recursively skip properly-nested parens.
      ConsumeParen();
      SkipUntil(tok::r_paren, StopAtNextStatement, false);
      break;
    case tok::l_parenslash:
      ConsumeParenSlash();
      SkipUntil(tok::slashr_paren, StopAtNextStatement, false);
      break;

    // Okay, we found a ']' or '}' or ')', which we think should be balanced.
    // Since the user wasn't looking for this token (if they were, it would
    // already be handled), this isn't balanced.  If there is a LHS token at a
    // higher level, we will assume that this matches the unbalanced token
    // and return it.  Otherwise, this is a spurious RHS token, which we skip.
    case tok::r_paren:
      if (ParenCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeParen();
      break;
    case tok::slashr_paren:
      if (ParenSlashCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeParenSlash();
      break;
    default:
      ConsumeToken();
      break;
    }
    isFirstTokenSkipped = false;
  }
}

bool Parser::ExpectStatementEnd() {
  if(!Tok.isAtStartOfStatement()) {
    auto Loc = getExpectedLoc();
    auto Start = Tok.getLocation();
    SkipUntilNextStatement();
    auto End = getExpectedLoc();
    Diag.Report(Loc, diag::err_expected_end_statement_at_end_of_stmt)
      << SourceRange(Start, End);
    return false;
  }
  return true;
}

bool Parser::SkipUntilNextStatement() {
  while(!Tok.isAtStartOfStatement())
    ConsumeAnyToken();
  return true;
}

/// ParseInclude - parses the include statement and loads the included file.
bool Parser::ParseInclude() {
  if(NextTok.isAtStartOfStatement() ||
     NextTok.isNot(tok::char_literal_constant)){
    Diag.Report(NextTok.getLocation(), diag::err_pp_expects_filename);
    return true;
  }
  std::string LiteralString;
  CleanLiteral(NextTok,LiteralString);
  if(!LiteralString.length()) {
    Diag.Report(NextTok.getLocation(), diag::err_pp_empty_filename);
    return true;
  }
  if(EnterIncludeFile(LiteralString) == true){
    Diag.Report(NextTok.getLocation(), diag::err_pp_file_not_found) <<
                LiteralString;
    return true;
  }
  return false;
}

/// ParseStatementLabel - Parse the statement label token. If the current token
/// isn't a statement label, then set the StmtLabelTok's kind to "unknown".
void Parser::ParseStatementLabel() {
  if (Tok.isNot(tok::statement_label)) {
    if (Tok.isAtStartOfStatement())
      StmtLabel = 0;
    return;
  }

  std::string NumStr;
  CleanLiteral(Tok, NumStr);
  StmtLabel = IntegerConstantExpr::Create(Context, getTokenRange(),
                                          NumStr);
  ConsumeToken();
}

/// ParseStatementLabelReference - Parses a statement label reference token.
ExprResult Parser::ParseStatementLabelReference(bool Consume) {
  if(Tok.isNot(tok::int_literal_constant))
    return ExprError();

  std::string NumStr;
  CleanLiteral(Tok, NumStr);
  auto Result = IntegerConstantExpr::Create(Context, getTokenRange(),
                                            NumStr);
  if(Consume) ConsumeToken();
  return Result;
}

/// ParseConstructNameLabel - Parses an optional construct-name ':' label.
/// If the construct name isn't there, then set the ConstructName to null.
///
/// FIXME: check for do, if, select case after
void Parser::ParseConstructNameLabel() {
  if(Tok.is(tok::identifier)) {
    if(IsNextToken(tok::colon)) {
      auto ID = Tok.getIdentifierInfo();
      auto Loc = ConsumeToken();
      StmtConstructName = ConstructName(Loc, ID);
      ConsumeToken(); // eat the ':'
      return;
    }
  }
  StmtConstructName.IDInfo = nullptr;
}

/// ParseTrailingConstructName - Parses an optional trailing construct-name identifier.
/// If the construct name isn't there, then set the ConstructName to null.
void Parser::ParseTrailingConstructName() {
  if(IsPresent(tok::identifier)) {
    auto ID = Tok.getIdentifierInfo();
    auto Loc = ConsumeToken();
    StmtConstructName = ConstructName(Loc, ID);
  } else
    StmtConstructName = ConstructName(getExpectedLoc(), nullptr);
}

// Assumed syntax rules
//
//   [R101] xyz-list        :=  xyz [, xyz] ...
//   [R102] xyz-name        :=  name
//   [R103] scalar-xyz      :=  xyz
//
//   [C101] (R103) scalar-xyz shall be scalar.

/// ParseProgramUnits - Main entry point to the parser. Parses the current
/// source.
bool Parser::ParseProgramUnits() {
  TranslationUnitScope TuScope;
  Actions.ActOnTranslationUnit(TuScope);

  // Prime the lexer.
  Lex();
  Tok.setFlag(Token::StartOfStatement);

  while (!ParseProgramUnit())
    /* Parse them all */;

  Actions.ActOnEndTranslationUnit();
  return false;
}

/// ParseProgramUnit - Parse a program unit.
///
///   [R202]:
///     program-unit :=
///         main-program
///      or external-subprogram
///      or module
///      or block-data
bool Parser::ParseProgramUnit() {
  if (Tok.is(tok::eof))
    return true;

  ParseStatementLabel();
  LookForTopLevelStmtKeyword();
  if (IsNextToken(tok::equal))
    return ParseMainProgram();

  // FIXME: These calls should return something proper.
  switch (Tok.getKind()) {
  default:
    ParseMainProgram();
    break;

  case tok::kw_REAL:
  case tok::kw_INTEGER:
  case tok::kw_COMPLEX:
  case tok::kw_CHARACTER:
  case tok::kw_LOGICAL:
  case tok::kw_DOUBLEPRECISION:
  case tok::kw_DOUBLECOMPLEX:
    if(ParseTypedExternalSubprogram(FunctionDecl::NoAttributes))
      ParseMainProgram();
    break;
  case tok::kw_RECURSIVE:
    ParseRecursiveExternalSubprogram();
    break;
  case tok::kw_FUNCTION:
  case tok::kw_SUBROUTINE:
    ParseExternalSubprogram();
    break;

  case tok::kw_MODULE:
    ParseModule();
    break;

  case tok::kw_BLOCKDATA:
    ParseBlockData();
    break;
  }

  return false;
}

/// ParseMainProgram - Parse the main program.
///
///   [R1101]:
///     main-program :=
///         [program-stmt]
///           [specification-part]
///           [execution-part]
///           [internal-subprogram-part]
///           end-program-stmt
bool Parser::ParseMainProgram() {
  // If the PROGRAM statement didn't have an identifier, pretend like it did for
  // the time being.
  StmtResult ProgStmt;
  if (Tok.is(tok::kw_PROGRAM)) {
    ProgStmt = ParsePROGRAMStmt();
    //Body.push_back(ProgStmt);
  }

  // If the PROGRAM statement has an identifier, pass it on to the main program
  // action.
  const IdentifierInfo *IDInfo = 0;
  SourceLocation NameLoc;
  if (ProgStmt.isUsable()) {
    ProgramStmt *PS = ProgStmt.takeAs<ProgramStmt>();
    IDInfo = PS->getProgramName();
    NameLoc = PS->getNameLocation();
  }

  MainProgramScope Scope;
  Actions.ActOnMainProgram(Context, Scope, IDInfo, NameLoc);

  ParseExecutableSubprogramBody(tok::kw_ENDPROGRAM);
  auto EndLoc = Tok.getLocation();
  auto EndProgStmt = ParseENDStmt(tok::kw_ENDPROGRAM);
  if(EndProgStmt.isUsable())
    EndLoc = EndProgStmt.get()->getLocation();

  Actions.ActOnEndMainProgram(EndLoc);

  return EndProgStmt.isInvalid();
}

/// ParseENDStmt - Parse the END PROGRAM/FUNCTION/SUBROUTINE statement.
///
///   [R1103]:
///     end-program-stmt :=
///         END [ PROGRAM/FUNCTION/SUBROUTINE [ program-name ] ]
Parser::StmtResult Parser::ParseENDStmt(tok::TokenKind EndKw) {
  ParseStatementLabel();
  if(Tok.isNot(tok::kw_END) && Tok.isNot(EndKw)) {
    const char *Expected = "";
    const char *Given = "";
    switch(EndKw) {
    case tok::kw_ENDPROGRAM:
      Expected = "end program"; Given = "program"; break;
    case tok::kw_ENDFUNCTION:
      Expected = "end function"; Given = "function"; break;
    case tok::kw_ENDSUBROUTINE:
      Expected = "end subroutine"; Given = "subroutine"; break;
    }
    Diag.Report(Tok.getLocation(), diag::err_expected_kw)
      << Expected;
    Diag.Report(cast<NamedDecl>(Actions.CurContext)->getLocation(), diag::note_matching)
      << Given;
    if(Tok.isAtStartOfStatement()) ConsumeToken();
    SkipUntilNextStatement();
    return StmtError();
  }

  ConstructPartStmt::ConstructStmtClass Kind;
  switch(Tok.getKind()) {
  case tok::kw_ENDPROGRAM:
    Kind = ConstructPartStmt::EndProgramStmtClass; break;
  case tok::kw_ENDFUNCTION:
    Kind = ConstructPartStmt::EndFunctionStmtClass; break;
  case tok::kw_ENDSUBROUTINE:
    Kind = ConstructPartStmt::EndSubroutineStmtClass; break;
  default:
    Kind = ConstructPartStmt::EndStmtClass; break;
  }
  auto Loc = ConsumeToken();
  const IdentifierInfo *IDInfo  = nullptr;
  auto IDLoc = Loc;
  if(IsPresent(tok::identifier)) {
    IDInfo = Tok.getIdentifierInfo();
    IDLoc = ConsumeToken();
  }
  ExpectStatementEnd();

  return Actions.ActOnEND(Context, Loc, Kind, IDLoc, IDInfo, StmtLabel);
}

bool Parser::ParseExecutableSubprogramBody(tok::TokenKind EndKw) {
  // FIXME: Check for the specific keywords and not just absence of END or
  //        ENDPROGRAM.
  ParseStatementLabel();
  if (Tok.isNot(tok::kw_END) && Tok.isNot(EndKw))
    ParseSpecificationPart();

  // Apply specification statements.
  Actions.ActOnSpecificationPart();

  ParseStatementLabel();
  ParseExecutionPart();
  return false;
}

/// ParseSpecificationPart - Parse the specification part.
///
///   [R204]:
///     specification-part :=
///        [use-stmt] ...
///          [import-stmt] ...
///          [implicit-part] ...
///          [declaration-construct] ...
bool Parser::ParseSpecificationPart() {
  bool HasErrors = false;
  LookForSpecificationStmtKeyword();
  while (Tok.is(tok::kw_USE)) {
    StmtResult S = ParseUSEStmt();
    if (S.isUsable()) {
      Actions.getCurrentBody()->Append(S.take());
    } else if (S.isInvalid()) {
      SkipUntilNextStatement();
      HasErrors = true;
    } else {
      break;
    }

    ParseStatementLabel();
    LookForSpecificationStmtKeyword();
  }

  while (Tok.is(tok::kw_IMPORT)) {
    StmtResult S = ParseIMPORTStmt();
    if (S.isUsable()) {
      Actions.getCurrentBody()->Append(S.take());
    } else if (S.isInvalid()) {
      SkipUntilNextStatement();
      HasErrors = true;
    } else {
      break;
    }

    ParseStatementLabel();
    LookForSpecificationStmtKeyword();
  }

  if (ParseImplicitPartList())
    HasErrors = true;


  if (ParseDeclarationConstructList()) {
    SkipUntilNextStatement();
    HasErrors = true;
  }

  return HasErrors;
}

/// ParseExternalSubprogram - Parse an external subprogram.
///
///   [R203]:
///     external-subprogram :=
///         function-subprogram
///      or subroutine-subprogram
///
///   [R1231]:
///     subroutine-subprogram :=
///         subroutine-stmt
///           [specification-part]
///           [execution-part]
///           [internal-subprogram-part]
///           end-subroutine-stmt
///
///   [R1223]:
///     function-subprogram :=
///         function-stmt
///           [specification-part]
///           [execution-part]
///           [internal-subprogram-part]
///           end-function-stmt
bool Parser::ParseExternalSubprogram() {
  DeclSpec ReturnType;
  return ParseExternalSubprogram(ReturnType, FunctionDecl::NoAttributes);
}

bool Parser::ParseTypedExternalSubprogram(int Attr) {
  auto ReparseLoc = LocFirstStmtToken;
  DeclSpec ReturnType;
  ParseDeclarationTypeSpec(ReturnType);
  bool IsRecursive = (Attr & FunctionDecl::Recursive) != 0;

  if(Tok.isAtStartOfStatement())
    goto err;
  if(!IsRecursive) {
    if(Features.FixedForm)
      ReLexAmbiguousIdentifier(fixedForm::KeywordMatcher(fixedForm::KeywordFilter(tok::kw_RECURSIVE,
                                                                                  tok::kw_FUNCTION)));
    if(Tok.is(tok::kw_RECURSIVE)) {
      ConsumeToken();
      Attr |= FunctionDecl::Recursive;
    }
  }

  if(Tok.isAtStartOfStatement())
    goto err;
  if(Features.FixedForm)
    ReLexAmbiguousIdentifier(fixedForm::KeywordMatcher(fixedForm::KeywordFilter(tok::kw_FUNCTION)));
  if(Tok.is(tok::kw_FUNCTION)) {
    ParseExternalSubprogram(ReturnType, Attr);
    return false;
  }
err:
  if(IsRecursive) {
    Diag.Report(getExpectedLoc(), diag::err_expected_kw)
      << "function";
    SkipUntilNextStatement();
  } else StartStatementReparse(ReparseLoc);
  return true;
}

bool Parser::ParseRecursiveExternalSubprogram() {
  ConsumeToken();
  if(Tok.isAtStartOfStatement())
    goto err;

  if(Features.FixedForm)
    ReLexAmbiguousIdentifier(FixedFormAmbiguities.getMatcherForKeywordsAfterRECURSIVE());
  if(Tok.is(tok::kw_SUBROUTINE) ||
     Tok.is(tok::kw_FUNCTION)) {
    DeclSpec ReturnType;
    return ParseExternalSubprogram(ReturnType, FunctionDecl::Recursive);
  }

  if(Tok.is(tok::kw_INTEGER) || Tok.is(tok::kw_REAL) || Tok.is(tok::kw_COMPLEX) ||
     Tok.is(tok::kw_DOUBLEPRECISION) || Tok.is(tok::kw_DOUBLECOMPLEX) ||
     Tok.is(tok::kw_LOGICAL) || Tok.is(tok::kw_CHARACTER) ||
     Tok.is(tok::kw_TYPE))
    return ParseTypedExternalSubprogram(FunctionDecl::Recursive);

err:
  Diag.Report(getExpectedLoc(), diag::err_expected_after_recursive);
  SkipUntilNextStatement();
  return true;
}

bool Parser::ParseExternalSubprogram(DeclSpec &ReturnType, int Attr) {
  bool IsSubroutine = Tok.is(tok::kw_SUBROUTINE);
  ConsumeToken();

  auto IDLoc = Tok.getLocation();
  auto II = Tok.getIdentifierInfo();
  if(!ExpectAndConsume(tok::identifier))
    return true;

  SubProgramScope Scope;
  Actions.ActOnSubProgram(Context, Scope, IsSubroutine, IDLoc, II, ReturnType, Attr);
  SmallVector<VarDecl* ,8> ArgumentList;
  bool HadErrorsInDeclStmt = false;

  if(ConsumeIfPresent(tok::l_paren)) {
    // argument list
    if(!IsPresent(tok::r_paren) && !Tok.isAtStartOfStatement()) {
      do {
        if(IsSubroutine && IsPresent(tok::star)) {
          Actions.ActOnSubProgramStarArgument(Context, ConsumeToken());
          continue;
        }
        auto IDLoc = Tok.getLocation();
        auto IDInfo = Tok.getIdentifierInfo();
        if(!ExpectAndConsume(tok::identifier)) {
          HadErrorsInDeclStmt = true;
          break;
        }
        auto Arg = Actions.ActOnSubProgramArgument(Context, IDLoc, IDInfo);
        if(Arg)
          ArgumentList.push_back(Arg);
      } while(ConsumeIfPresent(tok::comma));
    }

    // closing ')'
    if(!HadErrorsInDeclStmt) {
      if(!ExpectAndConsume(tok::r_paren)) {
        HadErrorsInDeclStmt = true;
      }
    }
  } else if(!IsSubroutine) {
    Diag.Report(getExpectedLoc(), diag::err_expected_lparen)
      << FixItHint(getExpectedLocForFixIt(), "(");
    HadErrorsInDeclStmt = true;
  }

  Actions.ActOnSubProgramArgumentList(Context, ArgumentList);

  if(HadErrorsInDeclStmt)
    SkipUntilNextStatement();
  else if(!IsSubroutine) {
    if(IsPresent(tok::kw_RESULT)) {
      if(ParseRESULT())
        SkipUntilNextStatement();
    }
  }

  auto EndKw = IsSubroutine? tok::kw_ENDSUBROUTINE :
                             tok::kw_ENDFUNCTION;
  ParseExecutableSubprogramBody(EndKw);
  auto EndLoc = Tok.getLocation();
  auto EndStmt = ParseENDStmt(EndKw);
  if(EndStmt.isUsable())
    EndLoc = EndStmt.get()->getLocation();
  StmtLabel = nullptr;
  Actions.ActOnEndSubProgram(Context, EndLoc);

  return false;
}

bool Parser::ParseRESULT() {
  ConsumeToken();
  if(!ExpectAndConsume(tok::l_paren))
    return true;
  auto IDLoc = Tok.getLocation();
  auto ID = Tok.getIdentifierInfo();
  if(!ExpectAndConsume(tok::identifier))
    return true;
  Actions.ActOnRESULT(Context, IDLoc, ID);
  if(!ExpectAndConsume(tok::r_paren))
    return true;
  return false;
}

/// ParseStatementFunction - parse a statement function.
///
/// NB: ambiguity with array assignment I(X) = 1 isi handled
/// by the caller by checking with Sema.
StmtResult Parser::ParseStatementFunction() {
  auto ID = Tok.getIdentifierInfo();
  auto Loc = ConsumeToken();

  SmallVector<VarDecl* ,8> ArgumentList;
  Actions.ActOnStatementFunction(Context, Loc, ID);
  ExpectAndConsume(tok::l_paren);
  bool DontParseBody = false;
  if(!ConsumeIfPresent(tok::r_paren)) {
    do {
      auto ArgID = Tok.getIdentifierInfo();
      auto Loc = Tok.getLocation();
      if(!ExpectAndConsume(tok::identifier))
        break;
      auto Arg = Actions.ActOnStatementFunctionArgument(Context, Loc, ArgID);
      if(Arg)
        ArgumentList.push_back(Arg);
    } while(ConsumeIfPresent(tok::comma));
    if(!ExpectAndConsume(tok::r_paren,0,"",tok::r_paren))
      DontParseBody = true;
  }
  Actions.ActOnSubProgramArgumentList(Context, ArgumentList);
  Actions.ActOnFunctionSpecificationPart();
  if(!DontParseBody) {
    auto EqLoc = Tok.getLocation();
    if(ExpectAndConsume(tok::equal)) {
      auto Body = ParseExpectedFollowupExpression("=");
      if(Body.isInvalid()) SkipUntilNextStatement();
      else {
        Actions.ActOnStatementFunctionBody(EqLoc, Body);
        ExpectStatementEnd();
      }
    } else
      SkipUntilNextStatement();
  }
  Actions.ActOnEndStatementFunction(Context);
  return StmtError();
}

/// ParseModule - Parse a module.
///
///   [R1104]:
///     module :=
///         module-stmt
///           [specification-part]
///           [module-subprogram-part]
///           end-module-stmt
bool Parser::ParseModule() {
  return false;
}

/// ParseBlockData - Parse block data.
///
///   [R1116]:
///     block-data :=
///         block-data-stmt
///           [specification-part]
///           end-block-data-stmt
bool Parser::ParseBlockData() {
  if (Tok.isNot(tok::kw_BLOCKDATA))
    return true;

  return false;
}

/// ParseImplicitPartList - Parse a (possibly empty) list of implicit part
/// statements.
bool Parser::ParseImplicitPartList() {
  bool HasErrors = false;
  while (true) {
    StmtResult S = ParseImplicitPart();
    if (S.isUsable()) {
      ExpectStatementEnd();
      Actions.getCurrentBody()->Append(S.take());
    } else if (S.isInvalid()) {
      SkipUntilNextStatement();
      HasErrors = true;
    } else {
      break;
    }
  }

  return HasErrors;
}

/// ParseImplicitPart - Parse the implicit part.
///
///   [R205]:
///     implicit-part :=
///         [implicit-part-stmt] ...
///           implicit-stmt
StmtResult Parser::ParseImplicitPart() {
  // [R206]:
  //   implicit-part-stmt :=
  //       implicit-stmt
  //    or parameter-stmt
  //    or format-stmt
  //    or entry-stmt [obs]
  ParseStatementLabel();
  LookForSpecificationStmtKeyword();
  StmtResult Result;
  switch (Tok.getKind()) {
  default: break;
  case tok::kw_IMPLICIT:  Result = ParseIMPLICITStmt();  break;
  case tok::kw_PARAMETER: Result = ParsePARAMETERStmt(); break;
  case tok::kw_FORMAT:    Result = ParseFORMATStmt();    break;
  case tok::kw_ENTRY:     Result = ParseENTRYStmt();     break;
  }

  return Result;
}

/// ParseExecutionPart - Parse the execution part.
///
///   [R208]:
///     execution-part :=
///         executable-construct
///           [ execution-part-construct ] ...
bool Parser::ParseExecutionPart() {
  bool HadError = false;

  while (true) {
    StmtResult SR = ParseExecutableConstruct();
    if (SR.isInvalid()) {
      SkipUntilNextStatement();
      HadError = true;
    } else if (SR.isUsable())
      ExpectStatementEnd();
    else break;
  }

  return HadError;
}

/// ParseDeclarationConstructList - Parse a (possibly empty) list of declaration
/// construct statements.
bool Parser::ParseDeclarationConstructList() {
  while (!ParseDeclarationConstruct())
    /* Parse them all */ ;

  return false;
}

/// ParseDeclarationConstruct - Parse a declaration construct.
///
///   [R207]:
///     declaration-construct :=
///         derived-type-def
///      or entry-stmt
///      or enum-def
///      or format-stmt
///      or interface-block
///      or parameter-stmt
///      or procedure-declaration-stmt
///      or specification-stmt
///      or type-declaration-stmt
///      or stmt-function-stmt
bool Parser::ParseDeclarationConstruct() {
  ParseStatementLabel();
  LookForSpecificationStmtKeyword();

  SmallVector<DeclResult, 4> Decls;  

  switch (Tok.getKind()) {
  default:
    // FIXME: error handling.
    if(ParseSpecificationStmt()) return true;
    break;
  case tok::kw_TYPE:
  case tok::kw_CLASS:
    if (IsNextToken(tok::equal))
      return true;
    if(!IsNextToken(tok::l_paren)) {
      ParseDerivedTypeDefinitionStmt();
      break;
    }
    // NB: fallthrough
  case tok::kw_INTEGER:
  case tok::kw_REAL:
  case tok::kw_COMPLEX:
  case tok::kw_CHARACTER:
  case tok::kw_LOGICAL:
  case tok::kw_DOUBLEPRECISION:
  case tok::kw_DOUBLECOMPLEX: {
    if (IsNextToken(tok::equal))
      return true;
    if (ParseTypeDeclarationStmt(Decls))
      SkipUntilNextStatement();
    else
      ExpectStatementEnd();
    Decls.clear(); // NB: the declarations can be discarded.
    break;
  }
    // FIXME: And the rest?
  }

  return false;
}

/// ParseForAllConstruct - Parse a forall construct.
///
///   [R752]:
///     forall-construct :=
///         forall-construct-stmt
///           [forall-body-construct] ...
///           end-forall-stmt
bool Parser::ParseForAllConstruct() {
  return false;
}

/// ParseArraySpec - Parse an array specification.
///
///   [R510]:
///     array-spec :=
///         explicit-shape-spec-list
///      or assumed-shape-spec-list
///      or deferred-shape-spec-list
///      or assumed-size-spec
bool Parser::ParseArraySpec(SmallVectorImpl<ArraySpec*> &Dims) {
  if(!ExpectAndConsume(tok::l_paren))
    goto error;

  // [R511], [R512], [R513]:
  //   explicit-shape-spec :=
  //       [ lower-bound : ] upper-bound
  //   lower-bound :=
  //       specification-expr
  //   upper-bound :=
  //       specification-expr
  //
  // [R729]:
  //   specification-expr :=
  //       scalar-int-expr
  //
  // [R727]:
  //   int-expr :=
  //       expr
  //
  //   C708: int-expr shall be of type integer.
  do {
    if(IsPresent(tok::star))
      Dims.push_back(ImpliedShapeSpec::Create(Context, ConsumeToken()));
    else {
      ExprResult E = ParseExpression();
      if (E.isInvalid()) goto error;
      if(ConsumeIfPresent(tok::colon)) {
        if(IsPresent(tok::star))
          Dims.push_back(ImpliedShapeSpec::Create(Context, ConsumeToken(),
                                                  E.take()));
        else {
          ExprResult E2 = ParseExpression();
          if(E2.isInvalid()) goto error;
          Dims.push_back(ExplicitShapeSpec::Create(Context, E.take(), E2.take()));
        }
      }
      else Dims.push_back(ExplicitShapeSpec::Create(Context, E.take()));
    }
  } while (ConsumeIfPresent(tok::comma));

  if (!ExpectAndConsume(tok::r_paren))
    goto error;

  return false;
 error:
  return true;
}

/// ParsePROGRAMStmt - If there is a PROGRAM statement, parse it.
/// 
///   [11.1] R1102:
///     program-stmt :=
///         PROGRAM program-name
Parser::StmtResult Parser::ParsePROGRAMStmt() {
  // Check to see if we start with a 'PROGRAM' statement.
  const IdentifierInfo *IDInfo = Tok.getIdentifierInfo();
  SourceLocation ProgramLoc = Tok.getLocation();
  if (!isaKeyword(IDInfo->getName()) || Tok.isNot(tok::kw_PROGRAM))
    return Actions.ActOnPROGRAM(Context, 0, ProgramLoc, ProgramLoc,
                                StmtLabel);

  // Parse the program name.
  Lex();
  SourceLocation NameLoc = Tok.getLocation();
  IDInfo = Tok.getIdentifierInfo();
  if(!ExpectAndConsume(tok::identifier))
    return StmtError();

  return Actions.ActOnPROGRAM(Context, IDInfo, ProgramLoc, NameLoc,
                              StmtLabel);
}

/// ParseUSEStmt - Parse the 'USE' statement.
///
///   [R1109]:
///     use-stmt :=
///         USE [ [ , module-nature ] :: ] module-name [ , rename-list ]
///      or USE [ [ , module-nature ] :: ] module-name , ONLY : [ only-list ]
Parser::StmtResult Parser::ParseUSEStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  Lex();

  // module-nature :=
  //     INTRINSIC
  //  or NON INTRINSIC
  UseStmt::ModuleNature MN = UseStmt::None;
  if (ConsumeIfPresent(tok::comma)) {
    if (ConsumeIfPresent(tok::kw_INTRINSIC)) {
      MN = UseStmt::IntrinsicStmtClass;
    } else if (ConsumeIfPresent(tok::kw_NONINTRINSIC)) {
      MN = UseStmt::NonIntrinsic;
    } else {
      Diag.ReportError(Tok.getLocation(),
                       "expected module nature keyword");
      return StmtResult(true);
    }

    if (!ExpectAndConsume(tok::coloncolon))
      return StmtError();
  }

  // Eat optional '::'.
  ConsumeIfPresent(tok::coloncolon);

  const IdentifierInfo *ModuleName = Tok.getIdentifierInfo();
  if (!ExpectAndConsume(tok::identifier))
    return StmtError();

  if (!ConsumeIfPresent(tok::comma)) {
    if (!Tok.isAtStartOfStatement()) {
      Diag.ReportError(Tok.getLocation(),
                       "expected a ',' in USE statement");
      return StmtResult(true);
    }

    return Actions.ActOnUSE(Context, MN, ModuleName, StmtLabel);
  }

  bool OnlyUse = false;
  IdentifierInfo *UseListFirstVar = 0;
  if (Tok.is(tok::kw_ONLY)) {
    UseListFirstVar = Tok.getIdentifierInfo();
    Lex(); // Eat 'ONLY'
    if (!ConsumeIfPresent(tok::colon)) {
      if (Tok.isNot(tok::equalgreater)) {
        Diag.ReportError(Tok.getLocation(),
                         "expected a ':' after the ONLY keyword");
        return StmtResult(true);
      }

      OnlyUse = false;
    } else {
      OnlyUse = true;
    }
  }

  SmallVector<UseStmt::RenamePair, 8> RenameNames;

  if (!OnlyUse && Tok.is(tok::equalgreater)) {
    // They're using 'ONLY' as a non-keyword and renaming it.
    Lex(); // Eat '=>'
    if (Tok.isAtStartOfStatement() || Tok.isNot(tok::identifier)) {
      Diag.ReportError(Tok.getLocation(),
                       "missing rename of variable in USE statement");
      return StmtResult(true);
    }

    RenameNames.push_back(UseStmt::RenamePair(UseListFirstVar,
                                              Tok.getIdentifierInfo()));
    Lex();
    ConsumeIfPresent(tok::comma);
  }

  while (!Tok.isAtStartOfStatement()) {
    const IdentifierInfo *LocalName = Tok.getIdentifierInfo();
    const IdentifierInfo *UseName = 0;
    Lex();

    if (ConsumeIfPresent(tok::equalgreater)) {
      UseName = Tok.getIdentifierInfo();
      Lex();
    }

    RenameNames.push_back(UseStmt::RenamePair(LocalName, UseName));

    if (!ConsumeIfPresent(tok::comma))
      break;
  }

  return Actions.ActOnUSE(Context, MN, ModuleName, OnlyUse, RenameNames,
                          StmtLabel);
}

/// ParseIMPORTStmt - Parse the IMPORT statement.
///
///   [R1209]:
///     import-stmt :=
///         IMPORT [ [ :: ] import-name-list ]
Parser::StmtResult Parser::ParseIMPORTStmt() {
  // Check if this is an assignment.
  if (IsNextToken(tok::equal))
    return StmtResult();

  SourceLocation Loc = Tok.getLocation();
  Lex();
  ConsumeIfPresent(tok::coloncolon);

  SmallVector<const IdentifierInfo*, 4> ImportNameList;
  while (!Tok.isAtStartOfStatement()) {
    if (Tok.isNot(tok::identifier)) {
      Diag.ReportError(Tok.getLocation(),
                       "expected import name in IMPORT statement");
      return StmtResult(true);
    }

    ImportNameList.push_back(Tok.getIdentifierInfo());
    Lex();
    if (!ConsumeIfPresent(tok::comma))
      break;
  }

  if (!Tok.isAtStartOfStatement()) {
    Diag.ReportError(Tok.getLocation(),
                     "missing comma before import name in IMPORT statement");
    SkipUntilNextStatement();
  }

  return Actions.ActOnIMPORT(Context, Loc, ImportNameList, StmtLabel);
}

/// ParseENTRYStmt - Parse the ENTRY statement.
///
///   [R1240]:
///     entry-stmt :=
///         ENTRY entry-name [ ( [ dummy-arg-list ] ) [ suffix ] ]
StmtResult Parser::ParseENTRYStmt() {
  return StmtResult();
}

/// ParseProcedureDeclStmt - Parse the procedure declaration statement.
///
///   [12.3.2.3] R1211:
///     procedure-declaration-stmt :=
///         PROCEDURE ([proc-interface]) [ [ , proc-attr-spec ]... :: ] #
///         # proc-decl-list
bool Parser::ParseProcedureDeclStmt() {
  return false;
}

/// ParseSpecificationStmt - Parse the specification statement or a statement function.
///
///   [R212]:
///     specification-stmt :=
///         access-stmt
///      or allocatable-stmt
///      or asynchronous-stmt
///      or bind-stmt
///      or common-stmt
///      or data-stmt
///      or dimension-stmt
///      or equivalence-stmt
///      or external-stmt
///      or intent-stmt
///      or intrinsic-stmt
///      or namelist-stmt
///      or optional-stmt
///      or pointer-stmt
///      or protected-stmt
///      or save-stmt
///      or target-stmt
///      or value-stmt
///      or volatile-stmt
bool Parser::ParseSpecificationStmt() {
  StmtResult Result;
  switch (Tok.getKind()) {
  default:
    // statement function.
    if(Tok.is(tok::identifier)) {
      if(IsNextToken(tok::l_paren)) {
        if(Actions.IsValidStatementFunctionIdentifier(Tok.getIdentifierInfo())) {
          Result = ParseStatementFunction();
          break;
        }
      }
    }
    return true;
  case tok::kw_PUBLIC:
  case tok::kw_PRIVATE:
    Result = ParseACCESSStmt();
    goto notImplemented;
  case tok::kw_ALLOCATABLE:
    Result = ParseALLOCATABLEStmt();
    goto notImplemented;
  case tok::kw_ASYNCHRONOUS:
    Result = ParseASYNCHRONOUSStmt();
    goto notImplemented;
  case tok::kw_BIND:
    Result = ParseBINDStmt();
    goto notImplemented;
  case tok::kw_COMMON:
    Result = ParseCOMMONStmt();
    break;
  case tok::kw_PARAMETER:
    Result = ParsePARAMETERStmt();
    break;
  case tok::kw_DATA:
    Result = ParseDATAStmt();
    break;
  case tok::kw_DIMENSION:
    Result = ParseDIMENSIONStmt();
    break;
  case tok::kw_EQUIVALENCE:
    Result = ParseEQUIVALENCEStmt();
    break;
  case tok::kw_EXTERNAL:
    Result = ParseEXTERNALStmt();
    break;
  case tok::kw_INTENT:
    Result = ParseINTENTStmt();
    goto notImplemented;
  case tok::kw_INTRINSIC:
    Result = ParseINTRINSICStmt();
    break;
  case tok::kw_NAMELIST:
    Result = ParseNAMELISTStmt();
    goto notImplemented;
  case tok::kw_OPTIONAL:
    Result = ParseOPTIONALStmt();
    goto notImplemented;
  case tok::kw_POINTER:
    Result = ParsePOINTERStmt();
    goto notImplemented;
  case tok::kw_PROTECTED:
    Result = ParsePROTECTEDStmt();
    goto notImplemented;
  case tok::kw_SAVE:
    Result = ParseSAVEStmt();
    if(Result.isUsable() && isa<AssignmentStmt>(Result.get()))
      return true; /// NB: Fixed-form ambiguity
    break;
  case tok::kw_TARGET:
    Result = ParseTARGETStmt();
    goto notImplemented;
  case tok::kw_VALUE:
    Result = ParseVALUEStmt();
    goto notImplemented;
  case tok::kw_VOLATILE:
    Result = ParseVOLATILEStmt();
    goto notImplemented;
  }

  if(Result.isInvalid())
    SkipUntilNextStatement();
  if(Result.isUsable()) {
    ExpectStatementEnd();
    Actions.getCurrentBody()->Append(Result.take());
  }

  return false;
notImplemented:
  auto Max = getMaxLocationOfCurrentToken();
  auto Loc = ConsumeAnyToken();
  Diag.Report(Loc,
              diag::err_unsupported_stmt)
      << SourceRange( Loc,
                      Max);
  SkipUntilNextStatement();
  return false;
}

/// ParseACCESSStmt - Parse the ACCESS statement.
///
///   [R524]:
///     access-stmt :=
///         access-spec [[::] access-id-list]
Parser::StmtResult Parser::ParseACCESSStmt() {
  return StmtResult();
}

/// ParseALLOCATABLEStmt - Parse the ALLOCATABLE statement.
///
///   [R526]:
///     allocatable-stmt :=
///         ALLOCATABLE [::] object-name       #
///         # [ ( deferred-shape-spec-list ) ] #
///         # [ , object-name [ ( deferred-shape-spec-list ) ] ] ...
Parser::StmtResult Parser::ParseALLOCATABLEStmt() {
  return StmtResult();
}

/// ParseASYNCHRONOUSStmt - Parse the ASYNCHRONOUS statement.
///
///   [R528]:
///     asynchronous-stmt :=
///         ASYNCHRONOUS [::] object-name-list
Parser::StmtResult Parser::ParseASYNCHRONOUSStmt() {
  SourceLocation Loc = Tok.getLocation();
  Lex();
  ConsumeIfPresent(tok::coloncolon);

  SmallVector<const IdentifierInfo*, 8> ObjNameList;
  while (!Tok.isAtStartOfStatement()) {
    if (Tok.isNot(tok::identifier)) {
      Diag.ReportError(Tok.getLocation(),
                       "expected an identifier in ASYNCHRONOUS statement");
      return StmtResult(true);
    }

    ObjNameList.push_back(Tok.getIdentifierInfo());
    Lex();

    if (!ConsumeIfPresent(tok::comma)) {
      if (!Tok.isAtStartOfStatement()) {
        Diag.ReportError(Tok.getLocation(),
                         "expected ',' in ASYNCHRONOUS statement");
        continue;
      }
      break;
    }
  }

  return Actions.ActOnASYNCHRONOUS(Context, Loc, ObjNameList, StmtLabel);
}

/// ParseBINDStmt - Parse the BIND statement.
///
///   [5.2.4] R522:
///     bind-stmt :=
///         language-binding-spec [::] bind-entity-list
Parser::StmtResult Parser::ParseBINDStmt() {
  return StmtResult();
}

/// ParseDATAStmt - Parse the DATA statement.
///
///   [R524]:
///     data-stmt :=
///         DATA data-stmt-set [ [,] data-stmt-set ] ...
Parser::StmtResult Parser::ParseDATAStmt() {
  SourceLocation Loc = ConsumeToken();

  SmallVector<Stmt *,8> StmtList;

  while(true) {
    auto Stmt = ParseDATAStmtPart(Loc);
    if(Stmt.isInvalid()) return StmtError();
    StmtList.push_back(Stmt.take());
    if(Tok.isAtStartOfStatement()) break;
    ConsumeIfPresent(tok::comma);
  }

  return Actions.ActOnCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

Parser::StmtResult Parser::ParseDATAStmtPart(SourceLocation Loc) {
  SmallVector<Expr*, 8> Objects;
  SmallVector<Expr*, 8> Values;
  bool HadSemaErrors = false;

  // nlist
  do {
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_expression);
      return StmtError();
    }

    ExprResult E;
    if(Tok.is(tok::l_paren)) {
      E = ParseDATAStmtImpliedDo();
      if(E.isUsable())
        E = Actions.ActOnDATAOuterImpliedDoExpr(Context, E);
    }
    else
       E = ParsePrimaryExpr();

     if(E.isInvalid())
       return StmtError();
     if(E.isUsable())
       Objects.push_back(E.get());
  } while(ConsumeIfPresent(tok::comma));

  // clist
  if(!ExpectAndConsume(tok::slash))
    return StmtError();

  do {
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_expression);
      return StmtError();
    }

    ExprResult Repeat;
    SourceLocation RepeatLoc;
    if(Tok.is(tok::int_literal_constant)
       && IsNextToken(tok::star)) {
      Repeat = ParsePrimaryExpr();
      RepeatLoc = Tok.getLocation();
      ConsumeIfPresent(tok::star);
      if(Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_expression);
        return StmtError();
      }
    }
    auto Value = ParsePrimaryExpr();
    if(Value.isInvalid()) return StmtError();

    Value = Actions.ActOnDATAConstantExpr(Context, RepeatLoc, Repeat, Value);
    if(Value.isUsable())
      Values.push_back(Value.get());
    else HadSemaErrors = true;
  } while(ConsumeIfPresent(tok::comma));

  if(!ExpectAndConsume(tok::slash))
    return StmtError();

  if(HadSemaErrors)
    return StmtError();
  return Actions.ActOnDATA(Context, Loc, Objects, Values, nullptr);
}

Parser::ExprResult Parser::ParseDATAStmtImpliedDo() {
  auto Loc = Tok.getLocation();
  Lex();

  SmallVector<ExprResult, 8> DList;
  ExprResult E1, E2, E3;

  while(true) {
    ExprResult E;
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_expression);
      return ExprError();
    }

    if(Tok.is(tok::l_paren))
      E = ParseDATAStmtImpliedDo();
    else {
      auto PrevDontResolveIdentifiersInSubExpressions =
             DontResolveIdentifiersInSubExpressions;
      DontResolveIdentifiersInSubExpressions = true;
      E = ParsePrimaryExpr();
      DontResolveIdentifiersInSubExpressions =
        PrevDontResolveIdentifiersInSubExpressions;
    }

    if(E.isInvalid()) return ExprError();
    DList.push_back(E);

    if(ConsumeIfPresent(tok::comma)) {
      if(Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_expression);
        return ExprError();
      }
      if(IsNextToken(tok::equal)) break;
    } else {
      Diag.Report(getExpectedLoc(), diag::err_expected_comma);
      return ExprError();
    }
  }

  auto IDLoc = Tok.getLocation();
  auto IDInfo = Tok.getIdentifierInfo();
  if(!ExpectAndConsume(tok::identifier))
    return ExprError();

  if(!ExpectAndConsume(tok::equal))
    return ExprError();

  auto PrevDontResolveIdentifiers = DontResolveIdentifiers;
  DontResolveIdentifiers = true;

  E1 = ParseExpectedFollowupExpression("=");
  if(E1.isInvalid()) return E1;
  if(!ExpectAndConsume(tok::comma)) {
    // NB: don't forget
    DontResolveIdentifiers = PrevDontResolveIdentifiers;
    return ExprError();
  }
  E2 = ParseExpectedFollowupExpression(",");
  if(E2.isInvalid()) return E2;
  if(ConsumeIfPresent(tok::comma)) {
    E3 = ParseExpectedFollowupExpression(",");
  }
  if(E3.isInvalid()) return E3;

  DontResolveIdentifiers = PrevDontResolveIdentifiers;

  if(!ExpectAndConsume(tok::r_paren))
    return ExprError();

  return Actions.ActOnDATAImpliedDoExpr(Context, Loc, IDLoc, IDInfo,
                                        DList, E1, E2, E3);
}

/// ParseINTENTStmt - Parse the INTENT statement.
///
///   [R536]:
///     intent-stmt :=
///         INTENT ( intent-spec ) [::] dummy-arg-name-list
Parser::StmtResult Parser::ParseINTENTStmt() {
  return StmtResult();
}

/// ParseNAMELISTStmt - Parse the NAMELIST statement.
///
///   [R552]:
///     namelist-stmt :=
///         NAMELIST #
///         # / namelist-group-name / namelist-group-object-list #
///         # [ [,] / namelist-group-name / #
///         #   namelist-group-object-list ] ...
Parser::StmtResult Parser::ParseNAMELISTStmt() {
  return StmtResult();
}

/// ParseOPTIONALStmt - Parse the OPTIONAL statement.
///
///   [R537]:
///     optional-stmt :=
///         OPTIONAL [::] dummy-arg-name-list
Parser::StmtResult Parser::ParseOPTIONALStmt() {
  return StmtResult();
}

/// ParsePOINTERStmt - Parse the POINTER statement.
///
///   [R540]:
///     pointer-stmt :=
///         POINTER [::] pointer-decl-list
Parser::StmtResult Parser::ParsePOINTERStmt() {
  return StmtResult();
}

/// ParsePROTECTEDStmt - Parse the PROTECTED statement.
///
///   [R542]:
///     protected-stmt :=
///         PROTECTED [::] entity-name-list
Parser::StmtResult Parser::ParsePROTECTEDStmt() {
  return StmtResult();
}

/// ParseTARGETStmt - Parse the TARGET statement.
///
///   [R546]:
///     target-stmt :=
///         TARGET [::] object-name [ ( array-spec ) ] #
///         # [ , object-name [ ( array-spec ) ] ] ...
Parser::StmtResult Parser::ParseTARGETStmt() {
  return StmtResult();
}

/// ParseVALUEStmt - Parse the VALUE statement.
///
///   [R547]:
///     value-stmt :=
///         VALUE [::] dummy-arg-name-list
Parser::StmtResult Parser::ParseVALUEStmt() {
  return StmtResult();
}

/// ParseVOLATILEStmt - Parse the VOLATILE statement.
///
///   [R548]:
///     volatile-stmt :=
///         VOLATILE [::] object-name-list
Parser::StmtResult Parser::ParseVOLATILEStmt() {
  return StmtResult();
}

/// ParseALLOCATEStmt - Parse the ALLOCATE statement.
///
///   [R623]:
///     allocate-stmt :=
///         ALLOCATE ( [ type-spec :: ] alocation-list [ , alloc-opt-list ] )
Parser::StmtResult Parser::ParseALLOCATEStmt() {
  return StmtResult();
}

/// ParseNULLIFYStmt - Parse the NULLIFY statement.
///
///   [R633]:
///     nullify-stmt :=
///         NULLIFY ( pointer-object-list )
Parser::StmtResult Parser::ParseNULLIFYStmt() {
  return StmtResult();
}

/// ParseDEALLOCATEStmt - Parse the DEALLOCATE statement.
///
///   [R635]:
///     deallocate-stmt :=
///         DEALLOCATE ( allocate-object-list [ , dealloc-op-list ] )
Parser::StmtResult Parser::ParseDEALLOCATEStmt() {
  return StmtResult();
}

/// ParseFORALLStmt - Parse the FORALL construct statement.
///
///   [R753]:
///     forall-construct-stmt :=
///         [forall-construct-name :] FORALL forall-header
Parser::StmtResult Parser::ParseFORALLStmt() {
  return StmtResult();
}

/// ParseENDFORALLStmt - Parse the END FORALL construct statement.
/// 
///   [R758]:
///     end-forall-stmt :=
///         END FORALL [forall-construct-name]
Parser::StmtResult Parser::ParseEND_FORALLStmt() {
  return StmtResult();
}

} //namespace flang
