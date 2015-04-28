//===-- Lexer.h - Fortran Lexer Interface -----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran lexer interface.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_PARSER_LEXER_H__
#define FLANG_PARSER_LEXER_H__

#include "flang/Basic/LangOptions.h"
#include "flang/Basic/Token.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include <string>
#include <vector>

namespace llvm {

class MemoryBuffer;
class SourceLocation;
class SourceMgr;

} // end namespace llvm

namespace flang {

class DiagnosticsEngine;
class CommentHandler;
class Parser;

namespace fixedForm {

class KeywordMatcher;

}

class Lexer {
  /// LineOfText - This represents a line of text in the program where
  /// continuation contexts are concatenated.
  class LineOfText {
    DiagnosticsEngine &Diags;
    const LangOptions &LanguageOptions;

    /// Atoms - A vector of atoms which make up one continuation context free
    /// line in the program. E.g.
    ///
    ///   'hello &
    ///   ! comment
    ///   &world'
    ///
    /// becomes two 'atoms' which can be treated as one contiguous line. I.e.
    ///
    ///   'hello world'
    SmallVector<StringRef, 8> Atoms;

    /// BufPtr - This is the next line to be lexed.
    const char *BufPtr;

    /// CurAtom - The current atom.
    unsigned CurAtom;

    /// CurPtr - Current index into the buffer. This is the next character to be
    /// lexed.
    uint64_t CurPtr;

    /// SkipBlankLinesAndComments - Helper function that skips blank lines and
    /// lines with only comments.
    bool SkipBlankLinesAndComments(unsigned &I, const char *&LineBegin,
                                   bool IgnoreContinuationChar = false);

    void SkipFixedFormBlankLinesAndComments(unsigned &I, const char *&LineBegin);

    /// GetCharacterLiteral - A character literal has to be treated specially
    /// because an ampersand may exist within it.
    void GetCharacterLiteral(unsigned &I, const char *&LineBegin, bool &PadAtoms);

    /// Padding - This is an extra space that we insert between two
    /// continuations which were merged. E.g.:
    ///
    ///   FOO&
    ///   BAR
    ///
    /// is interpreted as "FOO BAR" instead of "FOOBAR".
    static const char *Padding;

    friend class Lexer;
  public:
    explicit LineOfText(DiagnosticsEngine &D, const LangOptions &L)
      : Diags(D), LanguageOptions(L), BufPtr(0), CurAtom(0), CurPtr(0) {}

    void SetBuffer(const llvm::MemoryBuffer *Buf, const char *Ptr,
                   bool AtLineStart = true);

    bool empty() const { return Atoms.empty(); }

    /// IsInCurrentAtom - Return true if the pointer is within the current atom
    /// and before the current pointer.
    bool IsInCurrentAtom(const char *Ptr) const {
      const char *Data = Atoms[CurAtom].data();
      return Ptr >= Data && Ptr < &Data[CurPtr];
    }

    /// State - The Lexing state of the current line
    struct State {
      const char *BufPtr;
      unsigned CurAtom;
      uint64_t CurPtr;
    };

    /// GetState - Returns the lexing state of this line
    State GetState();

    /// SetState - Sets the lexing state of this line
    void SetState(const State &S);

    /// GetNextLine - Get the next line of the program to lex.
    void GetNextLine(bool AtLineStart = true);

    /// Reset the internal state to make ready for a new line of text.
    void Reset() {
      CurPtr = CurAtom = 0;
      Atoms.clear();
    }

    bool AtEndOfLine() const {
      return CurAtom == Atoms.size() - 1 && CurPtr >= Atoms[CurAtom].size();
    }

    char GetNextChar();
    char PeekNextChar() const;
    char PeekNextChar(int Offset) const;
    char PeekPrevChar() const;
    char GetCurrentChar() const;

    const char *GetLineBegin() const {
      assert(!Atoms.empty() && "Trying to get the start of an empty string!");
      return Atoms[0].data();
    }
    const char *GetCurrentPtr() const {
      assert(!Atoms.empty() && "Trying to get data from an empty string!");
      if (Atoms[CurAtom].data() == Padding)
        return Atoms[CurAtom + 1].data();
      return &Atoms[CurAtom].data()[CurPtr];
    }
    const char* GetBufferPtr() const {
      return BufPtr;
    }

    void dump() const;
    void dump(raw_ostream &OS) const;
  };

  /// Text - The text of the program.
  LineOfText Text;

  /// getNextChar - Get the next character from the buffer.
  char getNextChar() { return Text.GetNextChar(); }

  /// peekNextChar - Peek at the next character, but don't advance the buffer.
  char peekNextChar() const { return Text.PeekNextChar(); }

  char peekNextChar(int Offset) const { return Text.PeekNextChar(Offset); }

  /// peekPrevChar - Peek at the previous character.
  char peekPrevChar() const { return Text.PeekPrevChar(); }

  /// getCurrentChar - Get the current character the buffer's looking at.
  char getCurrentChar() { return Text.GetCurrentChar(); }

  /// getLineBegin - Get the start of the current line of text.
  const char *getLineBegin() const { return Text.GetLineBegin(); }

  /// getCurrentPtr - Get a pointer to the current character.
  const char *getCurrentPtr() const { return Text.GetCurrentPtr(); }

  DiagnosticsEngine &Diags;
  llvm::SourceMgr &SrcMgr;
  LangOptions Features;

  //===--------------------------------------------------------------------===//
  // Constant configuration values for this lexer.
  const llvm::MemoryBuffer *CurBuf;  // Start of the buffer.

  //===--------------------------------------------------------------------===//
  // Context that changes as the file is lexed.

  /// TokStart - Start of the current token.
  const char *TokStart;

  /// CurKind - The current "kind" of token.
  tok::TokenKind CurKind;

  /// LastTokenWasSemicolon - True if the last token we returned was a
  /// semicolon.
  bool LastTokenWasSemicolon;

  /// \brief Tracks all of the comment handlers that the client registered
  /// with this preprocessor.
  std::vector<CommentHandler *> CommentHandlers;

  /// SkipWhitespace - Efficiently skip over a series of whitespace characters.
  /// Update CurPtr to point to the next non-whitespace character and return.
  bool SkipWhitespace(Token &Result, const char *CurPtr);

  /// LexBlankLinesAndComments - Lex blank lines and lines with only
  /// comments. Used after we've parsed an ampersand.
  void LexBlankLinesAndComments();

  /// LexComment - Lex a comment. We sometimes want to return the comment.
  void LexComment(Token &Result);

  /// LexIdentifier - Lex an identifier token.
  void LexIdentifier(Token &Result);

  /// LexFixedFormIdentifier - Lex a fixed form identifier token.
  void LexFixedFormIdentifier(Token &Result);

  void LexFixedFormIdentifier(const fixedForm::KeywordMatcher &Matcher,
                              Token &Tok);

  /// LexPossibleDefinedOperator - Tries to lex a defined operator
  /// that begins with '.', returns true if sucessfull.
  bool LexPossibleDefinedOperator(Token &Result);

  /// LexFORMATDescriptor - Lex a format desriptor.
  void LexFORMATDescriptor(Token &Result);

  /// LexStatementLabel - Lex the remainder of a statement label -- a 5-digit
  /// number.
  void LexStatementLabel(Token &Result);

  /// LexIntegerLiteralConstant - Lex an integer literal constant.
  bool LexIntegerLiteralConstant();

  /// LexNumericConstant - Lex an integer or floating point constant.
  void LexNumericConstant(Token &Result);

  /// LexCharacterLiteralConstant - Lex the remainder of a character literal
  /// constant (string).
  void LexCharacterLiteralConstant(Token &Result, bool DoubleQuotes);

  /// LexBOZConstant - Lex the remainder of a BOZ constant. From[-1] is the
  /// first character lexed.  Return the end of the constant.
  template <bool (*Compare)(unsigned char)>
  void LexBOZConstant(Token &Result, const char *CurPtr, tok::TokenKind Kind);

  /// GetNextCharacter - Get the next character from the buffer ignoring
  /// continuation contexts.
  char GetNextCharacter(bool IncPtr = true);

  /// LexTokenInternal - This implements a simple Fortran family lexer. It is an
  /// extremely performance critical piece of code. This assumes that the buffer
  /// has a null character at the end of the file. It assumes that the Flags of
  /// result have been cleared before calling this.
  void LexTokenInternal(Token &Result, bool IsPeekAhead);

  /// FormTokenWithChars - When we lex a token, we have identified a span
  /// starting at CurPtr, going to TokEnd that forms the token. This method
  /// takes that range and assigns it to the token as its location and size. In
  /// addition, since tokens cannot overlap, this also updates CurPtr to be
  /// TokEnd.
  void FormTokenWithChars(Token &Result, tok::TokenKind Kind);

  /// FormDefinedOperatorTokenWithChars - A special form of
  /// FormTokenWithChars. It will see if the defined operator is an intrinsic
  /// operator. If so, it will set the token's kind to that value.
  void FormDefinedOperatorTokenWithChars(Token &Result);

  /// ReturnError - Set the error to the specified string at the specified
  /// location. This is defined to always return tok::error.
  tok::TokenKind ReturnError(const char *Loc, const std::string &Msg);

  Lexer(const Lexer&);          // DO NOT IMPLEMENT
  void operator=(const Lexer&); // DO NOT IMPLEMENT
public:

  /// Lexer constructor - Create a new lexer object. This lexer assumes that the
  /// text range will outlive it, so it doesn't take ownership of it.
  Lexer(llvm::SourceMgr &SM, const LangOptions &Features, DiagnosticsEngine &D);

  Lexer(llvm::SourceMgr &SM, const LangOptions &features, DiagnosticsEngine &D,
        SourceLocation StartingPoint);

  Lexer(const Lexer &TheLexer, SourceLocation StartingPoint);

  DiagnosticsEngine &getDiagnostics() const { return Diags; }

  const llvm::SourceMgr &getSourceManager() const { return SrcMgr; }

  SourceLocation getLoc() const;

  SourceLocation getLocEnd() const;

  /// getBufferPtr - Get a pointer to the next line to be lexed.
  const char* getBufferPtr() const { return Text.GetBufferPtr(); }

  void setBuffer(const llvm::MemoryBuffer *buf, const char *ptr = 0,
                 bool AtLineStart = true);

  /// \brief Add the specified comment handler to the preprocessor.
  void addCommentHandler(CommentHandler *Handler);

  /// \brief Remove the specified comment handler.
  ///
  /// It is an error to remove a handler that has not been registered.
  void removeCommentHandler(CommentHandler *Handler);

  // FIXME: CurKind isn't set.
  bool isa(tok::TokenKind Kind) const { return CurKind == Kind; }

  /// ReLexStatement - preparses the lexer for lexing the
  /// current statement from the start.
  void ReLexStatement(SourceLocation StmtStart);

  /// Lex - Return the next token in the file. If this is the end of file, it
  /// return the tok::eof token. Return true if an error occurred and
  /// compilation should terminate, false if normal.
  void Lex(Token &Result, bool IsPeekAhead = false) {
    // Start a new token.
    Result.startToken();

    // Get a token. Note that this may delete the current lexer if the end of
    // file is reached.
    LexTokenInternal(Result, IsPeekAhead);
  }

  /// LexFixedFormIdentifierMatchLongestKeyword -
  /// The lexer moves back to the location
  /// of the given token, and lexes the next identifier token as if it were
  /// two distinct identifiers. If the matcher matches a certain keyword,
  /// then the longest keyword match will be returned in the resulting token,
  /// otherwise the token returns the whole identifier.
  void LexFixedFormIdentifierMatchLongestKeyword(const fixedForm::KeywordMatcher &Matcher,
                                                 Token &Tok);

  /// LexFORMATToken - Return the next token in the file, with respect to the
  /// FORMAT token rules.
  void LexFORMATToken(Token &Result);

  /// getSpelling - Return the 'spelling' of the Tok token.  The spelling of a
  /// token is the characters used to represent the token in the source file.
  void getSpelling(const Token &Tok,
                   llvm::SmallVectorImpl<llvm::StringRef> &Spelling) const;

  /// getFixedFormIdentifierSpelling - Return the 'spelling' of the Tok token
  /// as determined by fixed-form identifier rules (i.e. whitespaces are ignored)
  void getFixedFormIdentifierSpelling(const Token &Tok,
                                      llvm::SmallVectorImpl<llvm::StringRef> &Spelling,
                                      const char *TokStart, unsigned TokLen) const;

  /// getFixedFormLiteralSpelling - Return the 'spelling' of the Tok token
  /// as determined by the fixed-form rules.
  void getFixedFormLiteralSpelling(const Token &Tok,
                                   llvm::SmallVectorImpl<llvm::StringRef> &Spelling,
                                   const char *TokStart, unsigned TokLen) const;

  /// PrintError - Error printing methods.
  void PrintError(const char *Loc, const std::string &Msg) const;
};

/// Lexes the format descriptor token.
class FormatDescriptorLexer {
protected:
  std::string Text;
  size_t Offset;
  SourceLocation TextLoc;
public:
  FormatDescriptorLexer(const Lexer &TheLexer,
                        const Token &FormatDescriptor);

  /// Returns the location of the next token.
  SourceLocation getCurrentLoc() const;

  /// returns true if the next token is an integer.
  bool IsIntPresent() const;

  /// Advances and returns true if an integer
  /// token is present.
  bool LexIntIfPresent(llvm::StringRef &Result);

  /// Advances and returns true if an identifier token
  /// is present.
  bool LexIdentIfPresent(llvm::StringRef& Result);

  /// Advances by one and returns true if the current char is c.
  bool LexCharIfPresent(char c);

  /// Returns true if there's no more characters left in the
  /// given string.
  bool IsDone() const;
};

/// \brief Abstract base class that describes a handler that will receive
/// source ranges for each of the comments encountered in the source file.
class CommentHandler {
public:
  virtual ~CommentHandler() {};

  // The handler shall return true if it has pushed any tokens
  // to be read using e.g. EnterToken or EnterTokenStream.
  virtual bool HandleComment(Lexer &Lexer, const SourceLocation& Loc, const llvm::StringRef &Comment) = 0;
};

} // end namespace flang

#endif
