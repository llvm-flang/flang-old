//===-- ParserExpr.cpp - Fortran Expression Parser ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Fortran expression parsing.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Ownership.h"
#include "flang/Sema/Sema.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"

namespace flang {

// ParseExpression - Expressions are level-5 expresisons optionally involving
// defined binary operators.
//
//   R722:
//     expr :=
//         [ expr defined-binary-op ] level-5-expr
//
//   R723:
//     defined-binary-op :=
//         . letter [ letter ] ... .
Parser::ExprResult Parser::ParseExpression() {
  ExprResult LHS = ParseLevel5Expr();
  if (LHS.isInvalid()) return LHS;

  if (!IsPresent(tok::defined_operator))
    return LHS;

  SourceLocation OpLoc = Tok.getLocation();
  IdentifierInfo *II = Tok.getIdentifierInfo();
  Lex();

  ExprResult RHS = ParseLevel5Expr();
  if (RHS.isInvalid()) return RHS;

  return DefinedBinaryOperatorExpr::Create(Context, OpLoc, LHS.take(), RHS.take(), II);
}

ExprResult Parser::ParseExpectedExpression() {
  if(Tok.isAtStartOfStatement()) {
    Diag.Report(getExpectedLoc(), diag::err_expected_expression);
    return ExprError();
  }
  return ParseExpression();
}

/// \brief Looks at the next token to see if it's an expression
/// and calls ParseExpression if it is, or reports an expected expression
/// error.
ExprResult Parser::ParseExpectedFollowupExpression(const char *DiagAfter) {
  if(Tok.isAtStartOfStatement()) {
    Diag.Report(getExpectedLoc(), diag::err_expected_expression_after)
      << DiagAfter;
    return ExprError();
  }
  return ParseExpression();
}

// ParseLevel5Expr - Level-5 expressions are level-4 expressions optionally
// involving the logical operators.
//
//   R717:
//     level-5-expr :=
//         [ level-5-expr equiv-op ] equiv-operand
//   R716:
//     equiv-operand :=
//         [ equiv-operand or-op ] or-operand
//   R715:
//     or-operand :=
//         [ or-operand and-op ] and-operand
//   R714:
//     and-operand :=
//         [ not-op ] level-4-expr
//         
//   R718:
//     not-op :=
//         .NOT.
//   R719:
//     and-op :=
//         .AND.
//   R720:
//     or-op :=
//         .OR.
//   R721:
//     equiv-op :=
//         .EQV.
//      or .NEQV.
Parser::ExprResult Parser::ParseAndOperand() {
  SourceLocation NotLoc = Tok.getLocation();
  bool Negate = ConsumeIfPresent(tok::kw_NOT);

  ExprResult E = ParseLevel4Expr();
  if (E.isInvalid()) return E;

  if (Negate)
    E = Actions.ActOnUnaryExpr(Context, NotLoc, UnaryExpr::Not, E);
  return E;
}
Parser::ExprResult Parser::ParseOrOperand() {
  ExprResult E = ParseAndOperand();
  if (E.isInvalid()) return E;

  while (Tok.getKind() == tok::kw_AND) {
    SourceLocation OpLoc = Tok.getLocation();
    Lex();
    ExprResult AndOp = ParseAndOperand();
    if (AndOp.isInvalid()) return AndOp;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, BinaryExpr::And, E, AndOp);
  }

  return E;
}
Parser::ExprResult Parser::ParseEquivOperand() {
  ExprResult E = ParseOrOperand();
  if (E.isInvalid()) return E;

  while (Tok.getKind() == tok::kw_OR) {
    SourceLocation OpLoc = Tok.getLocation();
    Lex();
    ExprResult OrOp = ParseOrOperand();
    if (OrOp.isInvalid()) return OrOp;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, BinaryExpr::Or, E, OrOp);
  }

  return E;
}
Parser::ExprResult Parser::ParseLevel5Expr() {
  ExprResult E = ParseEquivOperand();
  if (E.isInvalid()) return E;

  while (true) {
    SourceLocation OpLoc = Tok.getLocation();
    switch (Tok.getKind()) {
    default:
      return E;
    case tok::kw_EQV:
      Lex();
      E = Actions.ActOnBinaryExpr(Context, OpLoc, BinaryExpr::Eqv, E,
                               ParseEquivOperand());
      break;
    case tok::kw_NEQV:
      Lex();
      E = Actions.ActOnBinaryExpr(Context, OpLoc, BinaryExpr::Neqv, E,
                               ParseEquivOperand());
      break;
    }
  }
  return E;
}

// ParseLevel4Expr - Level-4 expressions are level-3 expressions optionally
// involving the relational operators.
//
//   R712:
//     level-4-expr :=
//         [ level-3-expr rel-op ] level-3-expr
//   R713:
//     rel-op :=
//         .EQ.
//      or .NE.
//      or .LT.
//      or .LE.
//      or .GT.
//      or .GE.
//      or ==
//      or /=
//      or <
//      or <=
//      or >
//      or >=
Parser::ExprResult Parser::ParseLevel4Expr() {
  ExprResult E = ParseLevel3Expr();
  if (E.isInvalid()) return E;

  while (true) {
    SourceLocation OpLoc = Tok.getLocation();
    BinaryExpr::Operator Op = BinaryExpr::None;
    switch (Tok.getKind()) {
    default:
      return E;
    case tok::kw_EQ: case tok::equalequal:
      Op = BinaryExpr::Equal;
      break;
    case tok::kw_NE: case tok::slashequal:
      Op = BinaryExpr::NotEqual;
      break;
    case tok::kw_LT: case tok::less:
      Op = BinaryExpr::LessThan;
      break;
    case tok::kw_LE: case tok::lessequal:
      Op = BinaryExpr::LessThanEqual;
      break;
    case tok::kw_GT: case tok::greater:
      Op = BinaryExpr::GreaterThan;
      break;
    case tok::kw_GE: case tok::greaterequal:
      Op = BinaryExpr::GreaterThanEqual;
      break;
    }

    Lex();
    ExprResult Lvl3Expr = ParseLevel3Expr();
    if (Lvl3Expr.isInvalid()) return Lvl3Expr;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, Op, E, Lvl3Expr);
  }
  return E;
}

// ParseLevel3Expr - Level-3 expressions are level-2 expressions optionally
// involving the character operator concat-op.
//
//   R710:
//     level-3-expr :=
//         [ level-3-expr concat-op ] level-2-expr
//   R711:
//     concat-op :=
//         //
Parser::ExprResult Parser::ParseLevel3Expr() {
  ExprResult E = ParseLevel2Expr();
  if (E.isInvalid()) return E;

  while (Tok.getKind() == tok::slashslash) {
    SourceLocation OpLoc = Tok.getLocation();
    Lex();
    ExprResult Lvl2Expr = ParseLevel2Expr();
    if (Lvl2Expr.isInvalid()) return Lvl2Expr;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, BinaryExpr::Concat, E, Lvl2Expr);
  }
  
  return E;
}

// ParseLevel2Expr - Level-2 expressions are level-1 expressions optionally
// involving the numeric operators power-op, mult-op, and add-op.
//
//   R706:
//     level-2-expr :=
//         [ [ level-2-expr ] add-op ] add-operand
//   R705:
//     add-operand :=
//         [ add-operand mult-op ] mult-operand
//   R704:
//     mult-operand :=
//         level-1-expr [ power-op mult-operand ]
//   R707:
//     power-op :=
//         **
//   R708:
//     mult-op :=
//         *
//      or /
//   R709:
//     add-op :=
//         +
//      or -
Parser::ExprResult Parser::ParseMultOperand() {
  ExprResult E = ParseLevel1Expr();
  if (E.isInvalid()) return E;

  if (Tok.getKind() == tok::starstar) {
    SourceLocation OpLoc = Tok.getLocation();
    Lex();
    ExprResult MulOp = ParseMultOperand();
    if (MulOp.isInvalid()) return MulOp;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, BinaryExpr::Power, E, MulOp);
  }

  return E;
}
Parser::ExprResult Parser::ParseAddOperand() {
  ExprResult E = ParseMultOperand();
  if (E.isInvalid()) return E;

  while (true) {
    SourceLocation OpLoc = Tok.getLocation();
    BinaryExpr::Operator Op = BinaryExpr::None;
    switch (Tok.getKind()) {
    default:
      return E;
    case tok::star:
      Op = BinaryExpr::Multiply;
      break;
    case tok::slash:
      Op = BinaryExpr::Divide;
      break;
    }

    Lex();
    ExprResult MulOp = ParseMultOperand();
    if (MulOp.isInvalid()) return MulOp;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, Op, E, MulOp);
  }
  return E;
}
Parser::ExprResult Parser::ParseLevel2Expr() {
  ExprResult E;
  SourceLocation OpLoc = Tok.getLocation();
  tok::TokenKind Kind = Tok.getKind();

  if (Kind == tok::plus || Kind == tok::minus) {
    Lex(); // Eat operand.

    E = ParseAddOperand();
    if (E.isInvalid()) return E;

    if (Kind == tok::minus)
      E = Actions.ActOnUnaryExpr(Context, OpLoc, UnaryExpr::Minus, E);
    else
      E = Actions.ActOnUnaryExpr(Context, OpLoc, UnaryExpr::Plus, E);
  } else {
    E = ParseAddOperand();
    if (E.isInvalid()) return E;
  }

  while (true) {
    OpLoc = Tok.getLocation();
    BinaryExpr::Operator Op = BinaryExpr::None;
    switch (Tok.getKind()) {
    default:
      return E;
    case tok::plus:
      Op = BinaryExpr::Plus;
      break;
    case tok::minus:
      Op = BinaryExpr::Minus;
      break;
    }

    Lex();
    ExprResult AddOp = ParseAddOperand();
    if (AddOp.isInvalid()) return AddOp;
    E = Actions.ActOnBinaryExpr(Context, OpLoc, Op, E, AddOp);
  }
  return E;
}

// ParseLevel1Expr - Level-1 expressions are primaries optionally operated on by
// defined unary operators.
//
//   R702:
//     level-1-expr :=
//         [ defined-unary-op ] primary
//   R703:
//     defined-unary-op :=
//         . letter [ letter ] ... .
Parser::ExprResult Parser::ParseLevel1Expr() {
  SourceLocation OpLoc = Tok.getLocation();
  IdentifierInfo *II = 0;
 if (IsPresent(tok::defined_operator) && !IsNextToken(tok::l_paren)) {
    II = Tok.getIdentifierInfo();
    Lex();
  }

  ExprResult E = ParsePrimaryExpr();
  if (E.isInvalid()) return E;

  if (II)
    E = DefinedUnaryOperatorExpr::Create(Context, OpLoc, E.take(), II);

  return E;
}

/// SetKindSelector - Set the constant expression's kind selector (if present).
void Parser::SetKindSelector(ConstantExpr *E, StringRef Kind) {
  if (Kind.empty()) return;

  SourceLocation Loc; // FIXME: Need to figure out the correct kind position.
  Expr *KindExpr = 0;

  if (::isdigit(Kind[0])) {
    KindExpr = IntegerConstantExpr::Create(Context, E->getSourceRange(),
                                           Kind);
  } else {
    std::string KindStr(Kind);
    const IdentifierInfo *IDInfo = getIdentifierInfo(KindStr);
    VarDecl *VD = Actions.ActOnKindSelector(Context, Loc, IDInfo);
    KindExpr = VarExpr::Create(Context, Loc, VD);
  }

  E->setKindSelector(KindExpr);
}

// ParsePrimaryExpr - Parse a primary expression.
//
//   [R701]:
//     primary :=
//         constant
//      or designator
//      or array-constructor
//      or structure-constructor
//      or function-reference
//      or type-param-inquiry
//      or type-param-name
//      or ( expr )
Parser::ExprResult Parser::ParsePrimaryExpr(bool IsLvalue) {
  ExprResult E;
  SourceLocation Loc = Tok.getLocation();

  // FIXME: Add rest of the primary expressions.
  switch (Tok.getKind()) {
  default:
    if (isTokenIdentifier())
      goto possible_keyword_as_ident;
    Diag.Report(getExpectedLoc(), diag::err_expected_expression);
    return ExprError();
  case tok::error:
    Lex();
    return ExprError();
  case tok::l_paren: {
    ConsumeParen();

    E = ParseExpression();
    // complex constant.
    if(ConsumeIfPresent(tok::comma)) {
      if(E.isInvalid()) return E;
      auto ImPart = ParseExpectedFollowupExpression(",");
      if(ImPart.isInvalid()) return ImPart;
      E = Actions.ActOnComplexConstantExpr(Context, Loc,
                                           getMaxLocationOfCurrentToken(),
                                           E, ImPart);
    }

    ExpectAndConsume(tok::r_paren, 0, "", tok::r_paren);
    break;
  }
  case tok::l_parenslash : {
    return ParseArrayConstructor();
    break;
  }
  case tok::logical_literal_constant: {
    std::string NumStr;
    CleanLiteral(Tok, NumStr);

    StringRef Data(NumStr);
    std::pair<StringRef, StringRef> StrPair = Data.split('_');
    E = LogicalConstantExpr::Create(Context, getTokenRange(),
                                    StrPair.first, Context.LogicalTy);
    SetKindSelector(cast<ConstantExpr>(E.get()), StrPair.second);
    ConsumeToken();
    break;
  }
  case tok::binary_boz_constant:
  case tok::octal_boz_constant:
  case tok::hex_boz_constant: {
    std::string NumStr;
    CleanLiteral(Tok, NumStr);

    StringRef Data(NumStr);
    std::pair<StringRef, StringRef> StrPair = Data.split('_');
    E = BOZConstantExpr::Create(Context, Loc,
                                getMaxLocationOfCurrentToken(),
                                StrPair.first);
    SetKindSelector(cast<ConstantExpr>(E.get()), StrPair.second);
    ConsumeToken();
    break;
  }
  case tok::char_literal_constant: {
    std::string NumStr;
    CleanLiteral(Tok, NumStr);
    E = CharacterConstantExpr::Create(Context, getTokenRange(),
                                      StringRef(NumStr), Context.CharacterTy);
    ConsumeToken();
    // Possible substring
    if(IsPresent(tok::l_paren))
      return ParseSubstring(E);
    break;
  }
  case tok::int_literal_constant: {
    std::string NumStr;
    CleanLiteral(Tok, NumStr);

    StringRef Data(NumStr);
    std::pair<StringRef, StringRef> StrPair = Data.split('_');
    E = IntegerConstantExpr::Create(Context, getTokenRange(),
                                    StrPair.first);
    SetKindSelector(cast<ConstantExpr>(E.get()), StrPair.second);

    Lex();
    break;
  }
  case tok::real_literal_constant: {
    std::string NumStr;
    CleanLiteral(Tok, NumStr);

    StringRef Data(NumStr);
    std::pair<StringRef, StringRef> StrPair = Data.split('_');
    E = RealConstantExpr::Create(Context, getTokenRange(),
                                 NumStr, Context.RealTy);
    SetKindSelector(cast<ConstantExpr>(E.get()), StrPair.second);
    ConsumeToken();
    break;
  }
  case tok::double_precision_literal_constant: {
    std::string NumStr;
    CleanLiteral(Tok, NumStr);
    // Replace the d/D exponent into e exponent
    for(size_t I = 0, Len = NumStr.length(); I < Len; ++I) {
      if(NumStr[I] == 'd' || NumStr[I] == 'D') {
        NumStr[I] = 'e';
        break;
      } else if(NumStr[I] == '_') break;
    }

    StringRef Data(NumStr);
    std::pair<StringRef, StringRef> StrPair = Data.split('_');
    E = RealConstantExpr::Create(Context, getTokenRange(),
                                 NumStr, Context.DoublePrecisionTy);
    SetKindSelector(cast<ConstantExpr>(E.get()), StrPair.second);
    ConsumeToken();
    break;
  }
  case tok::identifier:
    possible_keyword_as_ident:
    E = Parser::ParseDesignator(IsLvalue);
    if (E.isInvalid()) return E;
    break;
  case tok::minus:
    Lex();
    E = Parser::ParsePrimaryExpr();
    if (E.isInvalid()) return E;
    E = Actions.ActOnUnaryExpr(Context, Loc, UnaryExpr::Minus, E);
    break;
  case tok::plus:
    Lex();
    E = Parser::ParsePrimaryExpr();
    if (E.isInvalid()) return E;
    E = Actions.ActOnUnaryExpr(Context, Loc, UnaryExpr::Plus, E);
    break;
  }

  return E;
}

/// ParseDesignator - Parse a designator. Return null if current token is not a
/// designator.
///
///   [R601]:
///     designator :=
///         object-name
///      or array-element
///      or array-section
///      or coindexed-named-object
///      or complex-part-designator
///      or structure-component
///      or substring
///
/// FIXME: substring for a character array
ExprResult Parser::ParseDesignator(bool IsLvalue) {
  auto E = ParseNameOrCall();

  struct ScopedFlag {
    bool value;
    bool &dest;

    ScopedFlag(bool &flag) : dest(flag) {
      value = flag;
    }
    ~ScopedFlag() {
      dest = value;
    }
  };

  ScopedFlag Flag(DontResolveIdentifiers);
  if(DontResolveIdentifiersInSubExpressions)
    DontResolveIdentifiers = true;

  while(true) {
    if(!E.isUsable())
      break;
    if(IsPresent(tok::l_paren)) {
      auto EType = E.get()->getType();
      if(EType->isArrayType())
        E = ParseArraySubscript(E);
      else if(EType->isCharacterType())
        E = ParseSubstring(E);
      else {
        Diag.Report(Tok.getLocation(), diag::err_unexpected_lparen);
        return ExprError();
      }
    } else if(IsPresent(tok::percent)) {
      auto EType = E.get()->getType();
      if(EType->isRecordType())
        E = ParseStructureComponent(E);
      else {
        Diag.Report(Tok.getLocation(), diag::err_unexpected_percent);
        return ExprError();
      }
    }
    else break;
  }

  return E;
}

/// ParseNameOrCall - Parse a name or a call expression
ExprResult Parser::ParseNameOrCall() {
  auto IDInfo = Tok.getIdentifierInfo();
  assert(IDInfo && "Token isn't an identifier");
  auto IDRange = getTokenRange();
  auto IDLoc = ConsumeToken();

  if(DontResolveIdentifiers)
    return UnresolvedIdentifierExpr::Create(Context,
                                            IDRange, IDInfo);

  // [R504]:
  //   object-name :=
  //       name
  auto Declaration = Actions.ResolveIdentifier(IDInfo);
  if(!Declaration) {
    if(IsPresent(tok::l_paren))
      Declaration = Actions.ActOnImplicitFunctionDecl(Context, IDLoc, IDInfo);
    else
      Declaration = Actions.ActOnImplicitEntityDecl(Context, IDLoc, IDInfo);
    if(!Declaration)
      return ExprError();
  } else {
    // INTEGER f
    // X = f(10) <-- implicit function declaration.
    if(IsPresent(tok::l_paren))
      Declaration = Actions.ActOnPossibleImplicitFunctionDecl(Context, IDLoc, IDInfo, Declaration);
  }

  if(VarDecl *VD = dyn_cast<VarDecl>(Declaration)) {
    // FIXME: function returing array
    if(IsPresent(tok::l_paren) &&
       VD->isFunctionResult() && isa<FunctionDecl>(Actions.CurContext)) {
      // FIXME: accessing function results from inner recursive functions
      return ParseRecursiveCallExpression(IDRange);
    }
    return VarExpr::Create(Context, IDRange, VD);
  }
  else if(IntrinsicFunctionDecl *IFunc = dyn_cast<IntrinsicFunctionDecl>(Declaration)) {
    SmallVector<Expr*, 8> Arguments;
    SourceLocation RParenLoc = Tok.getLocation();
    auto Result = ParseFunctionCallArgumentList(Arguments, RParenLoc);
    if(Result.isInvalid())
      return ExprError();
    return Actions.ActOnIntrinsicFunctionCallExpr(Context, IDLoc, IFunc, Arguments);
  } else if(FunctionDecl *Func = dyn_cast<FunctionDecl>(Declaration)) {
    // FIXME: allow subroutines, but errors in sema
    if(!IsPresent(tok::l_paren))
      return FunctionRefExpr::Create(Context, IDRange, Func);
    if(!Func->isSubroutine()) {
      return ParseCallExpression(IDLoc, Func);
    }
  } else if(isa<SelfDecl>(Declaration) && isa<FunctionDecl>(Actions.CurContext))
    return ParseRecursiveCallExpression(IDRange);
  else if(auto Record = dyn_cast<RecordDecl>(Declaration))
    return ParseTypeConstructor(IDLoc, Record);
  Diag.Report(IDLoc, diag::err_expected_var);
  return ExprError();
}

ExprResult Parser::ParseRecursiveCallExpression(SourceRange IDRange) {
  auto Func = Actions.CurrentContextAsFunction();
  auto IDLoc = IDRange.Start;
  if(Func->isSubroutine()) {
    Diag.Report(IDLoc, diag::err_invalid_subroutine_use)
     << Func->getIdentifier() << getTokenRange(IDLoc);
    return ExprError();
  }
  if(!Actions.CheckRecursiveFunction(IDLoc))
    return ExprError();

  if(!IsPresent(tok::l_paren))
    return FunctionRefExpr::Create(Context, IDRange, Func);
  return ParseCallExpression(IDLoc, Func);
}

/// ParseCallExpression - Parse a call expression
ExprResult Parser::ParseCallExpression(SourceLocation IDLoc, FunctionDecl *Function) {
  SmallVector<Expr*, 8> Arguments;
  auto Loc = Tok.getLocation();
  SourceLocation RParenLoc = Loc;
  auto Result = ParseFunctionCallArgumentList(Arguments, RParenLoc);
  if(Result.isInvalid())
    return ExprError();
  return Actions.ActOnCallExpr(Context, Loc, RParenLoc, IDLoc, Function, Arguments);
}

/// ParseFunctionCallArgumentList - Parses an argument list to a call expression.
ExprResult Parser::ParseFunctionCallArgumentList(SmallVectorImpl<Expr*> &Args, SourceLocation &RParenLoc) {
  if(!ExpectAndConsume(tok::l_paren))
    return ExprError();

  RParenLoc = getExpectedLoc();
  if(ConsumeIfPresent(tok::r_paren))
    return ExprResult();

  auto PunctuationTok = "(";
  do {
    if(Tok.isAtStartOfStatement())
      break;
    auto E = ParseExpectedFollowupExpression(PunctuationTok);
    if(E.isInvalid())
      SkipUntil(tok::comma, tok::r_paren, true, true);
    else Args.push_back(E.get());
    PunctuationTok = ",";
  } while (ConsumeIfPresent(tok::comma));

  RParenLoc = getExpectedLoc();
  ExpectAndConsume(tok::r_paren, 0, "", tok::r_paren);
  return ExprResult();
}

/// ParseArrayElement - Parse an array element.
/// 
///   R617:
///     array-element :=
///         data-ref
ExprResult Parser::ParseArrayElement() {
  ExprResult E;
  return E;
}

/// ParseArraySection - Parse a array section.
///
///   R618:
///     array-section :=
///         data-ref [ ( substring-range ) ]
///      or complex-part-designator
///   R610:
///     substring-range :=
///         [ scalar-int-expr ] : [ scalar-int-expr ]
ExprResult Parser::ParseArraySection() {
  ExprResult E;
  return E;
}

/// ParseCoindexedNamedObject - Parse a coindexed named object.
///
///   R614:
///     coindexed-named-object :=
///         data-ref
///   C620:
///     The data-ref shall contain exactly one part-re. The part-ref shall
///     contain an image-selector. The part-name shall be the name of a scalar
///     coarray.
ExprResult Parser::ParseCoindexedNamedObject() {
  ExprResult E;
  return E;
}

/// ParseComplexPartDesignator - Parse a complex part designator.
///
///   R615:
///     complex-part-designator :=
///         designator % RE
///      or designator % IM
///   C621:
///     The designator shall be of complex type.
ExprResult Parser::ParseComplexPartDesignator() {
  ExprResult E;
  return E;
}

/// ParseStructureComponent - Parse a structure component.
///
///   R613:
///     structure-component :=
///        designator % data-ref
ExprResult Parser::ParseStructureComponent(ExprResult Target) {
  auto Loc = ConsumeToken();
  auto ID = Tok.getIdentifierInfo();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier))
    return ExprError();
  return Actions.ActOnStructureComponentExpr(Context, Loc, IDLoc, ID,
                                             Target.get());
}

/// ParseSubstring - Parse a substring.
///
///   R608:
///     substring :=
///         parent-string ( substring-range )
///   R609:
///     parent-string :=
///         scalar-variable-name
///      or array-element
///      or coindexed-named-object
///      or scalar-structure-component
///      or scalar-constant
///   R610:
///     substring-range :=
///         [ scalar-int-expr ] : [ scalar-int-expr ]
ExprResult Parser::ParseSubstring(ExprResult Target) {
  ExprResult StartingPoint, EndPoint;
  auto Loc = ConsumeParen();

  if(!ConsumeIfPresent(tok::colon)) {
    StartingPoint = ParseExpectedFollowupExpression("(");
    if(StartingPoint.isInvalid())
      SkipUntil(tok::colon, true, true);
    Loc = Tok.getLocation();
    if(!ExpectAndConsume(tok::colon, 0, "", tok::r_paren))
      goto done;
  }

  if(!ConsumeIfPresent(tok::r_paren)) {
    EndPoint = ParseExpectedFollowupExpression(":");
    if(EndPoint.isInvalid())
      SkipUntil(tok::r_paren, true, true);
    ExpectAndConsume(tok::r_paren, 0, "", tok::r_paren);
  }

done:
  return Actions.ActOnSubstringExpr(Context, Loc, Target.get(),
                                    StartingPoint.get(), EndPoint.get());
}

/// ParseArrauSubscript - Parse an Array Subscript Expression
ExprResult Parser::ParseArraySubscript(ExprResult Target) {
  SmallVector<Expr*, 8> ExprList;
  auto Loc = ConsumeParen();

  bool IgnoreRParen = false;
  auto PunctuationTok = "(";
  do {
    if(Tok.isAtStartOfStatement())
      IgnoreRParen = true;
    auto E = ParseArraySection(PunctuationTok);
    if(E.isInvalid())
      SkipUntil(tok::comma, tok::r_paren, true, true);
    if(E.isUsable())
      ExprList.push_back(E.get());
    PunctuationTok = ",";
  } while(ConsumeIfPresent(tok::comma));

  auto RParenLoc = getExpectedLoc();
  if(!IgnoreRParen)
    ExpectAndConsume(tok::r_paren, 0, "", tok::r_paren);

  return Actions.ActOnSubscriptExpr(Context, Loc, RParenLoc, Target.get(),
                                    ExprList);
}

ExprResult Parser::ParseArraySection(const char *PunctuationTok) {
  ExprResult LB, UB, Stride;
  bool Range = false;

  auto ColonLoc = Tok.getLocation();
  if(ConsumeIfPresent(tok::colon)) {
    Range = true;
    if(!IsPresent(tok::colon) && !IsPresent(tok::comma) && !IsPresent(tok::r_paren)) {
      UB = ParseExpectedFollowupExpression(":");
      if(UB.isInvalid())
        return UB;
    }
  } else {
    LB = ParseExpectedFollowupExpression(PunctuationTok);
    if(LB.isInvalid())
      return LB;
    ColonLoc = Tok.getLocation();
    if(ConsumeIfPresent(tok::colon)) {
      Range = true;
      if(!IsPresent(tok::colon) && !IsPresent(tok::comma) && !IsPresent(tok::r_paren)) {
        UB = ParseExpectedFollowupExpression(":");
        if(UB.isInvalid())
          return UB;
      }
    }
  }
  if(ConsumeIfPresent(tok::colon)) {
    Stride = ParseExpectedFollowupExpression(":");
    if(Stride.isInvalid())
      return Stride;
  }
  if(Stride.isUsable())
    return StridedRangeExpr::Create(Context, ColonLoc, LB.get(),
                                    UB.get(), Stride.get());
  if(Range)
    return RangeExpr::Create(Context, ColonLoc, LB.get(), UB.get());
  return LB;
}

/// ParseDataReference - Parse a data reference.
///
///   R611:
///     data-ref :=
///         part-ref [ % part-ref ] ...
ExprResult Parser::ParseDataReference() {
  std::vector<ExprResult> Exprs;

  do {
    ExprResult E = ParsePartReference();
    if (E.isInvalid()) return E;
    Exprs.push_back(E);
  } while (ConsumeIfPresent(tok::percent));

  return Actions.ActOnDataReference(Exprs);
}

/// ParsePartReference - Parse the part reference.
///
///   R612:
///     part-ref :=
///         part-name [ ( section-subscript-list ) ] [ image-selector ]
///   R620:
///     section-subscript :=
///         subscript
///      or subscript-triplet
///      or vector-subscript
///   R619:
///     subscript :=
///         scalar-int-expr
///   R621:
///     subscript-triplet :=
///         [ subscript ] : [ subscript ] [ : stride ]
///   R622:
///     stride :=
///         scalar-int-expr
///   R623:
///     vector-subscript :=
///         int-expr
///   R624:
///     image-selector :=
///         lbracket cosubscript-list rbracket
///   R625:
///     cosubscript :=
///         scalar-int-expr
ExprResult Parser::ParsePartReference() {
  ExprResult E;
  return E;
}

/// FIXME: todo implied-do
ExprResult Parser::ParseArrayConstructor() {
  auto Loc = ConsumeParenSlash();
  SourceLocation EndLoc = Tok.getLocation();

  SmallVector<Expr*, 16> ExprList;
  if(ConsumeIfPresent(tok::slashr_paren))
    return Actions.ActOnArrayConstructorExpr(Context, Loc, EndLoc, ExprList);
  do {
    auto E = ParseExpectedExpression();
    if(E.isInvalid())
      goto error;
    if(E.isUsable())
      ExprList.push_back(E.get());
  } while(ConsumeIfPresent(tok::comma));

  EndLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::slashr_paren))
    goto error;

  return Actions.ActOnArrayConstructorExpr(Context, Loc, EndLoc, ExprList);
error:
  EndLoc = Tok.getLocation();
  SkipUntil(tok::slashr_paren);
  return Actions.ActOnArrayConstructorExpr(Context, Loc, EndLoc, ExprList);
}

/// ParseTypeConstructorExpression - Parses a type constructor.
ExprResult Parser::ParseTypeConstructor(SourceLocation IDLoc, RecordDecl *Record) {
  SmallVector<Expr*, 8> Arguments;
  SourceLocation RParenLoc = IDLoc;
  auto LParenLoc = Tok.getLocation();
  auto E = ParseFunctionCallArgumentList(Arguments, RParenLoc);
  if(E.isInvalid())
    return ExprError();
  return Actions.ActOnTypeConstructorExpr(Context, IDLoc, LParenLoc, RParenLoc, Record, Arguments);
}

} //namespace flang
