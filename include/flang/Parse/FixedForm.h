//===-- FixedForm.h - Fortran Fixed-form Parsing Utilities ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_PARSER_FIXEDFORM_H
#define FLANG_PARSER_FIXEDFORM_H

#include "flang/Basic/LLVM.h"
#include "flang/Basic/TokenKinds.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/ArrayRef.h"

namespace flang {
namespace fixedForm {

/// KeywordFilter - a set of given keyword(s).
class KeywordFilter {
  ArrayRef<tok::TokenKind> Keywords;
  tok::TokenKind SmallArray[3];
public:

  KeywordFilter(ArrayRef<tok::TokenKind> KW)
    : Keywords(KW) {}
  KeywordFilter(tok::TokenKind K1, tok::TokenKind K2,
                tok::TokenKind K3 = tok::unknown);

  ArrayRef<tok::TokenKind> getKeywords() const {
    return Keywords;
  }
};

/// KeywordMatcher - represents a set of keywords that
/// can be matched.
class KeywordMatcher {
  llvm::StringSet<llvm::BumpPtrAllocator> Keywords;
public:
  KeywordMatcher() {}
  KeywordMatcher(ArrayRef<KeywordFilter> Filters);
  void operator=(ArrayRef<KeywordFilter> Filters);

  void Register(tok::TokenKind Keyword);
  bool Matches(StringRef Identifier) const;
};

/// \brief A set of executable statement and construct
/// keywords that are ambiguous. (e.g. DO)
class AmbiguousExecutableStatements : public KeywordFilter {
public:
  AmbiguousExecutableStatements();
};

/// \brief A set of all the end statement keywords
/// that are ambiguous.
class AmbiguousEndStatements : public KeywordFilter {
public:
  AmbiguousEndStatements();
};

/// \brief A set of all the type statement keywords
/// that are ambiguous.
class AmbiguousTypeStatements : public KeywordFilter {
public:
  AmbiguousTypeStatements();
};

/// \brief A set of all specification statement keywords
/// that are ambiguous.
/// NB: doesn't include type keywords.
class AmbiguousSpecificationStatements : public KeywordFilter {
public:
  AmbiguousSpecificationStatements();
};

/// \brief A set of all the top level declaration statement
/// keywords that are ambiguous.
class AmbiguousTopLevelDeclarationStatements : public KeywordFilter {
public:
  AmbiguousTopLevelDeclarationStatements();
};

/// CommonAmbiguities - contains the matchers for the common ambiguous
/// identifiers.
class CommonAmbiguities {
  KeywordMatcher MatcherForKeywordsAfterRECURSIVE;
  KeywordMatcher MatcherForKeywordsAfterIF;
  KeywordMatcher MatcherForTopLevel;
  KeywordMatcher MatcherForSpecStmts, MatcherForExecStmts;
public:
  CommonAmbiguities();

  /// \brief Returns the matcher for the keywords that come after
  /// RECURSIVE keyword.
  /// NB: This matcher must be used only for when RECURSIVE is used
  /// before the function's type.
  const KeywordMatcher &getMatcherForKeywordsAfterRECURSIVE() const {
    return MatcherForKeywordsAfterRECURSIVE;
  }

  /// \brief Returns the matcher for the keywords that come after
  /// the condition in the IF statement (THEN or action statement keywords).
  const KeywordMatcher &getMatcherForKeywordsAfterIF() const {
    return MatcherForKeywordsAfterIF;
  }

  const KeywordMatcher &getMatcherForTopLevelKeywords() const {
    return MatcherForTopLevel;
  }

  const KeywordMatcher &getMatcherForSpecificationStmtKeywords() const {
    return MatcherForSpecStmts;
  }

  const KeywordMatcher &getMatcherForExecutableStmtKeywords() const {
    return MatcherForExecStmts;
  }

};

} // end namespace fixedForm

} // end namespace flang

#endif
