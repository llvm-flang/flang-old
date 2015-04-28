//===---- VerifyDiagnosticConsumer.cpp - Verifying Diagnostic Client ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which buffers the diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "flang/Frontend/VerifyDiagnosticConsumer.h"
#include "flang/Frontend/TextDiagnosticBuffer.h"
#include "flang/Frontend/FrontendDiagnostic.h"
#include "flang/Parse/Lexer.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

typedef VerifyDiagnosticConsumer::Directive Directive;
typedef VerifyDiagnosticConsumer::DirectiveList DirectiveList;
typedef VerifyDiagnosticConsumer::ExpectedData ExpectedData;

VerifyDiagnosticConsumer::VerifyDiagnosticConsumer(DiagnosticsEngine &_Diags)
  : Diags(_Diags),
    PrimaryClient(Diags.getClient()), OwnsPrimaryClient(Diags.ownsClient()),
    Buffer(new TextDiagnosticBuffer()), CurrentPreprocessor(0),
    LangOpts(0), SrcManager(0), ActiveSourceFiles(0), Status(HasNoDirectives)
{
  Diags.takeClient();
  if (Diags.hasSourceManager())
    setSourceManager(Diags.getSourceManager());
}

VerifyDiagnosticConsumer::~VerifyDiagnosticConsumer() {
  assert(!ActiveSourceFiles && "Incomplete parsing of source files!");
  assert(!CurrentPreprocessor && "CurrentPreprocessor should be invalid!");
  SrcManager = 0;
  CheckDiagnostics();  
  Diags.takeClient();
  if (OwnsPrimaryClient)
    delete PrimaryClient;
}

// DiagnosticConsumer interface.

void VerifyDiagnosticConsumer::BeginSourceFile(const LangOptions &LangOpts, const Lexer *PP) {
  // Attach comment handler on first invocation.
  if (++ActiveSourceFiles == 1) {
    if (PP) {
      CurrentPreprocessor = PP;
      this->LangOpts = &LangOpts;
      setSourceManager(PP->getSourceManager());
      const_cast<Lexer*>(PP)->addCommentHandler(this);
    }
  }

  assert((!PP || CurrentPreprocessor == PP) && "Preprocessor changed!");
  PrimaryClient->BeginSourceFile(LangOpts, PP);
}

void VerifyDiagnosticConsumer::EndSourceFile() {
  assert(ActiveSourceFiles && "No active source files!");
  PrimaryClient->EndSourceFile();

  // Detach comment handler once last active source file completed.
  if (--ActiveSourceFiles == 0) {
    if (CurrentPreprocessor)
      const_cast<Lexer*>(CurrentPreprocessor)->removeCommentHandler(this);

    // Check diagnostics once last file completed.
    CheckDiagnostics();
    CurrentPreprocessor = 0;
    LangOpts = 0;
  }
}

void VerifyDiagnosticConsumer::HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                                SourceLocation L, const llvm::Twine &Msg,
                                                llvm::ArrayRef<SourceRange>,
                                                llvm::ArrayRef<FixItHint>) {
  // Send the diagnostic to the buffer, we will check it once we reach the end
  // of the source file (or are destructed).
  Buffer->HandleDiagnostic(DiagLevel, L, Msg);
}

//===----------------------------------------------------------------------===//
// Checking diagnostics implementation.
//===----------------------------------------------------------------------===//

typedef TextDiagnosticBuffer::DiagList DiagList;
typedef TextDiagnosticBuffer::const_iterator const_diag_iterator;

namespace {

/// StandardDirective - Directive with string matching.
///
class StandardDirective : public Directive {
public:
  StandardDirective(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
                    StringRef Text, unsigned Min, unsigned Max)
    : Directive(DirectiveLoc, DiagnosticLoc, Text, Min, Max) { }

  virtual bool isValid(std::string &Error) {
    // all strings are considered valid; even empty ones
    return true;
  }

  virtual bool match(StringRef S) {
    return S.find(Text) != StringRef::npos;
  }
};

/// RegexDirective - Directive with regular-expression matching.
///
class RegexDirective : public Directive {
public:
  RegexDirective(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
                 StringRef Text, unsigned Min, unsigned Max)
    : Directive(DirectiveLoc, DiagnosticLoc, Text, Min, Max), Regex(Text) { }

  virtual bool isValid(std::string &Error) {
    if (Regex.isValid(Error))
      return true;
    return false;
  }

  virtual bool match(StringRef S) {
    return Regex.match(S);
  }

private:
  llvm::Regex Regex;
};

/// isWhitespace - Return true if this character is horizontal or vertical
/// whitespace: ' ', '\t', '\f', '\v', '\n', '\r'.  Note that this returns false
/// for '\0'.
static inline bool isWhitespace(unsigned char c) {
  switch(c){
  case ' ': case '\t': case '\f': case '\v': case '\n': case '\r':
    return true;
  default: return false;
  }
}

class ParseHelper
{
public:
  ParseHelper(StringRef S)
    : Begin(S.begin()), End(S.end()), C(Begin), P(Begin), PEnd(NULL) { }

  // Return true if string literal is next.
  bool Next(StringRef S) {
    P = C;
    PEnd = C + S.size();
    if (PEnd > End)
      return false;
    return !memcmp(P, S.data(), S.size());
  }

  // Return true if number is next.
  // Output N only if number is next.
  bool Next(unsigned &N) {
    unsigned TMP = 0;
    P = C;
    for (; P < End && P[0] >= '0' && P[0] <= '9'; ++P) {
      TMP *= 10;
      TMP += P[0] - '0';
    }
    if (P == C)
      return false;
    PEnd = P;
    N = TMP;
    return true;
  }

  // Return true if string literal is found.
  // When true, P marks begin-position of S in content.
  bool Search(StringRef S, bool EnsureStartOfWord = false) {
    do {
      P = std::search(C, End, S.begin(), S.end());
      PEnd = P + S.size();
      if (P == End)
        break;
      if (!EnsureStartOfWord
            // Check if string literal starts a new word.
            || P == Begin || isWhitespace(P[-1])
            // Or it could be preceeded by the start of a comment.
            || (P > (Begin + 1) && (P[-1] == '/' || P[-1] == '*')
                                &&  P[-2] == '/'))
        return true;
      // Otherwise, skip and search again.
    } while (Advance());
    return false;
  }

  // Advance 1-past previous next/search.
  // Behavior is undefined if previous next/search failed.
  bool Advance() {
    C = PEnd;
    return C < End;
  }

  // Skip zero or more whitespace.
  void SkipWhitespace() {
    for (; C < End && isWhitespace(*C); ++C)
      ;
  }

  // Return true if EOF reached.
  bool Done() {
    return !(C < End);
  }

  const char * const Begin; // beginning of expected content
  const char * const End;   // end of expected content (1-past)
  const char *C;            // position of next char in content
  const char *P;

private:
  const char *PEnd; // previous next/search subject end (1-past)
};

} // namespace anonymous

/// \brief Get the source location in \arg BufferId for the given line.
static SourceLocation translateLine(const llvm::SourceMgr &SM,
                                 int BufferId,
                                 unsigned Line) {
  const llvm::MemoryBuffer *Buffer = SM.getMemoryBuffer(BufferId);
  const char *Buf = Buffer->getBufferStart();
  size_t BufLength = Buffer->getBufferSize();
  if (BufLength == 0)
    return SourceLocation::getFromPointer(Buf);

  size_t i = 0;
  unsigned LineNo = 1;

  for (; i < BufLength && LineNo != Line; ++i)
    if (Buf[i] == '\n') ++LineNo;

  return SourceLocation::getFromPointer(Buf + i);
}

static SourceLocation getLocWithOffset(SourceLocation L, intptr_t offset) {
  return SourceLocation::getFromPointer(L.getPointer() + offset);
}

/// ParseDirective - Go through the comment and see if it indicates expected
/// diagnostics. If so, then put them in the appropriate directive list.
///
/// Returns true if any valid directives were found.
static bool ParseDirective(StringRef S, ExpectedData *ED, const llvm::SourceMgr &SM,
                           Lexer *PP, const SourceLocation& Pos,
                           VerifyDiagnosticConsumer::DirectiveStatus &Status) {
  DiagnosticsEngine &Diags = PP->getDiagnostics();

  // A single comment may contain multiple directives.
  bool FoundDirective = false;
  for (ParseHelper PH(S); !PH.Done();) {
    // Search for token: expected
    if (!PH.Search("expected", true))
      break;
    PH.Advance();

    // Next token: -
    if (!PH.Next("-"))
      continue;
    PH.Advance();

    // Next token: { error | warning | note }
    DirectiveList* DL = NULL;
    if (PH.Next("error"))
      DL = ED ? &ED->Errors : NULL;
    else if (PH.Next("warning"))
      DL = ED ? &ED->Warnings : NULL;
    else if (PH.Next("note"))
      DL = ED ? &ED->Notes : NULL;
    else if (PH.Next("no-diagnostics")) {
      if (Status == VerifyDiagnosticConsumer::HasOtherExpectedDirectives) {
        Diags.Report(Pos, diag::err_verify_invalid_no_diags)
          << /*IsExpectedNoDiagnostics=*/true;
      }
      else
        Status = VerifyDiagnosticConsumer::HasExpectedNoDiagnostics;
      continue;
    } else
      continue;
    PH.Advance();

    if (Status == VerifyDiagnosticConsumer::HasExpectedNoDiagnostics) {
      Diags.Report(Pos, diag::err_verify_invalid_no_diags)
        << /*IsExpectedNoDiagnostics=*/false;
      continue;
    }
    Status = VerifyDiagnosticConsumer::HasOtherExpectedDirectives;

    // If a directive has been found but we're not interested
    // in storing the directive information, return now.
    if (!DL)
      return true;

    // Default directive kind.
    bool RegexKind = false;
    const char* KindStr = "string";

    // Next optional token: -
    if (PH.Next("-re")) {
      PH.Advance();
      RegexKind = true;
      KindStr = "regex";
    }

    // Next optional token: @
    SourceLocation ExpectedLoc;
    if (!PH.Next("@")) {
      ExpectedLoc = Pos;
    } else {
      PH.Advance();
      unsigned Line = 0;
      bool FoundPlus = PH.Next("+");
      if (FoundPlus || PH.Next("-")) {
        // Relative to current line.
        PH.Advance();
        bool Invalid = false;
        unsigned ExpectedLine = SM.FindLineNumber(Pos);
        if (!Invalid && PH.Next(Line) && (FoundPlus || Line < ExpectedLine)) {
          if (FoundPlus) ExpectedLine += Line;
          else ExpectedLine -= Line;
          ExpectedLoc = translateLine(SM,SM.FindBufferContainingLoc(Pos), ExpectedLine);
        }
      } else if (PH.Next(Line)) {
        // Absolute line number.
        if (Line > 0)
          ExpectedLoc = translateLine(SM,SM.FindBufferContainingLoc(Pos), Line);
      }

      if (!ExpectedLoc.isValid()) {
        Diags.Report(getLocWithOffset(Pos,PH.C-PH.Begin),
                     diag::err_verify_missing_line) << KindStr;
        continue;
      }
      PH.Advance();
    }

    // Skip optional whitespace.
    PH.SkipWhitespace();

    // Next optional token: positive integer or a '+'.
    unsigned Min = 1;
    unsigned Max = 1;
    if (PH.Next(Min)) {
      PH.Advance();
      // A positive integer can be followed by a '+' meaning min
      // or more, or by a '-' meaning a range from min to max.
      if (PH.Next("+")) {
        Max = Directive::MaxCount;
        PH.Advance();
      } else if (PH.Next("-")) {
        PH.Advance();
        if (!PH.Next(Max) || Max < Min) {
          Diags.Report(getLocWithOffset(Pos,PH.C-PH.Begin),
                       diag::err_verify_invalid_range) << KindStr;
          continue;
        }
        PH.Advance();
      } else {
        Max = Min;
      }
    } else if (PH.Next("+")) {
      // '+' on its own means "1 or more".
      Max = Directive::MaxCount;
      PH.Advance();
    }

    // Skip optional whitespace.
    PH.SkipWhitespace();

    // Next token: {{
    if (!PH.Next("{{")) {
      Diags.Report(getLocWithOffset(Pos,PH.C-PH.Begin),
                   diag::err_verify_missing_start) << KindStr;
      continue;
    }
    PH.Advance();
    const char* const ContentBegin = PH.C; // mark content begin

    // Search for token: }}
    if (!PH.Search("}}")) {
      Diags.Report(getLocWithOffset(Pos,PH.C-PH.Begin),
                   diag::err_verify_missing_end) << KindStr;
      continue;
    }
    const char* const ContentEnd = PH.P; // mark content end
    PH.Advance();

    // Build directive text; convert \n to newlines.
    std::string Text;
    StringRef NewlineStr = "\\n";
    StringRef Content(ContentBegin, ContentEnd-ContentBegin);
    size_t CPos = 0;
    size_t FPos;
    while ((FPos = Content.find(NewlineStr, CPos)) != StringRef::npos) {
      Text += Content.substr(CPos, FPos-CPos);
      Text += '\n';
      CPos = FPos + NewlineStr.size();
    }
    if (Text.empty())
      Text.assign(ContentBegin, ContentEnd);

    // Construct new directive.
    Directive *D = Directive::create(RegexKind, Pos, ExpectedLoc, Text,
                                     Min, Max);
    std::string Error;
    if (D->isValid(Error)) {
      DL->push_back(D);
      FoundDirective = true;
    } else {
      Diags.Report(getLocWithOffset(Pos,ContentBegin-PH.Begin),
                   diag::err_verify_invalid_content)
        << KindStr << Error;
    }
  }

  return FoundDirective;
}

/// HandleComment - Hook into the preprocessor and extract comments containing
///  expected errors and warnings.
bool VerifyDiagnosticConsumer::HandleComment(Lexer &PP, const SourceLocation& CommentBegin, const llvm::StringRef &C) {
  const llvm::SourceMgr &SM = PP.getSourceManager();

  // If this comment is for a different source manager, ignore it.
  if (SrcManager && &SM != SrcManager)
    return false;

  if (C.empty())
    return false;

  // Fold any "\<EOL>" sequences
  size_t loc = C.find('\\');
  if (loc == StringRef::npos) {
    ParseDirective(C, &ED, SM, &PP, CommentBegin, Status);
    return false;
  }

  std::string C2;
  C2.reserve(C.size());

  for (size_t last = 0;; loc = C.find('\\', last)) {
    if (loc == StringRef::npos || loc == C.size()) {
      C2 += C.substr(last);
      break;
    }
    C2 += C.substr(last, loc-last);
    last = loc + 1;

    if (C[last] == '\n' || C[last] == '\r') {
      ++last;

      // Escape \r\n  or \n\r, but not \n\n.
      if (last < C.size())
        if (C[last] == '\n' || C[last] == '\r')
          if (C[last] != C[last-1])
            ++last;
    } else {
      // This was just a normal backslash.
      C2 += '\\';
    }
  }

  if (!C2.empty())
    ParseDirective(C2, &ED, SM, &PP, CommentBegin, Status);
  return false;
}

/// \brief Takes a list of diagnostics that have been generated but not matched
/// by an expected-* directive and produces a diagnostic to the user from this.
static unsigned PrintUnexpected(DiagnosticsEngine &Diags, const llvm::SourceMgr *SourceMgr,
                                const_diag_iterator diag_begin,
                                const_diag_iterator diag_end,
                                DiagnosticsEngine::Level Kind) {
  if (diag_begin == diag_end) return 0;

  for (const_diag_iterator I = diag_begin, E = diag_end; I != E; ++I) {
    switch(Kind) {
      case DiagnosticsEngine::Error: Diags.ReportError(I->first,I->second); break;
      case DiagnosticsEngine::Warning: Diags.ReportWarning(I->first,I->second); break;
      case DiagnosticsEngine::Note: Diags.ReportNote(I->first,I->second); break;
    }
  }
  return std::distance(diag_begin, diag_end);
}

/// \brief Takes a list of diagnostics that were expected to have been generated
/// but were not and produces a diagnostic to the user from this.
static unsigned PrintExpected(DiagnosticsEngine &Diags, const llvm::SourceMgr &SourceMgr,
                              DirectiveList &DL, DiagnosticsEngine::Level Kind) {
  if (DL.empty())
    return 0;

  for (DirectiveList::iterator I = DL.begin(), E = DL.end(); I != E; ++I) {
    Directive &D = **I;

    Diags.ReportError(D.DirectiveLoc,llvm::Twine("Inconsistend verify directive: ")+D.Text);
  }

  return DL.size();
}

/// \brief Determine whether two source locations come from the same file.
static bool IsFromSameFile(const llvm::SourceMgr &SM, SourceLocation DirectiveLoc,
                           SourceLocation  DiagnosticLoc) {
  return SM.FindBufferContainingLoc(DirectiveLoc) ==
     SM.FindBufferContainingLoc(DiagnosticLoc);
  return true;
}

/// CheckLists - Compare expected to seen diagnostic lists and return the
/// the difference between them.
///
static unsigned CheckLists(DiagnosticsEngine &Diags, const llvm::SourceMgr &SourceMgr,
                           DiagnosticsEngine::Level Label,
                           DirectiveList &Left,
                           const_diag_iterator d2_begin,
                           const_diag_iterator d2_end) {
  DirectiveList LeftOnly;
  DiagList Right(d2_begin, d2_end);

  for (DirectiveList::iterator I = Left.begin(), E = Left.end(); I != E; ++I) {
    Directive& D = **I;
    unsigned LineNo1 = SourceMgr.FindLineNumber(D.DiagnosticLoc);

    for (unsigned i = 0; i < D.Max; ++i) {
      DiagList::iterator II, IE;
      for (II = Right.begin(), IE = Right.end(); II != IE; ++II) {
        unsigned LineNo2 = SourceMgr.FindLineNumber(II->first);
        if (LineNo1 != LineNo2)
          continue;

        if (!IsFromSameFile(SourceMgr, D.DiagnosticLoc, II->first))
          continue;

        const std::string &RightText = II->second;
        if (D.match(RightText))
          break;
      }
      if (II == IE) {
        // Not found.
        if (i >= D.Min) break;
        LeftOnly.push_back(*I);
      } else {
        // Found. The same cannot be found twice.
        Right.erase(II);
      }
    }
  }
  // Now all that's left in Right are those that were not matched.
  unsigned num = PrintExpected(Diags, SourceMgr, LeftOnly, Label);
  num += PrintUnexpected(Diags, &SourceMgr, Right.begin(), Right.end(), Label);
  return num;
}

/// CheckResults - This compares the expected results to those that
/// were actually reported. It emits any discrepencies. Return "true" if there
/// were problems. Return "false" otherwise.
///
static unsigned CheckResults(DiagnosticsEngine &Diags, const llvm::SourceMgr &SourceMgr,
                             const TextDiagnosticBuffer &Buffer,
                             ExpectedData &ED) {
  // We want to capture the delta between what was expected and what was
  // seen.
  //
  //   Expected \ Seen - set expected but not seen
  //   Seen \ Expected - set seen but not expected
  unsigned NumProblems = 0;

  // See if there are error mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, DiagnosticsEngine::Error, ED.Errors,
                            Buffer.err_begin(), Buffer.err_end());

  // See if there are warning mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, DiagnosticsEngine::Warning, ED.Warnings,
                            Buffer.warn_begin(), Buffer.warn_end());

  // See if there are note mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, DiagnosticsEngine::Note, ED.Notes,
                            Buffer.note_begin(), Buffer.note_end());

  return NumProblems;
}

void VerifyDiagnosticConsumer::CheckDiagnostics() {
  // Ensure any diagnostics go to the primary client.
  bool OwnsCurClient = Diags.ownsClient();
  DiagnosticClient *CurClient = Diags.takeClient();
  Diags.setClient(PrimaryClient, false);

  if (SrcManager) {
    // Produce an error if no expected-* directives could be found in the
    // source file(s) processed.
    if (Status == HasNoDirectives) {
      Diags.Report(diag::err_verify_no_directives).setForceEmit();
      ++NumErrors;
      Status = HasNoDirectivesReported;
    }

    // Check that the expected diagnostics occurred.
    NumErrors += CheckResults(Diags, *SrcManager, *Buffer, ED);
  } else {
    NumErrors += (PrintUnexpected(Diags, 0, Buffer->err_begin(),
                                  Buffer->err_end(), DiagnosticsEngine::Error) +
                  PrintUnexpected(Diags, 0, Buffer->warn_begin(),
                                  Buffer->warn_end(), DiagnosticsEngine::Warning) +
                  PrintUnexpected(Diags, 0, Buffer->note_begin(),
                                  Buffer->note_end(), DiagnosticsEngine::Note));
  }

  Diags.takeClient();
  Diags.setClient(CurClient, OwnsCurClient);

  // Reset the buffer, we have processed all the diagnostics in it.
  Buffer.reset(new TextDiagnosticBuffer());
  ED.Errors.clear();
  ED.Warnings.clear();
  ED.Notes.clear();
}

Directive *Directive::create(bool RegexKind, SourceLocation DirectiveLoc,
                             SourceLocation DiagnosticLoc, StringRef Text,
                             unsigned Min, unsigned Max) {
  if (RegexKind)
    return new RegexDirective(DirectiveLoc, DiagnosticLoc, Text, Min, Max);
  return new StandardDirective(DirectiveLoc, DiagnosticLoc, Text, Min, Max);
}

} // end namespace flang
