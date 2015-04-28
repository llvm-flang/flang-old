//==--- DiagnosticParseKinds.td - libparse diagnostics --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Parser Diagnostics
//===----------------------------------------------------------------------===//

let Component = "Parse" in {

let CategoryName = "Parse Issue" in {

// Generic errors
def err_parse_error : Error<"parse error">;
def err_duplicate_declspec : Error<"duplicate '%0' declaration specifier">;
def err_duplicate_len_selector : Error<
  "duplicate character length declaration specifier">;
def err_unexpected_lparen : Error<"unexpected '('">;
def err_unexpected_percent : Error<"unexpected '%'">;

def err_expected_expression : Error<"expected an expression">;
def err_expected_expression_after : Error<
  "expected an expression after '%0'">;
def err_expected_executable_stmt : Error<
  "expected an executable statement">;
def err_expected_stmt : Error<
  "expected '%0' statement">;
def err_expected_type : Error<"expected a type">;
def err_expected_external_declaration : Error<"expected external declaration">;
def err_extraneous_closing_brace : Error<"extraneous closing brace ('}')">;
def err_expected_ident : Error<"expected identifier">;
def err_expected_ident_after : Error<"expected identifier after '%0'">;
def err_expected_ident_lparen : Error<"expected identifier or '('">;
def err_expected_ident_lbrace : Error<"expected identifier or '{'">;
def err_expected_lbrace : Error<"expected '{'">;
def err_expected_lparen : Error<"expected '('">;
def err_expected_lparenslash : Error<"expected '(/'">;
def err_expected_lparen_after : Error<"expected '(' after '%0'">;
def err_expected_lparen_or_lbrace : Error<"expected '('or '{'">;
def err_expected_rparen : Error<"expected ')'">;
def err_expected_single_rparen_stmt_end : Error<
  "expected ')' before the end of the statement">;
def err_expected_rparen_stmt_end : Error<
  "expected %0 ')'s before the end of the statement">;
def err_expected_slashrparen : Error<"expected '/)'">;
def err_expected_lsquare : Error<"expected '['">;
def err_expected_rsquare : Error<"expected ']'">;
def err_expected_rbrace : Error<"expected '}'">;
def err_expected_colon : Error<"expected ':'">;
def err_expected_coloncolon : Error<"expected '::'">;
def err_expected_equal : Error<"expected '='">;
def err_expected_comma : Error<"expected ','">;
def err_expected_dot : Error<"expected '.'">;
def err_expected_slash : Error<"expected '/'">;
def err_expected_kw : Error<"expected '%0'">;
def err_expected_stmt_label : Error<"expected statement label">;
def err_expected_stmt_label_after : Error<"expected statement label after '%0'">;
def err_expected_semi_declaration : Error<
  "expected ';' at end of declaration">;
def err_expected_semi_decl_list : Error<
  "expected ';' at end of declaration list">;
def ext_expected_semi_decl_list : ExtWarn<
  "expected ';' at end of declaration list">;
def err_expected_member_name_or_semi : Error<
  "expected member name or ';' after declaration specifiers">;
def err_expected_case_or_end_select : Error<
  "expected 'case' or 'end select' statement after 'select case' statement">;
def err_expected_attr_spec : Error<
  "expected attribute specifier">;


def err_expected_end_statement_at_end_of_stmt : Error<
  "expected line break or ';' at end of statement">;


def err_expected_letter : Error<
  "expected a letter">;
def err_expected_int_literal_constant : Error<
  "expected an integer literal constant">;
def err_expected_int_literal_constant_after : Error<
  "expected an integer literal constant after '%0'">;
def err_expected_int_literal_constant_before : Error<
  "expected an integer literal constant before '%0'">;

def err_expected_var : Error<
  "expected a variable">;
def err_expected_do_var : Error<
  "expected a real or an integer variable">;
def err_expected_int_var : Error<
  "expected an integer variable after '%0'">;

def err_expected_after_recursive : Error<
  "expected 'function' or 'subroutine' after 'recursive'">;
def err_expected_fn_body : Error<
  "expected function body after function declarator">;
def err_expected_func_after : Error<
  "expected a function after '%0'">;


def err_format_invalid_desc : Error<
  "'%0' isn't a valid format descriptor">;
def err_format_expected_desc : Error<
  "expected a format descriptor">;
def err_format_desc_with_unparsed_end : Error<
  "invalid ending for a format descriptor">;

// Declarations
def err_invalid_decl_spec_combination : Error<
  "cannot combine with previous '%0' declaration specifier">;

def err_use_of_attr_spec_in_type_decl : Error<
  "use of %0 attribute specifier in a type construct">;


} // end of Parse Issue category.
} // end of Parser diagnostics
