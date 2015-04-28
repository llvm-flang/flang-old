//==--- DiagnosticLexKinds.td - Lexer Diagnostics -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Lexer Diagnostics
//===----------------------------------------------------------------------===//

let Component = "Lex", CategoryName = "Lexical Issue" in {

// continuations
def err_continuation_out_of_context : Error<"continuation character used out of context">;

// Literal
def ext_unknown_escape : ExtWarn<"unknown escape sequence '\\%0'">;
def err_unterminated_string : Error<"missing terminating ' character">;

def err_invalid_decimal_digit : Error<"invalid digit '%0' in decimal constant">;
def err_invalid_binary_digit : Error<"invalid digit '%0' in binary constant">;
def err_invalid_octal_digit : Error<"invalid digit '%0' in octal constant">;
def err_invalid_suffix_integer_constant : Error<
  "invalid suffix '%0' on integer constant">;
def err_invalid_suffix_float_constant : Error<
  "invalid suffix '%0' on floating constant">;
def warn_extraneous_char_constant : Warning<
  "extraneous characters in character constant ignored">;
def warn_char_constant_too_large : Warning<
  "character constant too long for its type">;
def err_multichar_utf_character_literal : Error<
  "Unicode character literals may not contain multiple characters">;
def err_exponent_has_no_digits : Error<"exponent has no digits">;
def ext_imaginary_constant : Extension<
  "imaginary constants are a GNU extension">, InGroup<GNU>;
def err_hexconstant_requires_exponent : Error<
  "hexadecimal floating constants require an exponent">;
def err_hexconstant_requires_digits : Error<
  "hexadecimal floating constants require a significand">;
def ext_hexconstant_invalid : Extension<
  "hexadecimal floating constants are a C99 feature">, InGroup<C99>;
def ext_binary_literal : Extension<
  "binary integer literals are a GNU extension">, InGroup<GNU>;

def err_pascal_string_too_long : Error<"Pascal string is too long">;
def warn_octal_escape_too_large : ExtWarn<"octal escape sequence out of range">;
def warn_hex_escape_too_large : ExtWarn<"hex escape sequence out of range">;
def ext_string_too_long : Extension<"string literal of length %0 exceeds "
  "maximum length %1 that %select{C90|ISO C99|C++}2 compilers are required to "
  "support">, InGroup<OverlengthStrings>;
def err_character_too_large : Error<
  "character too large for enclosing character literal type">;
def err_unsupported_string_concat : Error<
  "unsupported non-standard concatenation of string literals">;
def err_string_concat_mixed_suffix : Error<
  "differing user-defined suffixes ('%0' and '%1') in string literal "
  "concatenation">;
def err_pp_invalid_udl : Error<
  "%select{character|integer}0 literal with user-defined suffix "
  "cannot be used in preprocessor constant expression">;
def err_bad_string_encoding : Error<
  "illegal character encoding in string literal">;
def warn_bad_string_encoding : ExtWarn<
  "illegal character encoding in string literal">,
  InGroup<InvalidSourceEncoding>;
def err_bad_character_encoding : Error<
  "illegal character encoding in character literal">;
def warn_bad_character_encoding : ExtWarn<
  "illegal character encoding in character literal">,
  InGroup<InvalidSourceEncoding>;
def err_lexing_string : Error<"failure when lexing a string">;

// defined operators
def err_defined_operator_too_long : Error<"defined operator is too long">;
def err_defined_operator_missing_end : Error<"defined operator missing end '.'">;

// Include
def err_pp_file_not_found : Error<"'%0' file not found">, DefaultFatal;
def err_pp_error_opening_file : Error<
  "error opening file '%0': %1">, DefaultFatal;
def err_pp_empty_filename : Error<"empty filename">;
def err_pp_include_too_deep : Error<"INCLUDE nested too deeply">;
def err_pp_expects_filename : Error<"expected 'FILENAME'">;

}
