//===-- Parser.h - Fortran Parser Interface ---------------------*- C++ -*-===//
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

#ifndef FLANG_PARSER_PARSER_H__
#define FLANG_PARSER_PARSER_H__

#include "flang/AST/ASTContext.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"
#include "flang/Basic/IdentifierTable.h"
#include "flang/Basic/LangOptions.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Parse/FixedForm.h"
#include "flang/Parse/Lexer.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Ownership.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Twine.h"
#include <vector>

namespace llvm {
  class SourceMgr;
} // end namespace llvm

namespace flang {

class Action;
class VarExpr;
class ConstantExpr;
class DeclGroupRef;
class Expr;
class ArraySpec;
class Parser;
class Selector;
class Sema;
class UnitSpec;
class FormatSpec;

/// PrettyStackTraceParserEntry - If a crash happens while the parser is active,
/// an entry is printed for it.
class PrettyStackTraceParserEntry : public llvm::PrettyStackTraceEntry {
  const Parser &FP;
public:
  PrettyStackTraceParserEntry(const Parser &fp) : FP(fp) {}
  virtual void print(llvm::raw_ostream &OS) const;
};

/// Parser - This implements a parser for the Fortran family of languages. After
/// parsing units of the grammar, productions are invoked to handle whatever has
/// been read.
class Parser {
public:
  enum RetTy {
    Success,                //< The construct was parsed successfully
    WrongConstruct,         //< The construct we wanted to parse wasn't present
    Error                   //< There was an error parsing
  };
private:

  Lexer TheLexer;
  LangOptions Features;
  PrettyStackTraceParserEntry CrashInfo;
  llvm::SourceMgr &SrcMgr;

  /// LexerBufferContext - This is a stack of lexing contexts
  /// for files lower in the include stack
  std::vector<const char*> LexerBufferContext;

  /// This is the current buffer index we're lexing from as managed by the
  /// SourceMgr object.
  std::vector<int> CurBufferIndex;

  ASTContext &Context;

  /// Diag - Diagnostics for parsing errors.
  DiagnosticsEngine &Diag;

  /// Actions - These are the callbacks we invoke as we parse various constructs
  /// in the file. 
  Sema &Actions;

  /// FirstLoc - The location of the first token in the given statement.
  SourceLocation LocFirstStmtToken;

  /// Tok - The current token we are parsing. All parsing methods assume that
  /// this is valid.
  Token Tok;

  /// NextTok - The next token so that we can do one level of lookahead.
  Token NextTok;

  /// StmtLabel - If set, this is the statement label for the statement.
  Expr *StmtLabel;

  /// ConstructName - If set, this is the construct name for the construct.
  ConstructName StmtConstructName;

  unsigned short ParenCount, ParenSlashCount,
                 BracketCount, BraceCount;

  fixedForm::CommonAmbiguities FixedFormAmbiguities;

  // PrevTokLocation - The location of the token we previously consumed. This
  // token is used for diagnostics where we expected to see a token following
  // another token.
  SourceLocation PrevTokLocation;

  /// Returns the end of location of the previous token.
  SourceLocation PrevTokLocEnd;

  /// DontResolveIdentifiers - if set, the identifier tokens create
  /// an UnresolvedIdentifierExpr, instead of resolving the identifier.
  /// As a of this result Subscript, Substring and function call
  /// expressions aren't parsed.
  bool DontResolveIdentifiers;

  /// DontResolveIdentifiersInSubExpressions - if set,
  /// ParsePrimaryExpression will set the DontResolveIdentifiers
  /// to true when parsing any subexpressions like array subscripts,
  /// etc.
  bool DontResolveIdentifiersInSubExpressions;

  /// LexFORMATTokens - if set,
  /// The lexer will lex the format descriptor tokens instead
  /// of normal tokens.
  bool LexFORMATTokens;

  /// Identifiers - This is mapping/lookup information for all identifiers in
  /// the program, including program keywords.
  mutable IdentifierTable Identifiers;

  /// PrevStmtWasSelectCase - if set, this indicates that the last statement
  /// was select case and a case or an end select statement is expected.
  bool PrevStmtWasSelectCase;

private:

  /// getIdentifierInfo - Return information about the specified identifier
  /// token.
  IdentifierInfo *getIdentifierInfo(std::string &Name) const {
    return &Identifiers.get(Name);
  }

  /// Returns the maximum location of the current token
  SourceLocation getMaxLocationOfCurrentToken() {
    return SourceLocation::getFromPointer(Tok.getLocation().getPointer() +
                                          Tok.getLength());
  }

  /// CleanLiteral - Cleans up a literal if it needs cleaning. It removes the
  /// continuation contexts and comments. Cleaning a dirty literal is SLOW!
  void CleanLiteral(Token T, std::string &NameStr);

  bool EnterIncludeFile(const std::string &Filename);
  bool LeaveIncludeFile();

  /// IsNextToken - Returns true if the next token is a token kind
  bool IsNextToken(tok::TokenKind TokKind);

  void Lex();
  void ClassifyToken(Token &T);
public:

  typedef OpaquePtr<DeclGroupRef> DeclGroupPtrTy;

  typedef flang::ExprResult ExprResult;
  typedef flang::StmtResult StmtResult;
  typedef flang::FormatItemResult FormatItemResult;

  bool isaIdentifier(const llvm::StringRef &ID) const {
    return Identifiers.isaIdentifier(ID);
  }
  bool isaKeyword(const llvm::StringRef &KW) const {
    return Identifiers.isaKeyword(KW);
  }

  Parser(llvm::SourceMgr &SrcMgr, const LangOptions &Opts,
         DiagnosticsEngine &D, Sema &actions);

  llvm::SourceMgr &getSourceManager() { return SrcMgr; }

  const Token &getCurToken() const { return Tok; }
  const Lexer &getLexer() const { return TheLexer; }
  Lexer &getLexer() { return TheLexer; }

  bool ParseProgramUnits();

  ExprResult ExprError() { return ExprResult(true); }
  StmtResult StmtError() { return StmtResult(true); }

private:

  /// getExpectedLoc - returns the location
  /// for the expected token.
  SourceLocation getExpectedLoc() const;

  /// getExpectedLocForFixIt - returns the location
  /// in which the expected token should be inserted in.
  inline SourceLocation getExpectedLocForFixIt() const {
    return PrevTokLocEnd;
  }

  /// getTokenRange - returns the range of the token at
  /// the given location.
  SourceRange getTokenRange(SourceLocation Loc) const;

  /// getTokenRange - returns the range of the current token.
  SourceRange getTokenRange() const;

  //===--------------------------------------------------------------------===//
  // Low-Level token peeking and consumption methods.
  //

  /// isTokenParen - Return true if the cur token is '(' or ')'.
  bool isTokenParen() const {
    return Tok.getKind() == tok::l_paren || Tok.getKind() == tok::r_paren;
  }

  /// isTokenParenSlash - Return true if the cur token is '(/' or '/)'.
  bool isTokenParenSlash() const {
    return Tok.getKind() == tok::l_parenslash || Tok.getKind() == tok::slashr_paren;
  }

  /// isTokenIdentifier - Return true if the cur token is a usable
  /// identifier (this can also be a keyword).
  bool isTokenIdentifier() const {
    if (Tok.is(tok::identifier) ||
        (Tok.getIdentifierInfo() &&
          isaKeyword(Tok.getIdentifierInfo()->getName()))
        )
      return true;
    return false;
  }

  /// ConsumeToken - Consume the current token and lex the next one. This
  /// returns the location of the consumed token.
  SourceLocation ConsumeToken() {
    auto Loc = Tok.getLocation();
    Lex();
    return Loc;
  }

  /// ConsumeAnyToken - Dispatch to the right Consume* method based on the
  /// current token type.  This should only be used in cases where the type of
  /// the token really isn't known, e.g. in error recovery.
  SourceLocation ConsumeAnyToken(bool ConsumeCodeCompletionTok = false) {
    if (isTokenParen())
      return ConsumeParen();
    else if (isTokenParenSlash())
      return ConsumeParenSlash();
    else return ConsumeToken();
  }

  /// ConsumeParen - This consume method keeps the paren count up-to-date.
  ///
  SourceLocation ConsumeParen() {
    assert(isTokenParen() && "wrong consume method");
    auto Loc = Tok.getLocation();
    if (Tok.getKind() == tok::l_paren)
      ++ParenCount;
    else if (ParenCount)
      --ParenCount;       // Don't let unbalanced )'s drive the count negative.
    Lex();
    return Loc;
  }

  /// ConsumeParenSlash - This consume method keeps the paren slash count up-to-date.
  ///
  SourceLocation ConsumeParenSlash() {
    assert(isTokenParenSlash() && "wrong consume method");
    auto Loc = Tok.getLocation();
    if (Tok.getKind() == tok::l_parenslash)
      ++ParenSlashCount;
    else if (ParenSlashCount)
      --ParenSlashCount;       // Don't let unbalanced /)'s drive the count negative.
    Lex();
    return Loc;
  }

  /// IsPresent - Returns true if the next token is Tok.
  bool IsPresent(tok::TokenKind TokKind, bool InSameStatement = true);

  /// ConsumeIfPresent - Consumes the token if it's present. Return 'true' if it was
  /// delicious.
  bool ConsumeIfPresent(tok::TokenKind OptionalTok, bool InSameStatement = true);

  /// ExpectAndConsume - The parser expects that 'ExpectedTok' is next in the
  /// input.  If so, it is consumed and true is returned.
  ///
  /// If the input is malformed, this emits the specified diagnostic.  Next, if
  /// SkipToTok is specified, it calls SkipUntil(SkipToTok).  Finally, false is
  /// returned.
  ///
  /// If the diagnostic isn't specified, the appropriate diagnostic for
  /// the given token type is emitted.
  bool ExpectAndConsume(tok::TokenKind ExpectedTok, unsigned Diag = 0,
                        const char *DiagMsg = "",
                        tok::TokenKind SkipToTok = tok::unknown,
                        bool InSameStatement = true);

  bool ExpectAndConsumeFixedFormAmbiguous(tok::TokenKind ExpectedTok, unsigned Diag = 0,
                                          const char *DiagMsg = "");

  /// ExpectStatementEnd - The parser expects that the next token is in the
  /// next statement.
  bool ExpectStatementEnd();

  /// SkipUntil - Read tokens until we get to the specified token, then consume
  /// it.  Because we cannot guarantee that the
  /// token will ever occur, this skips to the next token, or to some likely
  /// good stopping point.  If StopAtNextStatement is true,
  /// skipping will stop when the next statement is reached.
  ///
  /// If SkipUntil finds the specified token, it returns true, otherwise it
  /// returns false.
  bool SkipUntil(tok::TokenKind T, bool StopAtNextStatement = true,
                 bool DontConsume = false) {
    return SkipUntil(llvm::makeArrayRef(T), StopAtNextStatement,
                     DontConsume);
  }
  bool SkipUntil(tok::TokenKind T1, tok::TokenKind T2, bool StopAtNextStatement = true,
                 bool DontConsume = false) {
    tok::TokenKind TokArray[] = {T1, T2};
    return SkipUntil(TokArray, StopAtNextStatement, DontConsume);
  }
  bool SkipUntil(tok::TokenKind T1, tok::TokenKind T2, tok::TokenKind T3,
                 bool StopAtNextStatement = true, bool DontConsume = false) {
    tok::TokenKind TokArray[] = {T1, T2, T3};
    return SkipUntil(TokArray, StopAtNextStatement, DontConsume);
  }
  bool SkipUntil(ArrayRef<tok::TokenKind> Toks, bool StopAtNextStatement = true,
                 bool DontConsume = false);

  /// SkipUntilNextStatement - Consume tokens until the next statement is reached.
  /// Returns true.
  bool SkipUntilNextStatement();

  /// Certain fixed-form are ambigous, and might need to be reparsed
  void StartStatementReparse(SourceLocation Where = SourceLocation());

  StmtResult ReparseAmbiguousAssignmentStatement(SourceLocation Where = SourceLocation());

  void ReLexAmbiguousIdentifier(const fixedForm::KeywordMatcher &Matcher);

  // High-level parsing methods.
  bool ParseInclude();
  bool ParseProgramUnit();
  bool ParseMainProgram();
  bool ParseExternalSubprogram();
  bool ParseExternalSubprogram(DeclSpec &ReturnType, int Attr);
  bool ParseTypedExternalSubprogram(int Attr);
  bool ParseRecursiveExternalSubprogram();
  bool ParseExecutableSubprogramBody(tok::TokenKind EndKw);
  bool ParseModule();
  bool ParseBlockData();

  bool ParseRESULT();

  StmtResult ParseStatementFunction();

  bool ParseSpecificationPart();
  bool ParseImplicitPartList();
  StmtResult ParseImplicitPart();
  bool ParseExecutionPart();

  bool ParseDeclarationConstructList();
  bool ParseDeclarationConstruct();
  bool ParseForAllConstruct();
  void CheckStmtOrder(SourceLocation Loc, StmtResult SR);
  StmtResult ParseExecutableConstruct();

  bool ParseTypeDeclarationStmt(SmallVectorImpl<DeclResult> &Decls);

  /// ParseDeclarationTypeSpec - returns true if a parsing error
  /// occurred.
  bool ParseDeclarationTypeSpec(DeclSpec &DS, bool AllowSelectors = true,
                                bool AllowOptionalCommaAfterCharLength = true);

  /// ParseTypeOrClassDeclTypeSpec - parses a TYPE(..) or CLASS(..) type
  /// specification. Returns true if a parsing error ocurred.
  bool ParseTypeOrClassDeclTypeSpec(DeclSpec &DS);

  /// ParseEntityDeclarationList - returns true if a parsing error
  /// occurred.
  bool ParseEntityDeclarationList(DeclSpec &DS,
                                SmallVectorImpl<DeclResult> &Decls);

  /// ParseDimensionAttributeSpec - parses the DIMENSION attribute
  /// for the given declaration and returns true if a parsing error
  /// occurred.
  bool ParseDimensionAttributeSpec(SourceLocation Loc, DeclSpec &DS);

  /// ParseObjectArraySpec - parses the optional array specification
  /// which comes after an object name and returns true if a parsing
  /// error occurred.
  bool ParseObjectArraySpec(SourceLocation Loc, DeclSpec &DS);

  /// ParseObjectCharLength - parses the optional character length
  /// specification which comes after an object name and returns
  /// true if a parsing error occurred.
  bool ParseObjectCharLength(SourceLocation Loc, DeclSpec &DS);

  bool ParseProcedureDeclStmt();
  bool ParseSpecificationStmt();
  StmtResult ParseActionStmt();

  // Designator parsing methods.
  ExprResult ParseDesignator(bool IsLvalue);
  ExprResult ParseNameOrCall();
  ExprResult ParseArrayElement();
  ExprResult ParseArraySection();
  ExprResult ParseCoindexedNamedObject();
  ExprResult ParseComplexPartDesignator();
  ExprResult ParseStructureComponent(ExprResult Target);
  ExprResult ParseSubstring(ExprResult Target);
  ExprResult ParseArraySubscript(ExprResult Target);
  ExprResult ParseArraySection(const char *PunctuationTok);
  ExprResult ParseDataReference();
  ExprResult ParsePartReference();

  // Stmt-level parsing methods.
  void LookForTopLevelStmtKeyword();
  StmtResult ParsePROGRAMStmt();
  StmtResult ParseENDStmt(tok::TokenKind EndKw);
  StmtResult ParseUSEStmt();
  StmtResult ParseIMPORTStmt();
  StmtResult ParseIMPLICITStmt();
  StmtResult ParsePARAMETERStmt();
  StmtResult ParseFORMATStmt();
  StmtResult ParseFORMATSpec(SourceLocation Loc);
  FormatItemResult ParseFORMATItems(bool IsOuter = false);
  FormatItemResult ParseFORMATItem();
  StmtResult ParseENTRYStmt();

  // Specification statement's contents.
  void LookForSpecificationStmtKeyword();
  StmtResult ParseACCESSStmt();
  StmtResult ParseALLOCATABLEStmt();
  StmtResult ParseASYNCHRONOUSStmt();
  StmtResult ParseBINDStmt();
  StmtResult ParseCOMMONStmt();
  StmtResult ParseDATAStmt();
  StmtResult ParseDATAStmtPart(SourceLocation Loc);
  ExprResult ParseDATAStmtImpliedDo();
  StmtResult ParseDIMENSIONStmt();
  StmtResult ParseEQUIVALENCEStmt();
  StmtResult ParseEXTERNALStmt();
  StmtResult ParseINTENTStmt();
  StmtResult ParseINTRINSICStmt(bool IsActuallyExternal = false);
  StmtResult ParseNAMELISTStmt();
  StmtResult ParseOPTIONALStmt();
  StmtResult ParsePOINTERStmt();
  StmtResult ParsePROTECTEDStmt();
  StmtResult ParseSAVEStmt();
  StmtResult ParseTARGETStmt();
  StmtResult ParseVALUEStmt();
  StmtResult ParseVOLATILEStmt();

  // Dynamic association.
  StmtResult ParseALLOCATEStmt();
  StmtResult ParseNULLIFYStmt();
  StmtResult ParseDEALLOCATEStmt();

  StmtResult ParseFORALLStmt();
  StmtResult ParseEND_FORALLStmt();

  // Executable statements
  void LookForExecutableStmtKeyword(bool AtStartOfStatement = true);
  StmtResult ParseAssignStmt();
  StmtResult ParseGotoStmt();

  StmtResult ParseIfStmt();
  StmtResult ParseElseIfStmt();
  StmtResult ParseElseStmt();
  StmtResult ParseEndIfStmt();
  ExprResult ParseExpectedConditionExpression(const char *DiagAfter);

  StmtResult ParseDoStmt();
  StmtResult ParseDoWhileStmt(bool isDo);
  StmtResult ReparseAmbiguousDoWhileStatement();
  StmtResult ParseEndDoStmt();
  StmtResult ParseCycleStmt();
  StmtResult ParseExitStmt();

  StmtResult ParseSelectCaseStmt();
  StmtResult ParseCaseStmt();
  StmtResult ParseEndSelectStmt();

  StmtResult ParseWhereStmt();
  StmtResult ParseElseWhereStmt();
  StmtResult ParseEndWhereStmt();

  StmtResult ParseContinueStmt();
  StmtResult ParseStopStmt();
  StmtResult ParseReturnStmt();
  StmtResult ParseCallStmt();
  StmtResult ParseAmbiguousAssignmentStmt();
  StmtResult ParseAssignmentStmt();
  StmtResult ParsePrintStmt();
  StmtResult ParseWriteStmt();
  UnitSpec *ParseUNITSpec(bool IsLabeled);
  FormatSpec *ParseFMTSpec(bool IsLabeled);
  void ParseIOList(SmallVectorImpl<ExprResult> &List);

  // Helper functions.
  ExprResult ParseLevel5Expr();
  ExprResult ParseEquivOperand();
  ExprResult ParseOrOperand();
  ExprResult ParseAndOperand();
  ExprResult ParseLevel4Expr();
  ExprResult ParseLevel3Expr();
  ExprResult ParseLevel2Expr();
  ExprResult ParseAddOperand();
  ExprResult ParseMultOperand();
  ExprResult ParseLevel1Expr();
  ExprResult ParsePrimaryExpr(bool IsLvalue = false);
  ExprResult ParseExpression();
  ExprResult ParseFunctionCallArgumentList(SmallVectorImpl<Expr*> &Args,
                                           SourceLocation &RParenLoc);
  ExprResult ParseRecursiveCallExpression(SourceRange IDRange);
  ExprResult ParseCallExpression(SourceLocation IDLoc, FunctionDecl *Function);
  ExprResult ParseArrayConstructor();
  ExprResult ParseTypeConstructor(SourceLocation IDLoc, RecordDecl *Record);

  /// \brief Looks at the next token to see if it's an expression
  /// and calls ParseExpression if it is, or reports an expected expression
  /// error.
  ExprResult ParseExpectedExpression();

  /// \brief Looks at the next token to see if it's an expression
  /// and calls ParseExpression if it is, or reports an expected expression
  /// error.
  ExprResult ParseExpectedFollowupExpression(const char *DiagAfter = "");

  void ParseStatementLabel();
  ExprResult ParseStatementLabelReference(bool Consume = true);

  /// ParseConstructNameLabel - Parses an optional construct-name ':' label.
  /// If the construct name isn't there, then set the ConstructName to null.
  void ParseConstructNameLabel();

  /// ParseTrailingConstructName - Parses an optional trailing construct-name identifier.
  /// If the construct name isn't there, then set the ConstructName to null.
  void ParseTrailingConstructName();

  // Declaration construct functions
  bool ParseDerivedTypeDefinitionStmt();
  void ParseEndTypeStmt();
  bool ParseDerivedTypeComponentStmt();

  ExprResult ParseSelector(bool IsKindSel);
  bool ParseArraySpec(llvm::SmallVectorImpl<ArraySpec*> &Dims);
  bool ParseCharacterStarLengthSpec(DeclSpec &DS);

  void SetKindSelector(ConstantExpr *E, StringRef Kind);

};

} // end flang namespace

#endif
