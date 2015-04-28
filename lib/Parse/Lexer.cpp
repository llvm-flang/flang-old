//===-- Lexer.cpp - Fortran Lexer Implementation --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implementation of the Fortran lexer.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Lexer.h"
#include "flang/Parse/LexDiagnostic.h"
#include "flang/Parse/Parser.h"
#include "flang/Parse/FixedForm.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/Twine.h"

// FIXME: Errors from diag:: for BOZ literals

namespace flang {

static void InitCharacterInfo();
static bool isWhitespace(unsigned char c);
static bool isHorizontalWhitespace(unsigned char c);
static bool isHorizontalTab(unsigned char c);
static bool isVerticalWhitespace(unsigned char c);

Lexer::Lexer(llvm::SourceMgr &SM, const LangOptions &features, DiagnosticsEngine &D)
  : Text(D, features), Diags(D), SrcMgr(SM), Features(features), TokStart(0),
    LastTokenWasSemicolon(false) {
  InitCharacterInfo();
}

Lexer::Lexer(llvm::SourceMgr &SM, const LangOptions &features, DiagnosticsEngine &D,
      SourceLocation StartingPoint)
  : Text(D, features), Diags(D), SrcMgr(SM), Features(features), TokStart(0),
    LastTokenWasSemicolon(false) {
  assert(StartingPoint.isValid());
  setBuffer(SM.getMemoryBuffer(SM.FindBufferContainingLoc(StartingPoint)),
            StartingPoint.getPointer(), false);
}

Lexer::Lexer(const Lexer &TheLexer, SourceLocation StartingPoint)
  : Text(TheLexer.Diags, TheLexer.Features), Diags(TheLexer.Diags),
    SrcMgr(TheLexer.SrcMgr), Features(TheLexer.Features), TokStart(0),
    LastTokenWasSemicolon(false) {

  assert(StartingPoint.isValid());
  assert(StartingPoint.getPointer() >= TheLexer.CurBuf->getBufferStart() &&
         StartingPoint.getPointer() < TheLexer.CurBuf->getBufferEnd());
  setBuffer(TheLexer.CurBuf, StartingPoint.getPointer(), false);
}

void Lexer::setBuffer(const llvm::MemoryBuffer *Buf, const char *Ptr,
                      bool AtLineStart) {
  Text.SetBuffer(Buf, Ptr, AtLineStart);
  CurBuf = Buf;
  TokStart = 0;
}

SourceLocation Lexer::getLoc() const {
  return SourceLocation::getFromPointer(TokStart);
}

SourceLocation Lexer::getLocEnd() const {
  return SourceLocation::getFromPointer(getCurrentPtr());
}

void Lexer::addCommentHandler(CommentHandler *Handler) {
  assert(Handler && "NULL comment handler");
  assert(std::find(CommentHandlers.begin(), CommentHandlers.end(), Handler) ==
         CommentHandlers.end() && "Comment handler already registered");
  CommentHandlers.push_back(Handler);
}

void Lexer::removeCommentHandler(CommentHandler *Handler) {
  std::vector<CommentHandler *>::iterator Pos
  = std::find(CommentHandlers.begin(), CommentHandlers.end(), Handler);
  assert(Pos != CommentHandlers.end() && "Comment handler not registered");
  CommentHandlers.erase(Pos);
}

void Lexer::LineOfText::
SetBuffer(const llvm::MemoryBuffer *Buf, const char *Ptr, bool AtLineStart) {
  BufPtr = (Ptr ? Ptr : Buf->getBufferStart());
  CurAtom = CurPtr = 0;
  Atoms.clear();
  GetNextLine(AtLineStart);
}

Lexer::LineOfText::State Lexer::LineOfText::GetState() {
  State Result;
  Result.BufPtr = BufPtr;
  Result.CurAtom = CurAtom;
  Result.CurPtr = CurPtr;
  return Result;
}

void Lexer::LineOfText::SetState(const State &S) {
  BufPtr = S.BufPtr;
  CurAtom = S.CurAtom;
  CurPtr = S.CurPtr;
}

/// SkipBlankLinesAndComments - Helper function that skips blank lines and lines
/// with only comments.
bool Lexer::LineOfText::
SkipBlankLinesAndComments(unsigned &I, const char *&LineBegin, bool IgnoreContinuationChar) {
  // Skip blank lines and lines with only comments.
  while (isVerticalWhitespace(*BufPtr) && *BufPtr != '\0')
    ++BufPtr;

  while (I != 132 && isHorizontalWhitespace(*BufPtr) && *BufPtr != '\0')
    ++I, ++BufPtr;

  if (I != 132 && *BufPtr == '!') {
    do {
      ++BufPtr;
    } while (!isVerticalWhitespace(*BufPtr));

    while (isVerticalWhitespace(*BufPtr))
      ++BufPtr;

    // Save a pointer to the beginning of the line.
    LineBegin = BufPtr;
    I = 0;
    SkipBlankLinesAndComments(I, LineBegin);
  }

  // If we have a continuation character at the beginning of the line, and we've
  // had a previous continuation character at the end of the line, then readjust
  // the LineBegin.
  if (I != 132 && *BufPtr == '&') {
    if (Atoms.empty() && !IgnoreContinuationChar)
      Diags.Report(SourceLocation::getFromPointer(BufPtr),
                   diag::err_continuation_out_of_context);
    ++I, ++BufPtr;
    LineBegin = BufPtr;
    return true;
  }

  return false;
}

void Lexer::LineOfText::
SkipFixedFormBlankLinesAndComments(unsigned &I, const char *&LineBegin) {
  // Skip blank lines and lines with only comments.
  while (isVerticalWhitespace(*BufPtr) && *BufPtr != '\0')
    ++BufPtr;

  while (I != 72 && isHorizontalWhitespace(*BufPtr) && *BufPtr != '\0') {
    I += isHorizontalTab(*BufPtr)? LanguageOptions.TabWidth : 1;
    ++BufPtr;
  }

  if(I == 0 && (*BufPtr == 'C' || *BufPtr == 'c' || *BufPtr == '*')) {
    do {
      ++BufPtr;
    } while (!isVerticalWhitespace(*BufPtr));

    while (isVerticalWhitespace(*BufPtr))
      ++BufPtr;

    // Save a pointer to the beginning of the line.
    LineBegin = BufPtr;
    I = 0;
    SkipFixedFormBlankLinesAndComments(I, LineBegin);
  }
}

/// GetCharacterLiteral - A character literal has to be treated specially
/// because an ampersand may exist within it.
void Lexer::LineOfText::
GetCharacterLiteral(unsigned &I, const char *&LineBegin, bool &PadAtoms) {
  // Skip blank lines and lines with only comments.
  SkipBlankLinesAndComments(I, LineBegin);

  const char *AmpersandPos = 0;
  const char *QuoteStart = BufPtr;
  bool DoubleQuotes = (*BufPtr == '"');
  //A flag which ignores the next quote after a double quote continuation is found:
  // 'String'&\n&'' <-- the first ' on the second line needs this flag
  bool skipNextQuoteChar = false;
  ++I, ++BufPtr;
  while(true) {
    while (I != 132 && !isVerticalWhitespace(*BufPtr) && *BufPtr != '\0') {
      if (*BufPtr == '"' || *BufPtr == '\'') {
        if(!skipNextQuoteChar){
          char quoteChar = *BufPtr;
          ++I, ++BufPtr;

          //Allow the "/' character inside '/" character literal
          //Allow ''/"" character inside '/" character literal
          if (DoubleQuotes) {
            if (quoteChar == '\'')
              continue;
            if(I != 132 && *BufPtr == '"'){
              ++I,++BufPtr;
              continue;
            }
          } else {
            if (quoteChar == '"')
              continue;
            if(I != 132 && *BufPtr == '\''){
              ++I,++BufPtr;
              continue;
            }
          }

          //We need to verify that the next continued character is not ' or "
          //  in order for the character literal to terminate
          //  Example: 'String'&\n&'' should be (String') not (String) (')
          if(*BufPtr == '&'){
            unsigned currentI = I; const char* CurrentBufPtr = BufPtr,*CurrentLineBegin = LineBegin;

            I++, ++BufPtr;
            LineBegin = BufPtr;
            I = 0;
            SkipBlankLinesAndComments(I, LineBegin, true);
            char next = *BufPtr;
            bool endQuote = true;
            if(DoubleQuotes){
              if(next == '"'){
                assert(quoteChar == '"');
                endQuote = false;
              }
            } else {
              if(next == '\''){
                assert(quoteChar == '\'');
                endQuote = false;
              }
            }
            I = currentI;BufPtr = CurrentBufPtr; LineBegin = CurrentLineBegin;
            if(!endQuote){
              PadAtoms = false;
              skipNextQuoteChar = true;
              continue;
            }
          }


          return;
        } else
          skipNextQuoteChar = false; //Reset the flag
      }


      if (*BufPtr != '&') goto next_char;

      AmpersandPos = BufPtr;
      ++I, ++BufPtr;
      if (I == 132)
        break;
      while (I != 132 && isHorizontalWhitespace(*BufPtr) && *BufPtr != '\0')
        ++I, ++BufPtr;

      if (I == 132 || isVerticalWhitespace(*BufPtr) || *BufPtr == '\0')
        break;
      AmpersandPos = 0;

      next_char:
      ++I, ++BufPtr;
    }

    if (AmpersandPos)
      Atoms.push_back(StringRef(LineBegin, AmpersandPos - LineBegin));
    else {
      Diags.Report(SourceLocation::getFromPointer(QuoteStart),diag::err_unterminated_string);
      break;
    }

    LineBegin = BufPtr;
    I = 0;
    // Skip blank lines and lines with only comments.
    SkipBlankLinesAndComments(I, LineBegin);

    AmpersandPos = 0;
  }
}

const char *Lexer::LineOfText::Padding = " ";

/// GetNextLine - Get the next line of the program to lex.
void Lexer::LineOfText::GetNextLine(bool AtLineStart) {
  // Save a pointer to the beginning of the line.
  const char *LineBegin = BufPtr;

  // Fill the line buffer with the current line.
  unsigned I = 0;

  bool BeginsWithAmp = true;
  bool PadAtoms = true;

  const char *AmpersandPos = 0;
  if(LanguageOptions.FixedForm) {
    if(AtLineStart)
      SkipFixedFormBlankLinesAndComments(I, LineBegin);

    // Fixed form
    while (I != 72 && !isVerticalWhitespace(*BufPtr) && *BufPtr != '\0') {
      ++I, ++BufPtr;
    }
    Atoms.push_back(StringRef(LineBegin, BufPtr - LineBegin));

    // Increment the buffer pointer to the start of the next line.
    while (*BufPtr != '\0' && !isVerticalWhitespace(*BufPtr))
      ++BufPtr;
    while (*BufPtr != '\0' && isVerticalWhitespace(*BufPtr))
      ++BufPtr;

    I = 0;
    LineBegin = BufPtr;
    auto PrevBufPtr = BufPtr;
    SkipFixedFormBlankLinesAndComments(I, LineBegin);

    // this was a continuation line.
    bool IsContinuation = false;
    if(I == 5 && *BufPtr != '\0' && !isWhitespace(*BufPtr) && *BufPtr != '0') {
      ++I, ++BufPtr;
      IsContinuation = true;
    }

    if(IsContinuation)
      GetNextLine();
    else BufPtr = PrevBufPtr;
    return;
  } else {
    // Skip blank lines and lines with only comments.
    if(AtLineStart)
      BeginsWithAmp = SkipBlankLinesAndComments(I, LineBegin);
    // Free form
    while (I != 132 && !isVerticalWhitespace(*BufPtr) && *BufPtr != '\0') {
      if (*BufPtr == '\'' || *BufPtr == '"') {
        // TODO: A BOZ constant doesn't get parsed like a character literal.
        GetCharacterLiteral(I, LineBegin, PadAtoms);
        if (I == 132 || isVerticalWhitespace(*BufPtr))
          break;
      } else if (*BufPtr == '&') {
        AmpersandPos = BufPtr;
        do {
          ++I, ++BufPtr;
        } while (I != 132 && isHorizontalWhitespace(*BufPtr) && *BufPtr!='\0');

        // We should be either at the end of the line, at column 132, or at the
        // beginning of a comment. If not, the '&' is invalid. Report and ignore
        // it.
        if (I != 132 && !isVerticalWhitespace(*BufPtr) && *BufPtr != '!') {
          Diags.ReportError(SourceLocation::getFromPointer(AmpersandPos),
                            "continuation character not at end of line");
          AmpersandPos = 0;     // Pretend nothing's wrong.
        }

        if (*BufPtr == '!') {
          // Eat the comment after a continuation.
          while (!isVerticalWhitespace(*BufPtr) && *BufPtr != '\0')
            ++BufPtr;

          break;
        }

        if (I == 132 || isVerticalWhitespace(*BufPtr))
          break;
      } else if(*BufPtr == '!') {
        while (!isVerticalWhitespace(*BufPtr) && *BufPtr != '\0')
          ++BufPtr;
        break;
      }

      ++I, ++BufPtr;
    }
  }

  if (AmpersandPos) {
    Atoms.push_back(StringRef(LineBegin, AmpersandPos - LineBegin));
  } else {
    if (!BeginsWithAmp && !Atoms.empty() && PadAtoms)
      // This is a line that doesn't start with an '&'. The tokens are not
      // contiguous. Insert a space to indicate this.
      Atoms.push_back(StringRef(Padding));
    Atoms.push_back(StringRef(LineBegin, BufPtr - LineBegin));
  }

  // Increment the buffer pointer to the start of the next line.
  while (*BufPtr != '\0' && !isVerticalWhitespace(*BufPtr))
    ++BufPtr;
  while (*BufPtr != '\0' && isVerticalWhitespace(*BufPtr))
    ++BufPtr;

  if (AmpersandPos)
    GetNextLine();
}

char Lexer::LineOfText::GetNextChar() {
  StringRef Atom = Atoms[CurAtom];
  if (CurPtr + 1 >= Atom.size()) {
    if (CurAtom + 1 >= Atoms.size()) {
      if (CurPtr != Atom.size()) ++CurPtr;
      return '\0';
    }
    Atom = Atoms[++CurAtom];
    CurPtr = 0;
    return Atom.data()[CurPtr];
  }
  assert(!Atom.empty() && "Atom has no contents!");
  return Atom.data()[++CurPtr];
}

char Lexer::LineOfText::GetCurrentChar() const {
  StringRef Atom = Atoms[CurAtom];
  if (CurPtr == Atom.size())
    return '\0';
  assert(!Atom.empty() && "Atom has no contents!");
  return Atom.data()[CurPtr];
}

char Lexer::LineOfText::PeekNextChar() const {
  StringRef Atom = Atoms[CurAtom];
  if (CurPtr + 1 == Atom.size()) {
    if (CurAtom + 1 == Atoms.size())
      return '\0';
    return Atoms[CurAtom + 1][0];
  }
  assert(!Atom.empty() && "Atom has no contents!");
  return Atom.data()[CurPtr + 1];
}

char Lexer::LineOfText::PeekNextChar(int Offset) const {
  auto CurrentAtom = CurAtom;
  auto Ptr = CurPtr;
  while(Ptr + Offset >= Atoms[CurrentAtom].size() && Offset > 0) {
    Offset -= (Atoms[CurrentAtom].size() - Ptr);
    Ptr = 0;
    ++CurrentAtom;
    if(CurrentAtom >= Atoms.size())
      return '\0';
  }
  StringRef Atom = Atoms[CurrentAtom];
  assert(!Atom.empty() && "Atom has no contents!");
  return Atom.data()[Ptr + Offset];
}

char Lexer::LineOfText::PeekPrevChar() const {
  if (Atoms.empty()) return '\0';
  StringRef Atom = Atoms[CurAtom];
  if (CurPtr == 0) {
    if (CurAtom == 0)
      return '\0';
    return Atoms[CurAtom - 1].back();
  }
  assert(!Atom.empty() && "Atom has no contents!");
  return Atom.data()[CurPtr - 1];
}

void Lexer::LineOfText::dump() const {
  dump(llvm::errs());
}

void Lexer::LineOfText::dump(raw_ostream &OS) const {
  for (SmallVectorImpl<StringRef>::const_iterator
         I = Atoms.begin(), E = Atoms.end(); I != E; ++I)
    OS << *I;
  OS << '\n';
}

//===----------------------------------------------------------------------===//
// Character information.
//===----------------------------------------------------------------------===//

enum {
  CHAR_HORZ_WS  = 0x01,  // ' ', '\t', '\f', '\v'.  Note, no '\0'
  CHAR_VERT_WS  = 0x02,  // '\r', '\n'
  CHAR_LETTER   = 0x04,  // a-z,A-Z
  CHAR_NUMBER   = 0x08,  // 0-9
  CHAR_UNDER    = 0x10,  // _
  CHAR_PERIOD   = 0x20,  // .
  CHAR_HEX      = 0x40   // [a-fA-F]
};

// Statically initialize CharInfo table based on ASCII character set
// Reference: FreeBSD 7.2 /usr/share/misc/ascii
static const unsigned char CharInfo[256] = {
// 0 NUL         1 SOH         2 STX         3 ETX
// 4 EOT         5 ENQ         6 ACK         7 BEL
   0           , 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
// 8 BS          9 HT         10 NL         11 VT
//12 NP         13 CR         14 SO         15 SI
   0           , CHAR_HORZ_WS, CHAR_VERT_WS, CHAR_HORZ_WS,
   CHAR_HORZ_WS, CHAR_VERT_WS, 0           , 0           ,
//16 DLE        17 DC1        18 DC2        19 DC3
//20 DC4        21 NAK        22 SYN        23 ETB
   0           , 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
//24 CAN        25 EM         26 SUB        27 ESC
//28 FS         29 GS         30 RS         31 US
   0           , 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
//32 SP         33  !         34  "         35  #
//36  $         37  %         38  &         39  '
   CHAR_HORZ_WS, 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
//40  (         41  )         42  *         43  +
//44  ,         45  -         46  .         47  /
   0           , 0           , 0           , 0           ,
   0           , 0           , CHAR_PERIOD , 0           ,
//48  0         49  1         50  2         51  3
//52  4         53  5         54  6         55  7
   CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER ,
   CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER ,
//56  8         57  9         58  :         59  ;
//60  <         61  =         62  >         63  ?
   CHAR_NUMBER , CHAR_NUMBER , 0           , 0           ,
   0           , 0           , 0           , 0           ,
//64  @
   0           ,
//65  A
   (CHAR_HEX | CHAR_LETTER),
//66  B
   (CHAR_HEX | CHAR_LETTER),
//67  C
   (CHAR_HEX | CHAR_LETTER),
//68  D
   (CHAR_HEX | CHAR_LETTER),
//69  E
   (CHAR_HEX | CHAR_LETTER),
//70  F
   (CHAR_HEX | CHAR_LETTER),
//71  G
   CHAR_LETTER ,
//72  H         73  I         74  J         75  K
//76  L         77  M         78  N         79  O
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//80  P         81  Q         82  R         83  S
//84  T         85  U         86  V         87  W
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//88  X         89  Y         90  Z         91  [
//92  \         93  ]         94  ^         95  _
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , 0           ,
   0           , 0           , 0           , CHAR_UNDER  ,
//96  `
   0           ,
//97  a
   (CHAR_HEX | CHAR_LETTER),
//98  b
   (CHAR_HEX | CHAR_LETTER),
//99  c
   (CHAR_HEX | CHAR_LETTER),
//100  d
   (CHAR_HEX | CHAR_LETTER),
//101  e
   (CHAR_HEX | CHAR_LETTER),
//102  f
   (CHAR_HEX | CHAR_LETTER),
//103  g
   CHAR_LETTER ,
//104  h        105  i        106  j        107  k
//108  l        109  m        110  n        111  o
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//112  p        113  q        114  r        115  s
//116  t        117  u        118  v        119  w
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//120  x        121  y        122  z        123  {
//124  |        125  }        126  ~        127 DEL
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , 0           ,
   0           , 0           , 0           , 0
};

static void InitCharacterInfo() {
  static bool isInited = false;
  if (isInited) return;

  // Check the statically-initialized CharInfo table
  assert(CHAR_HORZ_WS == CharInfo[(int)' ']);
  assert(CHAR_HORZ_WS == CharInfo[(int)'\t']);
  assert(CHAR_HORZ_WS == CharInfo[(int)'\f']);
  assert(CHAR_HORZ_WS == CharInfo[(int)'\v']);
  assert(CHAR_VERT_WS == CharInfo[(int)'\n']);
  assert(CHAR_VERT_WS == CharInfo[(int)'\r']);
  assert(CHAR_UNDER   == CharInfo[(int)'_']);
  assert(CHAR_PERIOD  == CharInfo[(int)'.']);

  for (unsigned i = 'a'; i <= 'f'; ++i) {
    assert((CHAR_LETTER|CHAR_HEX) == CharInfo[i]);
    assert((CHAR_LETTER|CHAR_HEX) == CharInfo[i+'A'-'a']);
  }

  for (unsigned i = 'g'; i <= 'z'; ++i) {
    assert(CHAR_LETTER == CharInfo[i]);
    assert(CHAR_LETTER == CharInfo[i+'A'-'a']);
  }

  for (unsigned i = '0'; i <= '9'; ++i)
    assert(CHAR_NUMBER == CharInfo[i]);

  isInited = true;
}

/// isIdentifierBody - Return true if this is the body character of an
/// identifier, which is [a-zA-Z0-9_].
static inline bool isIdentifierBody(unsigned char c) {
  return (CharInfo[c] & (CHAR_LETTER | CHAR_NUMBER | CHAR_UNDER)) ?
    true : false;
}

/// isFormatDescriptorBody - Return true if this is the body character of a
/// descriptor in a FORMAT statement, which is [a-zA-Z0-9.].
static inline bool isFormatDescriptorBody(unsigned char c) {
  return (CharInfo[c] & (CHAR_LETTER | CHAR_NUMBER | CHAR_PERIOD)) ?
    true : false;
}

/// isLetter - Return true if this is a letter, which is [a-zA-Z].
static inline bool isLetter(unsigned char c) {
  return (CharInfo[c] & CHAR_LETTER) ? true : false;
}

/// isHorizontalWhitespace - Return true if this character is horizontal
/// whitespace: ' ', '\t', '\f', '\v'.  Note that this returns false for '\0'.
static inline bool isHorizontalWhitespace(unsigned char c) {
  return (CharInfo[c] & CHAR_HORZ_WS) ? true : false;
}

/// isHorizontalTab - Return true if this character is horizontal tab.
static inline bool isHorizontalTab(unsigned char c) {
  return c == '\t';
}

/// isVerticalWhitespace - Return true if this character is vertical whitespace:
/// '\n', '\r'.  Note that this returns false for '\0'.
static inline bool isVerticalWhitespace(unsigned char c) {
  return (CharInfo[c] & CHAR_VERT_WS) ? true : false;
}

/// isWhitespace - Return true if this character is horizontal or vertical
/// whitespace: ' ', '\t', '\f', '\v', '\n', '\r'.  Note that this returns false
/// for '\0'.
static inline bool isWhitespace(unsigned char c) {
  return (CharInfo[c] & (CHAR_HORZ_WS | CHAR_VERT_WS)) ? true : false;
}

/// isNumberBody - Return true if this is the body character of a number, which
/// is [0-9_.].
static inline bool isNumberBody(unsigned char c) {
  return (CharInfo[c] & (CHAR_NUMBER | CHAR_UNDER | CHAR_PERIOD)) ?
    true : false;
}

/// isBinaryNumberBody - Return true if this is the body character of a binary
/// number, which is [01].
static inline bool isBinaryNumberBody(unsigned char c) {
  return (c == '0' | c == '1') ? true : false;
}

/// isOctalNumberBody - Return true if this is the body character of an octal
/// number, which is [0-7].
static inline bool isOctalNumberBody(unsigned char c) {
  return (c >= '0' & c <= '7') ? true : false;
}

/// isDecimalNumberBody - Return true if this is the body character of a decimal
/// number, which is [0-9].
static inline bool isDecimalNumberBody(unsigned char c) {
  return (CharInfo[c] & CHAR_NUMBER) ? true : false;
}

/// isHexNumberBody - Return true if this is the body character of a hexadecimal
/// number, which is [0-9a-fA-F].
static inline bool isHexNumberBody(unsigned char c) {
  return (CharInfo[c] & (CHAR_NUMBER | CHAR_HEX)) ? true : false;
}

//===----------------------------------------------------------------------===//
// Token Spelling
//===----------------------------------------------------------------------===//

/// getSpelling() - Return the 'spelling' of this token.  The spelling of a
/// token are the characters used to represent the token in the source file.
void Lexer::getSpelling(const Token &Tok,
                        llvm::SmallVectorImpl<llvm::StringRef> &Spelling) const{
  assert((int)Tok.getLength() >= 0 && "Token character range is bogus!");

  const char *TokStart = Tok.isLiteral() ?
    Tok.getLiteralData() : Tok.getLocation().getPointer();
  unsigned TokLen = Tok.getLength();

  // If this token contains nothing interesting, return it directly.
  if (!Tok.needsCleaning())
    return Spelling.push_back(llvm::StringRef(TokStart, TokLen));

  if(Text.LanguageOptions.FixedForm) {
    if(Tok.isLiteral()) {
      getFixedFormLiteralSpelling(Tok, Spelling, TokStart, TokLen);
      return;
    }
    getFixedFormIdentifierSpelling(Tok, Spelling, TokStart, TokLen);
    return;
  }

  const char *CurPtr = TokStart;
  const char *Start = TokStart;
  unsigned Len = 0;

  while (true) {
    while (Len != TokLen) {
      if (*CurPtr != '&') {
        ++CurPtr, ++Len;
        continue;
      }
      if (Tok.isNot(tok::char_literal_constant))
        break;
      const char *TmpPtr = CurPtr + 1;
      unsigned TmpLen = Len + 1;
      while (TmpLen != TokLen && isHorizontalWhitespace(*TmpPtr))
        ++TmpPtr, ++TmpLen;
      if (*TmpPtr == '\n' || *TmpPtr == '\r')
        break;
      CurPtr = TmpPtr;
      Len = TmpLen;
    }

    Spelling.push_back(llvm::StringRef(Start, CurPtr - Start));

    if (*CurPtr != '&' || Len >= TokLen)
      break;

    Start = ++CurPtr; ++Len;

    if (Len >= TokLen)
      break;

    while (true) {
      // Skip blank lines...
      while (Len != TokLen && isWhitespace(*CurPtr))
        ++CurPtr, ++Len;

      if (*CurPtr != '!')
        break;

      // ...and lines with only comments.
      while (Len != TokLen && *CurPtr != '\n' && *CurPtr != '\r')
        ++CurPtr, ++Len;
    }

    if (*CurPtr != '&' || Len >= TokLen)
      break;

    Start = ++CurPtr; ++Len;
  }
}

void  Lexer::getFixedFormIdentifierSpelling(const Token &Tok,
                                            llvm::SmallVectorImpl<llvm::StringRef> &Spelling,
                                            const char *TokStart,
                                            unsigned TokLen) const {
  const char *CurPtr = TokStart;
  const char *Start = TokStart;
  unsigned Len = 0;

  while (true) {
    while (Len != TokLen) {
      if(!isHorizontalWhitespace(*CurPtr) &&
         !isVerticalWhitespace(*CurPtr)) {
        ++CurPtr, ++Len;
        continue;
      }
      break;
    }

    Spelling.push_back(llvm::StringRef(Start, CurPtr - Start));

    if(!isVerticalWhitespace(*CurPtr)) {
      // Skip spaces
      while (Len != TokLen && isHorizontalWhitespace(*CurPtr))
        ++CurPtr, ++Len;
      if(Len >= TokLen)
        break;
      Start = CurPtr;
      continue;
    }

    if (Len >= TokLen)
      break;
    Start = ++CurPtr; ++Len;

    while (true) {
      // Skip blank lines...
      while (Len != TokLen && isWhitespace(*CurPtr))
        ++CurPtr, ++Len;

      if (*CurPtr != 'C' && *CurPtr != 'c' && *CurPtr != '*')
        break;

      // ...and lines with only comments.
      while (Len != TokLen && *CurPtr != '\n' && *CurPtr != '\r')
        ++CurPtr, ++Len;
    }

    if(Len >= TokLen)
      break;

    Start = ++CurPtr; ++Len;
  }
}

void  Lexer::getFixedFormLiteralSpelling(const Token &Tok,
                                         llvm::SmallVectorImpl<llvm::StringRef> &Spelling,
                                         const char *TokStart,
                                         unsigned TokLen) const {
  const char *CurPtr = TokStart;
  const char *Start = TokStart;
  unsigned Len = 0;

  while (true) {
    while (Len != TokLen) {
      if(!isVerticalWhitespace(*CurPtr)) {
        ++CurPtr, ++Len;
        continue;
      }
      break;
    }

    Spelling.push_back(llvm::StringRef(Start, CurPtr - Start));

    if (Len >= TokLen)
      break;
    Start = ++CurPtr; ++Len;

    while (true) {
      // Skip blank lines...
      while (Len != TokLen && isWhitespace(*CurPtr))
        ++CurPtr, ++Len;

      if (*CurPtr != 'C' && *CurPtr != 'c' && *CurPtr != '*')
        break;

      // ...and lines with only comments.
      while (Len != TokLen && *CurPtr != '\n' && *CurPtr != '\r')
        ++CurPtr, ++Len;
    }

    if(Len >= TokLen)
      break;

    Start = ++CurPtr; ++Len;
  }
}

//===----------------------------------------------------------------------===//
// Helper methods for lexing.
//===----------------------------------------------------------------------===//

/// FormTokenWithChars - When we lex a token, we have identified a span starting
/// at the current pointer, going to TokEnd that forms the token. This method
/// takes that range and assigns it to the token as its location and size. In
/// addition, since tokens cannot overlap.
void Lexer::FormTokenWithChars(Token &Result, tok::TokenKind Kind) {
  uint64_t TokLen = getCurrentPtr() - TokStart;
  CurKind = Kind;
  Result.setLocation(SourceLocation::getFromPointer(TokStart));
  Result.setLength(TokLen);
  Result.setKind(Kind);

  if (!Text.IsInCurrentAtom(TokStart))
    Result.setFlag(Token::NeedsCleaning);
}

/// FormDefinedOperatorTokenWithChars - A special form of FormTokenWithChars. It
/// will see if the defined operator is an intrinsic operator. If so, it will
/// set the token's kind to that value.
void Lexer::FormDefinedOperatorTokenWithChars(Token &Result) {
  std::string CleanedOp;
  unsigned TokLen;
  llvm::StringRef FullOp;

  if (!Text.IsInCurrentAtom(TokStart) || Result.needsCleaning()) {
    llvm::SmallVector<llvm::StringRef, 2> Spelling;
    FormTokenWithChars(Result, tok::unknown);
    getSpelling(Result, Spelling);
    for(auto Part: Spelling)
      CleanedOp += Part;
    FullOp = CleanedOp;
    TokLen = Result.getLength();
  } else {
    TokLen = getCurrentPtr() - TokStart;
    FullOp = llvm::StringRef(TokStart, TokLen);
  }
  assert(TokLen >= 2 && "Malformed defined operator!");

  if (TokLen - 2 > 63){
    Diags.Report(SourceLocation::getFromPointer(TokStart),
                 diag::err_defined_operator_too_long);
    return FormTokenWithChars(Result, tok::unknown);
  }
  size_t Under = FullOp.find('_');
  llvm::StringRef Op = FullOp;

  if (Under != llvm::StringRef::npos)
    Op = FullOp.substr(0, Under);

  tok::TokenKind Kind = tok::defined_operator;

  if (Op.compare_lower(".TRUE.") == 0 || Op.compare_lower(".FALSE.") == 0){
    FormTokenWithChars(Result, tok::logical_literal_constant);
    Result.setLiteralData(TokStart);
    return;
  }
  else if (Op.compare_lower(".EQ.") == 0)
    Kind = tok::kw_EQ;
  else if (Op.compare_lower(".NE.") == 0)
    Kind = tok::kw_NE;
  else if (Op.compare_lower(".LT.") == 0)
    Kind = tok::kw_LT;
  else if (Op.compare_lower(".LE.") == 0)
    Kind = tok::kw_LE;
  else if (Op.compare_lower(".GT.") == 0)
    Kind = tok::kw_GT;
  else if (Op.compare_lower(".GE.") == 0)
    Kind = tok::kw_GE;
  else if (Op.compare_lower(".NOT.") == 0)
    Kind = tok::kw_NOT;
  else if (Op.compare_lower(".AND.") == 0)
    Kind = tok::kw_AND;
  else if (Op.compare_lower(".OR.") == 0)
    Kind = tok::kw_OR;
  else if (Op.compare_lower(".EQV.") == 0)
    Kind = tok::kw_EQV;
  else if (Op.compare_lower(".NEQV.") == 0)
    Kind = tok::kw_NEQV;

  return FormTokenWithChars(Result, Kind);
}

/// LexIdentifier - Lex the remainder of an identifier.
void Lexer::LexIdentifier(Token &Result) {
  if(Features.FixedForm)
    return LexFixedFormIdentifier(Result);

  // Match [_A-Za-z0-9]*, we have already matched [A-Za-z$]
  unsigned char C = getCurrentChar();

  while (isIdentifierBody(C))
    C = getNextChar();

  // We let the parser determine what type of identifier this is: identifier,
  // keyword, or built-in function.
  FormTokenWithChars(Result, tok::identifier);
}

/// LexFixedFormIdentifier - Lex a fixed form identifier token.
void Lexer::LexFixedFormIdentifier(Token &Result) {
  bool NeedsCleaning = false;
  unsigned char C = getCurrentChar();
  while (!Text.empty() && !Text.AtEndOfLine()) {
    if(!isIdentifierBody(C)){
      if(!isHorizontalWhitespace(C))
        break;
      NeedsCleaning = true;
    }
    C = getNextChar();
  }
  FormTokenWithChars(Result, tok::identifier);
  if(NeedsCleaning)
    Result.setFlag(Token::NeedsCleaning);
}

void Lexer::LexFixedFormIdentifier(const fixedForm::KeywordMatcher &Matcher,
                                   Token &Tok) {
  LineOfText::State LastMatchedIdState;
  bool MatchedId = false;
  bool NeedsCleaning = false;

  llvm::SmallVector<char, 64> Token;
  auto C = getCurrentChar();
  while(!Text.empty() && !Text.AtEndOfLine()) {
    C = getCurrentChar();
    if(!isIdentifierBody(C)) {
      if(isHorizontalWhitespace(C))
        NeedsCleaning = true;
      else break;
    } else Token.push_back(C);

    getNextChar();

    if(Matcher.Matches(StringRef(Token.data(),
                                 Token.size()))) {
      LastMatchedIdState = Text.GetState();
      MatchedId = true;
    }
  }

  if(MatchedId) Text.SetState(LastMatchedIdState);
  FormTokenWithChars(Tok, tok::identifier);
  if(NeedsCleaning)
    Tok.setFlag(Token::NeedsCleaning);
}

bool Lexer::LexPossibleDefinedOperator(Token &Result) {
  auto Char = getNextChar();
  if (Features.FixedForm) {
    while(isHorizontalWhitespace(Char)) {
      Result.setFlag(Token::NeedsCleaning);
      Char = getNextChar();
    }
  }
  if (!isLetter(Char))
    return false;
  // Match [A-Za-z]*, we have already matched '.'.
  if(Features.FixedForm) {
    for(;;) {
      Char = getNextChar();
      if(!isLetter(Char)) {
        if(!isHorizontalWhitespace(Char))
          break;
        Result.setFlag(Token::NeedsCleaning);
      }
    }
  } else {
    while (isLetter(Char))
      Char = getNextChar();
  }

  if (Char != '.') {
    Diags.Report(SourceLocation::getFromPointer(TokStart), diag::err_defined_operator_missing_end);
    FormTokenWithChars(Result, tok::unknown);
    return true;
  }

  // FIXME: fixed-form
  Char = getNextChar();
  if (Char == '_') {
    // Parse the kind.
    do {
      Char = getNextChar();
    } while (isIdentifierBody(Char) || isDecimalNumberBody(Char));
  }

  FormDefinedOperatorTokenWithChars(Result);
  return true;
}

/// LexFormatDescriptor - Lex the remainder of a format descriptor.
void Lexer::LexFORMATDescriptor(Token &Result) {
  // Match [_A-Za-z0-9]*, we have already matched [A-Za-z$]
  unsigned char C = getCurrentChar();

  while (isFormatDescriptorBody(C))
    C = getNextChar();

  FormTokenWithChars(Result, tok::format_descriptor);
}

/// LexStatementLabel - Lex the remainder of a statement label -- a 5-digit
/// number.
void Lexer::LexStatementLabel(Token &Result) {
  char C = getNextChar();

  while (isDecimalNumberBody(C))
    C = getNextChar();

  // Update the location of token.
  FormTokenWithChars(Result, tok::statement_label);
  Result.setLiteralData(TokStart);
}

/// LexIntegerLiteralConstant - Lex an integer literal constant.
///
///   [R406]:
///     signed-int-literal-constant :=
///         [ sign ] int-literal-constant
///   [R407]:
///     int-literal-constant :=
///         digit-string [ _kind-param ]
///   [R410]:
///     digit-string :=
///         digit [ digit ] ...
bool Lexer::LexIntegerLiteralConstant() {
  bool IntPresent = false;
  char C = getCurrentChar();
  if (C == '-' || C == '+')
    C = getNextChar();

  while (isDecimalNumberBody(C)) {
    IntPresent = true;
    C = getNextChar();
  }

  return IntPresent;
}

/// LexNumericConstant - Lex an integer or floating point constant.
void Lexer::LexNumericConstant(Token &Result) {
  bool IsReal = false;
  bool IsDoublePrecision = false;
  bool IsExp = false;

  const char *NumBegin = TokStart;
  bool BeginsWithDot = (*NumBegin == '.');
  if (!LexIntegerLiteralConstant() && BeginsWithDot) {
    Diags.ReportError(SourceLocation::getFromPointer(NumBegin),
                      "invalid REAL literal");
    FormTokenWithChars(Result, tok::error);
    return;
  }

  char C;
  if(!BeginsWithDot) {

    char PrevChar = getCurrentChar();
    if (PrevChar == '.') {
      char C = peekNextChar();
      if(isDecimalNumberBody(C)) {
        getNextChar();
        IsReal = true;
        if (LexIntegerLiteralConstant())
          PrevChar = '\0';
      }
      else if(C == 'E' || C == 'e' || C == 'D' || C == 'd') {
        if(!isLetter(peekNextChar(2))) {
          getNextChar();
          IsReal = true;
          IsExp = true;
        }
      } else if(!isLetter(C)) {
       getNextChar();
       IsReal = true;
      }
    }

    // Could be part of a defined operator. Form numeric constant from what we now
    // have.
    C = getCurrentChar();
    if (!IsExp && PrevChar == '.' && isLetter(C)) {
      C = getCurrentChar();
      if (isLetter(C)) {
        if (!BeginsWithDot)
          IsReal = false;
        goto make_literal;
      }
    }
  } else {
    IsReal = true;
    C = getCurrentChar();
  }

  if (C == 'E' || C == 'e' || C == 'D' || C == 'd') {
    IsReal = true;
    if(C == 'D' || C == 'd') IsDoublePrecision = true;
    C = getNextChar();
    if (C == '-' || C == '+')
      C = getNextChar();
    if (!isDecimalNumberBody(C)) {
      Diags.Report(SourceLocation::getFromPointer(NumBegin),
                   diag::err_exponent_has_no_digits);
      FormTokenWithChars(Result, tok::error);
      return;
    }
    LexIntegerLiteralConstant();
  }

  if (C == '_')
    do {
      C = getNextChar();
  } while (isIdentifierBody(C) || isDecimalNumberBody(C));

  // Update the location of token.
 make_literal:
  if (!IsReal)
    FormTokenWithChars(Result, tok::int_literal_constant);
  else
    FormTokenWithChars(Result, IsDoublePrecision? tok::double_precision_literal_constant:
                                                  tok::real_literal_constant);
  Result.setLiteralData(TokStart);
}

/// LexCharacterLiteralConstant - Lex the remainder of a character literal
/// constant (string).
void Lexer::LexCharacterLiteralConstant(Token &Result,
                                        bool DoubleQuotes) {
  while (true) {
    char C = getNextChar();
    if (C == '\0') break;

    if (DoubleQuotes) {
      if (C == '"') {
        if (peekNextChar() != '"') {
          getNextChar();
          break;
        }
        C = getNextChar();
      }
    } else {
      if (C == '\'') {
        if (peekNextChar() != '\'') {
          getNextChar();
          break;
        }
        C = getNextChar();
      }
    }
  }

  // Update the location of token.
  FormTokenWithChars(Result, tok::char_literal_constant);
  Result.setLiteralData(TokStart);
}

/// LexComment - Lex a comment and return it, why not?
void Lexer::LexComment(Token &Result) {
  char C;
  do {
    C = getNextChar();
  } while (C != '\0');

  FormTokenWithChars(Result, tok::comment);
  Result.setLiteralData(TokStart);
  for(std::vector<CommentHandler*>::const_iterator I = CommentHandlers.begin();
      I != CommentHandlers.end(); ++I) {
    (*I)->HandleComment(*this, Result.getLocation(),
                        llvm::StringRef(Result.getLiteralData(),Result.getLength()));
  }
}

void Lexer::ReLexStatement(SourceLocation StmtStart) {
  LastTokenWasSemicolon = true;
  setBuffer(CurBuf, StmtStart.getPointer(), false);
}

/// LexTokenInternal - This implements a simple Fortran family lexer. It is an
/// extremely performance critical piece of code. This assumes that the buffer
/// has a null character at the end of the file. It assumes that the Flags of
/// Result have been cleared before calling this.
void Lexer::LexTokenInternal(Token &Result, bool IsPeekAhead) {
  // Check to see if there is still more of the line to lex.
  if (Text.empty() || Text.AtEndOfLine()) {
    if(IsPeekAhead) {
      Result.setKind(tok::unknown);
      return;
    }
    Text.Reset();
    Text.GetNextLine();
  }

  // Check to see if we're at the start of a line.
  if (getLineBegin() == getCurrentPtr())
    // The returned token is at the start of the line.
    Result.setFlag(Token::StartOfStatement);

  // If we saw a semicolon, then we're at the start of a new statement.
  if (LastTokenWasSemicolon) {
    LastTokenWasSemicolon = false;
    Result.setFlag(Token::StartOfStatement);
  }

  // Small amounts of horizontal whitespace is very common between tokens.
  char Char = getCurrentChar();
  while (isHorizontalWhitespace(Char))
    Char = getNextChar();

  TokStart = getCurrentPtr();
  tok::TokenKind Kind;

  switch (Char) {
  case 0:  // Null.
    // Found end of file?
    if (getCurrentPtr() >= CurBuf->getBufferEnd()) {
      Kind = tok::eof;
      break;
    }

    getNextChar();
    return LexTokenInternal(Result, IsPeekAhead);
  case '\n':
  case '\r':
  case ' ':
  case '\t':
  case '\f':
  case '\v':
    do {
      Char = getNextChar();
    } while (isHorizontalWhitespace(Char));
    return LexTokenInternal(Result, IsPeekAhead);

  case '.':
    if(LexPossibleDefinedOperator(Result))
      return;
    // FALLTHROUGH
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    // [TODO]: Kinds on literals.
    if (Result.isAtStartOfStatement()) {
      return LexStatementLabel(Result);
    }
    return LexNumericConstant(Result);

  case '"':
  case '\'':
    // [TODO]: Kinds.
    return LexCharacterLiteralConstant(Result, Char == '"'); 

  // [TODO]: BOZ literals.
  case 'B': case 'b':
    if (Char == '"' || Char == '\'') { // No whitespace between B and quote.
      // Possible binary constant: B'...', B"..."
      const char *BOZBegin = getCurrentPtr();
      bool DoubleQuote = (Char == '"');

      do {
        Char = getNextChar();
      } while (isBinaryNumberBody(Char));

      if (getCurrentPtr() - TokStart == 2) {
        Diags.ReportError(SourceLocation::getFromPointer(BOZBegin),
                          "no binary digits for BOZ constant");
        FormTokenWithChars(Result, tok::error);
        return;
      }

      if ((DoubleQuote && Char != '"') || Char != '\'') {
        Diags.ReportError(SourceLocation::getFromPointer(BOZBegin),
                          "binary BOZ constant missing ending quote");
        FormTokenWithChars(Result, tok::error);
        return;
      }

      // Update the location of token.
      FormTokenWithChars(Result, tok::binary_boz_constant);
      Result.setLiteralData(TokStart);
      return;
    }

    goto LexIdentifier;
  case 'O': case 'o':
    if (Char == '"' || Char == '\'') { // No whitespace between O and quote.
      // Possible octal constant: O'...', O"..."
      const char *BOZBegin = getCurrentPtr();
      bool DoubleQuote = (Char == '"');

      do {
        Char = getNextChar();
      } while (isOctalNumberBody(Char));

      if (getCurrentPtr() - TokStart == 2) {
        Diags.ReportError(SourceLocation::getFromPointer(BOZBegin),
                          "no octal digits for BOZ constant");
        FormTokenWithChars(Result, tok::error);
        return;
      }

      if ((DoubleQuote && Char != '"') || Char != '\'') {
        Diags.ReportError(SourceLocation::getFromPointer(BOZBegin),
                          "octal BOZ constant missing ending quote");
        FormTokenWithChars(Result, tok::unknown);
        return;
      }

      // Update the location of token.
      FormTokenWithChars(Result, tok::octal_boz_constant);
      Result.setLiteralData(TokStart);
      return;
    }

    goto LexIdentifier;
  case 'X': case 'x':
  case 'Z': case 'z':
    if (Char == '"' || Char == '\'') { // No whitespace between Z and quote.
      // Possible hexadecimal constant: Z'...', Z"..."
      const char *BOZBegin = getCurrentPtr();
      bool DoubleQuote = (Char == '"');

      do {
        Char = getNextChar();
      } while (isHexNumberBody(Char));

      if (getCurrentPtr() - TokStart == 2) {
        Diags.ReportError(SourceLocation::getFromPointer(BOZBegin),
                          "no hex digits for BOZ constant");
        FormTokenWithChars(Result, tok::unknown);
        return;
      }

      if ((DoubleQuote && Char != '"') || Char != '\'') {
        Diags.ReportError(SourceLocation::getFromPointer(BOZBegin),
                          "hex BOZ constant missing ending quote");
        FormTokenWithChars(Result, tok::unknown);
        return;
      }

      // Update the location of token.
      FormTokenWithChars(Result, tok::hex_boz_constant);
      Result.setLiteralData(TokStart);
      return;
    }

    goto LexIdentifier;
  case 'A': /* 'B' */ case 'C': case 'D': case 'E': case 'F': case 'G':
  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
  /* 'O' */ case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
  case 'V': case 'W': /* 'X' */ case 'Y': /* 'Z' */
  case 'a': /* 'b' */ case 'c': case 'd': case 'e': case 'f': case 'g':
  case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
  /* 'o' */ case 'p': case 'q': case 'r': case 's': case 't': case 'u':
  case 'v': case 'w': /* 'x' */ case 'y': /* 'z' */
LexIdentifier:
    if(IsPeekAhead) {
      Result.setKind(tok::unknown);
      return;
    }
    return LexIdentifier(Result);

  case '!':
    LexComment(Result);
    if (Features.ReturnComments)
      return;
    return LexTokenInternal(Result, IsPeekAhead);

  // [TODO]: Special Characters.
  case '[':
    Kind = tok::l_square;
    break;
  case ']':
    Kind = tok::r_square;
    break;
  case '(':
    Char = peekNextChar();
    if (Char == '/') {
      // beginning of array initialization.
      Kind = tok::l_parenslash;
      Char = getNextChar();
    } else {
      Kind = tok::l_paren;
    }
    break;
  case ')':
    Kind = tok::r_paren;
    break;
  case '{':
    Kind = tok::l_brace;
    break;
  case '}':
    Kind = tok::r_brace;
    break;
  case ',':
    Kind = tok::comma;
    break;
  case ':':
    Char = peekNextChar();
    if (Char == ':') {
      Kind = tok::coloncolon;
      Char = getNextChar();
    } else {
      Kind = tok::colon;
    }
    break;
  case ';':
    LastTokenWasSemicolon = true;
    return LexTokenInternal(Result, IsPeekAhead);
  case '%':
    Kind = tok::percent;
    break;
  case '~':
    Kind = tok::tilde;
    break;
  case '?':
    Kind = tok::question;
    break;
  case '`':
    Kind = tok::backtick;
    break;
  case '^':
    Kind = tok::caret;
    break;
  case '|':
    Kind = tok::pipe;
    break;
  case '$':
    Kind = tok::dollar;
    break;
  case '#':
    Kind = tok::hash;
    break;
  case '@':
    Kind = tok::at;
    break;
  // [TODO]: Arithmetical Operators
  case '+':
    Kind = tok::plus;
    break;
  case '-':
    Kind = tok::minus;
    break;
  case '*':
    Char = peekNextChar();
    if (Char == '*') {
      // Power operator.
      Kind = tok::starstar;
      Char = getNextChar();
    } else {
      Kind = tok::star;
    }
    break;
  case '/':
    Char = peekNextChar();
    if (Char == '=') {
      // Not equal operator.
      Kind = tok::slashequal;
      Char = getNextChar();
    } else if (Char == ')') {
      // End of array initialization list.
      Kind = tok::slashr_paren;
      Char = getNextChar();
    } else if (Char == '/') {
      // Concatenation operator.
      Kind = tok::slashslash;
      Char = getNextChar();
    } else {
      Kind = tok::slash;
    }
    break;
  // [TODO]: Logical Operators
  case '=':
    Char = peekNextChar();
    if (Char == '=') {
      Kind = tok::equalequal;
      Char = getNextChar();
    } else if (Char == '>') {
      Kind = tok::equalgreater;
      Char = getNextChar();
    } else {      
      Kind = tok::equal;
    }
    break;
  case '<':
    Char = peekNextChar();
    if (Char == '=') {
      Kind = tok::lessequal;
      Char = getNextChar();
    } else {
      Kind = tok::less;
    }
    break;
  case '>':
    Char = peekNextChar();
    if (Char == '=') {
      Kind = tok::greaterequal;
      Char = getNextChar();
    } else {
      Kind = tok::greater;
    }
    break;
  default:
    TokStart = getCurrentPtr();
    Kind = tok::error;
    break;
  }

  // Update the location of token as well as LexPtr.
  Char = getNextChar();
  FormTokenWithChars(Result, Kind);
}

void Lexer::LexFixedFormIdentifierMatchLongestKeyword(const fixedForm::KeywordMatcher &Matcher,
                                                      Token &Tok) {
  LastTokenWasSemicolon = false;
  setBuffer(CurBuf, Tok.getLocation().getPointer(), false);
  Tok.startToken();

  // Check to see if there is still more of the line to lex.
  if (Text.empty() || Text.AtEndOfLine()) {
    Text.Reset();
    Text.GetNextLine();
  }

  TokStart = getCurrentPtr();

  LexFixedFormIdentifier(Matcher, Tok);
}

void Lexer::LexFORMATToken(Token &Result) {
  Result.startToken();

  if (Text.empty() || Text.AtEndOfLine())  {
    TokStart = getCurrentPtr();
    FormTokenWithChars(Result, tok::eof);
    return;
  }

  // Small amounts of horizontal whitespace is very common between tokens.
  char Char = getCurrentChar();
  while (isHorizontalWhitespace(Char))
    Char = getNextChar();

  TokStart = getCurrentPtr();
  tok::TokenKind Kind;

  switch (Char) {
  case 0:  // Null.
    // Found end of file?
    Kind = tok::eof;
    break;
  case '!':
    LexComment(Result);
    if (Features.ReturnComments)
      return;
    return LexFORMATToken(Result);
  case ')':
    Kind = tok::r_paren;
    break;
  case '(':
    Kind = tok::l_paren;
    break;
  case '*':
    Kind = tok::star;
    break;
  case 'A': case 'B':  case 'C': case 'D': case 'E': case 'F': case 'G':
  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
  case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
  case 'V': case 'W': case 'X': case 'Y': case 'Z':
  case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
  case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
  case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
  case 'v': case 'w': case 'x': case 'y': case 'z':
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
  case '.':
    return LexFORMATDescriptor(Result);
  case ',':
    Kind = tok::comma;
    break;
  case ':':
    Kind = tok::colon;
    break;
  case '/':
    Kind = tok::slash;
    break;
  case '"':
  case '\'':
    // [TODO]: Kinds.
    return LexCharacterLiteralConstant(Result, Char == '"');
  default:
    TokStart = getCurrentPtr();
    // FIXME: error.
    Kind = tok::error;
    break;
  }

  // Update the location of token as well as LexPtr.
  Char = getNextChar();
  FormTokenWithChars(Result, Kind);
}

FormatDescriptorLexer::FormatDescriptorLexer(const Lexer &TheLexer,
                                             const Token &FormatDescriptor) {
  Offset = 0;
  TextLoc = FormatDescriptor.getLocation();

  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  TheLexer.getSpelling(FormatDescriptor, Spelling);
  Text = FormatDescriptor.CleanLiteral(Spelling);
}

SourceLocation FormatDescriptorLexer::getCurrentLoc() const {
  return SourceLocation::getFromPointer(TextLoc.getPointer() + Offset);
}

/// returns true if the next token is an integer.
bool FormatDescriptorLexer::IsIntPresent() const {
  return !IsDone() && isDecimalNumberBody(Text[Offset]);
}

/// Advances and returns true if an integer
/// token is present.
bool FormatDescriptorLexer::LexIntIfPresent(llvm::StringRef& Result) {
  if(IsIntPresent()) {
    auto Start = Offset;
    while(!IsDone() && isDecimalNumberBody(Text[Offset])) ++Offset;
    Result = llvm::StringRef(Text).slice(Start, Offset);
    return true;
  }
  return false;
}

/// Advances and returns true if an identifier token
/// is present.
bool FormatDescriptorLexer::LexIdentIfPresent(llvm::StringRef& Result) {
  if(!IsDone() && isLetter(Text[Offset])) {
    auto Start = Offset;
    while(!IsDone() && isLetter(Text[Offset])) ++Offset;
    Result = llvm::StringRef(Text).slice(Start, Offset);
    return true;
  }
  return false;
}

/// Advances by one and returns true if the current char is c.
bool FormatDescriptorLexer::LexCharIfPresent(char c) {
  if(!IsDone() && Text[Offset] == c) {
    ++Offset;
    return true;
  }
  return false;
}

/// Returns true if there's no more characters left in the
/// given string.
bool FormatDescriptorLexer::IsDone() const {
  return Offset >= Text.size();
}

} //namespace flang
