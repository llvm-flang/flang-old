//===-- ParserDecl.cpp - Fortran Declaration Parser -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Fortran declaration parsing.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Sema.h"

namespace flang {

/// ParseTypeDeclarationStmt - Parse a type-declaration-stmt construct.
///
///   [R501]:
///     type-declaration-stmt :=
///         declaration-type-spec [ [ , attr-spec ] ... :: ] entity-decl-list
bool Parser::ParseTypeDeclarationStmt(SmallVectorImpl<DeclResult> &Decls) {
  DeclSpec DS;
  if (ParseDeclarationTypeSpec(DS))
    return true;

  while (ConsumeIfPresent(tok::comma)) {
    // [R502]:
    //   attr-spec :=
    //       access-spec
    //    or ALLOCATABLE
    //    or ASYNCHRONOUS
    //    or CODIMENSION lbracket coarray-spec rbracket
    //    or CONTIGUOUS
    //    or DIMENSION ( array-spec )
    //    or EXTERNAL
    //    or INTENT ( intent-spec )
    //    or INTRINSIC
    //    or language-binding-spec // TODO!
    //    or OPTIONAL
    //    or PARAMETER
    //    or POINTER
    //    or PROTECTED
    //    or SAVE
    //    or TARGET
    //    or VALUE
    //    or VOLATILE
    auto Kind = Tok.getKind();
    auto Loc = Tok.getLocation();
    if(!ExpectAndConsume(tok::identifier)) {
      goto error;
    }
    switch (Kind) {
    default:
      Diag.ReportError(Loc,
                       "unknown attribute specification");
      goto error;
    case tok::kw_ALLOCATABLE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_allocatable);
      break;
    case tok::kw_ASYNCHRONOUS:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_asynchronous);
      break;
    case tok::kw_CODIMENSION:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_codimension);
      // FIXME: Parse the coarray-spec.
      break;
    case tok::kw_CONTIGUOUS:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_contiguous);
      break;
    case tok::kw_DIMENSION:
      if (ParseDimensionAttributeSpec(Loc, DS))
        goto error;
      break;
    case tok::kw_EXTERNAL:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_external);
      break;
    case tok::kw_INTENT:
      // FIXME:
      if(!ExpectAndConsume(tok::l_paren))
        goto error;

      switch (Tok.getKind()) {
      default:
        Diag.ReportError(Tok.getLocation(),
                         "invalid INTENT specifier");
        goto error;
      case tok::kw_IN:
        Actions.ActOnIntentSpec(Loc, DS, DeclSpec::IS_in);
        break;
      case tok::kw_OUT:
        Actions.ActOnIntentSpec(Loc, DS, DeclSpec::IS_out);
        break;
      case tok::kw_INOUT:
        Actions.ActOnIntentSpec(Loc, DS, DeclSpec::IS_inout);
        break;
      }
      Lex();

      if(!ExpectAndConsume(tok::r_paren))
        goto error;

      break;
    case tok::kw_INTRINSIC:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_intrinsic);
      break;
    case tok::kw_OPTIONAL:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_optional);
      break;
    case tok::kw_PARAMETER:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_parameter);
      break;
    case tok::kw_POINTER:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_pointer);
      break;
    case tok::kw_PROTECTED:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_protected);
      break;
    case tok::kw_SAVE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_save);
      break;
    case tok::kw_TARGET:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_target);
      break;
    case tok::kw_VALUE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_value);
      break;
    case tok::kw_VOLATILE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_volatile);
      break;

    // Access Control Specifiers
    case tok::kw_PUBLIC:
      Actions.ActOnAccessSpec(Loc, DS, DeclSpec::AC_public);
      break;
    case tok::kw_PRIVATE:
      Actions.ActOnAccessSpec(Loc, DS, DeclSpec::AC_private);
      break;
    }
  }

  ConsumeIfPresent(tok::coloncolon);

  if (ParseEntityDeclarationList(DS, Decls))
    return true;

  return false;
 error:
  return true;
}

bool Parser::ParseDimensionAttributeSpec(SourceLocation Loc, DeclSpec &DS) {
  SmallVector<ArraySpec*, 8> Dimensions;
  if (ParseArraySpec(Dimensions))
    return true;
  Actions.ActOnDimensionAttrSpec(Context, Loc, DS, Dimensions);
  return false;
}

static bool isAttributeSpec(tok::TokenKind Kind) {
  switch (Kind) {
  default:
    return false;
  case tok::kw_ALLOCATABLE:
  case tok::kw_ASYNCHRONOUS:
  case tok::kw_CODIMENSION:
  case tok::kw_CONTIGUOUS:
  case tok::kw_DIMENSION:
  case tok::kw_EXTERNAL:
  case tok::kw_INTENT:
  case tok::kw_INTRINSIC:
  case tok::kw_OPTIONAL:
  case tok::kw_PARAMETER:
  case tok::kw_POINTER:
  case tok::kw_PROTECTED:
  case tok::kw_SAVE:
  case tok::kw_TARGET:
  case tok::kw_VALUE:
  case tok::kw_VOLATILE:
  case tok::kw_PUBLIC:
  case tok::kw_PRIVATE:
    break;
  }
  return true;
}

bool Parser::ParseEntityDeclarationList(DeclSpec &DS,
                                        SmallVectorImpl<DeclResult> &Decls) {
  do {
    auto IDLoc = Tok.getLocation();
    auto ID = Tok.getIdentifierInfo();
    if(!ExpectAndConsume(tok::identifier))
      return true;

    DeclSpec ObjectDS(DS);
    if(ParseObjectArraySpec(IDLoc, ObjectDS))
      return true;
    if(ParseObjectCharLength(IDLoc, ObjectDS))
      return true;
    Decls.push_back(Actions.ActOnEntityDecl(Context, ObjectDS, IDLoc, ID));

  } while(ConsumeIfPresent(tok::comma));
  return false;
}

bool Parser::ParseObjectArraySpec(SourceLocation Loc, DeclSpec &DS) {
  if(IsPresent(tok::l_paren)) {
    SmallVector<ArraySpec*, 8> Dimensions;
    if (ParseArraySpec(Dimensions))
      return true;
    Actions.ActOnObjectArraySpec(Context, Loc, DS, Dimensions);
  }
  return false;
}

bool Parser::ParseObjectCharLength(SourceLocation Loc, DeclSpec &DS) {
  if(DS.getTypeSpecType() == TST_character && ConsumeIfPresent(tok::star)) {
    if (DS.hasLengthSelector())
      Diag.Report(getExpectedLoc(), diag::err_duplicate_len_selector);
    return ParseCharacterStarLengthSpec(DS);
  }
  return false;
}

/// Parse the optional KIND or LEN selector.
/// 
///   [R405]:
///     kind-selector :=
///         ( [ KIND = ] scalar-int-initialization-expr )
///   [R425]:
///     length-selector :=
///         ( [ LEN = ] type-param-value )
ExprResult Parser::ParseSelector(bool IsKindSel) {
  if (ConsumeIfPresent(IsKindSel ? tok::kw_KIND : tok::kw_LEN)) {
    if (!ExpectAndConsume(tok::equal))
      return ExprError();
    // TODO: We have a "REAL (KIND(10D0)) :: x" situation.
  }

  return ParseExpression();
}

bool Parser::ParseCharacterStarLengthSpec(DeclSpec &DS) {
  ExprResult Len;
  if(ConsumeIfPresent(tok::l_paren)) {
    if(ConsumeIfPresent(tok::star)) {
      DS.setStartLengthSelector();
    } else Len = ParseExpectedFollowupExpression("(");
    if(!ExpectAndConsume(tok::r_paren))
      return true;
  }
  else Len = ParseExpectedFollowupExpression("*");

  if(Len.isInvalid()) return true;
  if(Len.isUsable())  DS.setLengthSelector(Len.take());
  return false;
}

/// ParseDeclarationTypeSpec - Parse a declaration type spec construct.
/// 
///   [R502]:
///     declaration-type-spec :=
///         intrinsic-type-spec
///      or TYPE ( derived-type-spec )
///      or CLASS ( derived-type-spec )
///      or CLASS ( * )
bool Parser::ParseDeclarationTypeSpec(DeclSpec &DS, bool AllowSelectors,
                                      bool AllowOptionalCommaAfterCharLength) {
  // [R403]:
  //   intrinsic-type-spec :=
  //       INTEGER [ kind-selector ]
  //    or REAL [ kind-selector ]
  //    or DOUBLE PRECISION
  //    or COMPLEX [ kind-selector ]
  //    or DOUBLE COMPLEX
  //    or CHARACTER [ char-selector ]
  //    or LOGICAL [ kind-selector ]
  switch (Tok.getKind()) {
  default:
    DS.SetTypeSpecType(DeclSpec::TST_unspecified);
    break;
  case tok::kw_INTEGER:
    DS.SetTypeSpecType(DeclSpec::TST_integer);
    break;
  case tok::kw_REAL:
    DS.SetTypeSpecType(DeclSpec::TST_real);
    break;
  case tok::kw_COMPLEX:
    DS.SetTypeSpecType(DeclSpec::TST_complex);
    break;
  case tok::kw_CHARACTER:
    DS.SetTypeSpecType(DeclSpec::TST_character);
    break;
  case tok::kw_LOGICAL:
    DS.SetTypeSpecType(DeclSpec::TST_logical);
    break;
  case tok::kw_DOUBLEPRECISION:
    DS.SetTypeSpecType(DeclSpec::TST_real);
    DS.setDoublePrecision(); // equivalent to Kind = 8
    break;
  case tok::kw_DOUBLECOMPLEX:
    DS.SetTypeSpecType(DeclSpec::TST_complex);
    DS.setDoublePrecision(); // equivalent to Kind = 8
    break;
  }

  if (DS.getTypeSpecType() == DeclSpec::TST_unspecified)
    if (ParseTypeOrClassDeclTypeSpec(DS))
      return true;

  ExprResult Kind;
  ExprResult Len;

  // FIXME: no Kind for double complex and double precision
  switch (DS.getTypeSpecType()) {
  case DeclSpec::TST_struct:
    break;
  default:
    ConsumeToken();
    if (ConsumeIfPresent(tok::star)) {
      // FIXME: proper obsolete COMPLEX*16 support
      ConsumeAnyToken();
      DS.setDoublePrecision();
    }

    if (!AllowSelectors)
      break;
    if (ConsumeIfPresent(tok::l_paren)) {
      Kind = ParseSelector(true);
      if (Kind.isInvalid())
        return true;

      if(!ExpectAndConsume(tok::r_paren, 0, "", tok::r_paren))
        return true;
    }

    break;
  case DeclSpec::TST_character:
    // [R424]:
    //   char-selector :=
    //       length-selector
    //    or ( LEN = type-param-value , KIND = scalar-int-initialization-expr )
    //    or ( type-param-value , #
    //    #    [ KIND = ] scalar-int-initialization-expr )
    //    or ( KIND = scalar-int-initialization-expr [, LEN = type-param-value])
    //
    // [R425]:
    //   length-selector :=
    //       ( [ LEN = ] type-param-value )
    //    or * char-length [,]
    //
    // [R426]:
    //   char-length :=
    //       ( type-param-value )
    //    or scalar-int-literal-constant
    //
    // [R402]:
    //   type-param-value :=
    //       scalar-int-expr
    //    or *
    //    or :
    ConsumeToken();

    if(ConsumeIfPresent(tok::star)) {
      ParseCharacterStarLengthSpec(DS);
      if(AllowOptionalCommaAfterCharLength)
        ConsumeIfPresent(tok::comma);
    } else {
      if (!AllowSelectors)
        break;
      if(ConsumeIfPresent(tok::l_paren)) {
        if(IsPresent(tok::kw_LEN)) {
          Len = ParseSelector(false);
          if (Len.isInvalid())
            return true;
        } else if(IsPresent(tok::kw_KIND)) {
          Kind = ParseSelector(true);
          if (Kind.isInvalid())
            return true;
        } else {
          Len = ParseExpectedFollowupExpression("(");
          if(Len.isInvalid())
            return true;
        }

        if(ConsumeIfPresent(tok::comma)) {
          // FIXME:
          if (Tok.is(tok::kw_LEN)) {
            if (Len.isInvalid())
              return Diag.ReportError(Tok.getLocation(),
                                      "multiple LEN selectors for this type");
            Len = ParseSelector(false);
            if (Len.isInvalid())
              return true;
          } else if (Tok.is(tok::kw_KIND)) {
            if (Kind.isInvalid())
              return Diag.ReportError(Tok.getLocation(),
                                      "multiple KIND selectors for this type");
            Kind = ParseSelector(true);
            if (Kind.isInvalid())
              return true;
          } else {
            if (Kind.isInvalid())
              return Diag.ReportError(Tok.getLocation(),
                                      "multiple KIND selectors for this type");

            ExprResult KindExpr = ParseExpression();
            Kind = KindExpr;
          }
        }

        if(!ExpectAndConsume(tok::r_paren))
          return true;
      }
    }

    break;
  }

  // Set the selectors for declspec.
  if(Kind.isUsable()) DS.setKindSelector(Kind.get());
  if(Len.isUsable())  DS.setLengthSelector(Len.get());
  return false;
}

/// ParseTypeOrClassDeclTypeSpec - Parse a TYPE(...) or CLASS(...) declaration
/// type spec.
///
///   [R502]:
///     declaration-type-spec :=
///         TYPE ( derived-type-spec )
///      or CLASS ( derived-type-spec )
///      or CLASS ( * )
///
///   [R455]:
///     derived-type-spec :=
///         type-name [ ( type-param-spec-list ) ]
///
///   [R456]:
///     type-param-spec :=
///         [ keyword = ] type-param-value
bool Parser::ParseTypeOrClassDeclTypeSpec(DeclSpec &DS) {
  if(Tok.is(tok::kw_TYPE)) {
    ConsumeToken();
    if(!ExpectAndConsume(tok::l_paren))
      return true;
    auto ID = Tok.getIdentifierInfo();
    auto Loc = Tok.getLocation();
    if(!ExpectAndConsume(tok::identifier))
      return true;
    if(!ExpectAndConsume(tok::r_paren))
      return true;
    Actions.ActOnTypeDeclSpec(Context, Loc, ID, DS);
    return false;
  }

  // FIXME: Handle CLASS.
  return true;
}

// FIXME: Fortran 2008 stuff.
/// ParseDerivedTypeDefinitionStmt - Parse a type or a class definition.
///
/// [R422]:
///   derived-type-def :=
///     derived-type-stmt
///     [ private-sequence-stmt ] ...
///     component-def-stmt
///     [ component-def-stmt ] ...
///     end-type-stmt
///
/// [R423]:
///   derived-type-stmt :=
///     TYPE [ [ , access-spec ] :: ] type-name
///
/// [R424]:
///   private-sequence-stmt :=
///     PRIVATE or SEQUENCE
///
/// [R425]:
///   component-def-stmt :=
///     type-spec [ [ component-attr-spec-list ] :: ] component-decl-list
///
/// [R426]:
///   component-attr-spec-list :=
///     POINTER or
///     DIMENSION( component-array-spec )
///
/// [R427]:
///   component-array-spec :=
///     explicit-shape-spec-list or
///     deffered-shape-spec-list
///
/// [R428]:
///   component-decl :=
///     component-name [ ( component-array-spec ) ]
///     [ * char-length ] [ component-initialization ]
///
/// [R429]:
///   component-initialization :=
///     = initialization-expr or
///     => NULL()
///
/// [R430]:
///   end-type-stmt :=
///     END TYPE [ type-name ]
bool Parser::ParseDerivedTypeDefinitionStmt() {
  bool IsClass = Tok.is(tok::kw_CLASS);
  auto Loc = ConsumeToken();

  if(ConsumeIfPresent(tok::comma)) {
    //FIXME: access-spec
  }
  ConsumeIfPresent(tok::coloncolon);

  auto ID = Tok.getIdentifierInfo();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier)) {
    SkipUntilNextStatement();
    return true;
  } else
    ExpectStatementEnd();

  Actions.ActOnDerivedTypeDecl(Context, Loc, IDLoc, ID);

  // FIXME: private
  if(Tok.is(tok::kw_SEQUENCE)) {
    Actions.ActOnDerivedTypeSequenceStmt(Context, ConsumeToken());
    ExpectStatementEnd();
  }

  bool Done = false;
  while(!Done) {
    switch(Tok.getKind()) {
    case tok::kw_TYPE:
    case tok::kw_CLASS:
    case tok::kw_INTEGER:
    case tok::kw_REAL:
    case tok::kw_COMPLEX:
    case tok::kw_CHARACTER:
    case tok::kw_LOGICAL:
    case tok::kw_DOUBLEPRECISION:
    case tok::kw_DOUBLECOMPLEX:
      if(ParseDerivedTypeComponentStmt())
        SkipUntilNextStatement();
      else ExpectStatementEnd();
      break;
    default:
      Done = true;
      break;
    }
  }

  ParseEndTypeStmt();

  Actions.ActOnEndDerivedTypeDecl(Context);
  return false;
error:
  return true;
}

void Parser::ParseEndTypeStmt() {
  if(Tok.isNot(tok::kw_ENDTYPE)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_kw)
      << "end type";
    Diag.Report(cast<NamedDecl>(Actions.CurContext)->getLocation(), diag::note_matching)
      << "type";
    return;
  }

  auto Loc = ConsumeToken();
  if(IsPresent(tok::identifier)) {
    auto ID = Tok.getIdentifierInfo();
    Actions.ActOnENDTYPE(Context, Loc, ConsumeToken(), ID);
  } else
    Actions.ActOnENDTYPE(Context, Loc, Loc, nullptr);
  ExpectStatementEnd();
}

bool Parser::ParseDerivedTypeComponentStmt() {
  // type-spec
  DeclSpec DS;
  if(ParseDeclarationTypeSpec(DS, true, false))
    return true;

  // component-attr-spec
  if(ConsumeIfPresent(tok::comma)) {
    do {
      auto Kind = Tok.getKind();
      auto Loc = Tok.getLocation();
      auto ID = Tok.getIdentifierInfo();
      if(!ExpectAndConsume(tok::identifier))
        return true;
      if(Kind == tok::kw_POINTER)
        Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_pointer);
      else if(Kind == tok::kw_DIMENSION) {
        if(ParseDimensionAttributeSpec(Loc, DS))
          return true;
      } else {
        if(isAttributeSpec(Kind))
          Diag.Report(Loc, diag::err_use_of_attr_spec_in_type_decl)
            << ID;
        else
          Diag.Report(Loc, diag::err_expected_attr_spec);
        if(!SkipUntil(tok::coloncolon, true, true))
          return true;
        break;
      }
    } while(ConsumeIfPresent(tok::comma));
    if(!ExpectAndConsume(tok::coloncolon))
      return true;
  } else
    ConsumeIfPresent(tok::coloncolon);

  // component-decl-list
  do {
    auto IDLoc = Tok.getLocation();
    auto ID = Tok.getIdentifierInfo();
    if(!ExpectAndConsume(tok::identifier))
      return true;

    DeclSpec ObjectDS(DS);
    if(ParseObjectArraySpec(IDLoc, ObjectDS))
      return true;
    if(ParseObjectCharLength(IDLoc, ObjectDS))
      return true;
    // FIXME: initialization expressions
    Actions.ActOnDerivedTypeFieldDecl(Context, ObjectDS, IDLoc, ID);

  } while(ConsumeIfPresent(tok::comma));
  return false;
}

} //namespace flang
