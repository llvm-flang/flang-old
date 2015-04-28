//==--- DiagnosticSemaKinds.td - libsema diagnostics ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Semantic Analysis
//===----------------------------------------------------------------------===//

let Component = "Sema" in {
let CategoryName = "Semantic Issue" in {

def err_typecheck_expected_integer_expr : Error<
  "expected an expression of integer type (%0 invalid)">;
def err_expected_integer_gt_0 : Error<
  "expected an integer greater than 0">;
def err_expected_positive_integer : Error<
  "expected a positive integer">;
def err_expected_integer_or_real_constant_expr : Error<
  "expected an integer or a real constant expression">;
def err_expected_integer_constant_expr : Error<
  "expected an integer constant expression">;
def err_expected_scalar_or_character_expr : Error<
  "expected an integer, real, complex or character expression">;
def err_expected_scalar_or_character_constant_expr : Error<
  "expected an integer, real, complex or character constant expression">;
def err_expected_scalar_numeric_expr : Error<
  "expected a scalar expression (%0 invalid)">;
def err_expected_numeric_or_logical_expr : Error<
  "expected an expression of integer, real, complex or logical type (%0 invalid)">;
def err_expected_var_ref : Error<
  "expected a variable reference">;

def err_expr_not_assignable : Error<
  "expression is not assignable">;
def err_typecheck_assign_incompatible : Error<
  "assigning to %0 from incompatible type %1">;
def err_typecheck_initialization_incompatible : Error<
  "initializing %0 with an expression of incompatible type %1">;
def err_typecheck_return_incompatible : Error<
  "returning %0 from a function with incompatible result type %1">;
def err_typecheck_passing_incompatible : Error<
  "passing %0 to parameter of incompatible type %1">;
def err_typecheck_passing_incompatible_named_arg : Error<
  "passing %0 to parameter '%1' of incompatible type %2">;
def err_typecheck_passing_incompatible_named_args : Error<
  "passing %0 to parameter '%1' of incompatible type %2 (or parameter '%3' of type %4)">;
def note_typecheck_passing_argument_to_param_here : Note<
  "passing argument to parameter %0 here">;
def err_typecheck_arg_conflict_type : Error<
  "conflicting types in arguments '%0' and '%1' (%2 and %3)">;
def note_typecheck_initializing_member_here : Note<
  "initializing member %0 declared here">;

def err_typecheck_arith_invalid_operands : Error<
  "invalid operands to an arithmetic binary expression (%0 and %1)">;
def warn_f77_typecheck_arith_invalid_operands : Warning<
  "invalid operands to an arithmetic binary expression according to the Fortran 77 standard (%0 and %1)">;
def err_typecheck_char_invalid_operands : Error<
  "invalid operands to a character binary expression (%0 and %1)">;
def err_typecheck_relational_invalid_operands : Error<
  "invalid operands to a relational binary expression (%0 and %1)">;
def warn_f77_typecheck_relational_invalid_operands : Warning<
  "invalid operands to a relational binary expression according to the Fortran 77 standard (%0 and %1)">;
def err_typecheck_logical_invalid_operands : Error<
  "invalid operands to a logical binary expression (%0 and %1)">;

def err_typecheck_arith_unary_expr : Error<
  "invalid operand to an arithmetic unary expression (%0)">;
def err_typecheck_logical_unary_expr : Error<
  "invalid operand to a logical unary expression (%0)">;

def err_typecheck_stmt_requires_int_expr : Error<
  "statement requires an expression of integer type (%0 invalid)">;
def err_typecheck_stmt_requires_int_var : Error<
  "statement requires an integer variable (%0 invalid)">;
def err_typecheck_expected_logical_expr : Error<
  "expected an expression of logical type (%0 invalid)">;
def err_typecheck_stmt_requires_logical_expr : Error<
  "statement requires an expression of logical type (%0 invalid)">;
def err_typecheck_stmt_requires_logical_array_expr : Error<
  "statement requires an expression of logical array type (%0 invalid)">;
def err_typecheck_stmt_requires_int_logical_char_expr : Error<
  "statement requires an expression of integer, logical or character type (%0 invalid)">;
def err_typecheck_expected_char_expr : Error<
  "expected an expression of character type (%0 invalid)">;
def err_typecheck_expected_default_kind_expr : Error<
  "expected an expression with default type kind (%0 invalid)">;
def err_typecheck_expected_expr_of_type : Error<
  "expected an expression of %0 type (%1 invalid)">;
def err_typecheck_expected_int_var : Error<
  "expected an integer variable instead of a variable with type %0">;
def err_typecheck_expected_format_spec : Error<
  "expected a valid format specifier instead of an expression with type %0">;

def err_typecheck_expected_array_of_dim_count : Error<
  "conflicting shapes in an array expression (%0 %plural{1:dimension|:dimensions}0"
  " and %1 %plural{1:dimension|:dimensions}1)">;
def err_typecheck_array_dim_shape_mismatch : Error<
  "conflicting size for dimension %0 in an array expression (%1 and %2)">;

def err_typecheck_arg_conflict_array_dim_count : Error<
  "passing array with %1 %plural{1:dimension|:dimensions}1 to argument '%2'"
  "of incompatible shape with %0 %plural{1:dimension|:dimensions}0">;
def err_typecheck_arg_conflict_array_dim_size: Error<
  "passing array with dimension %0 of size %1 to argument '%3' of incompatible size %2">;

def err_typecheck_args_conflict_array_dim_count : Error<
  "conflicting shapes in arguments '%2' and '%3' (%0 %plural{1:dimension|:dimensions}0"
  " and %1 %plural{1:dimension|:dimensions}1)">;
def err_typecheck_args_conflict_array_dim_size: Error<
  "conflicting size for dimension %0 in arguments '%3' and '%4' (%1 and %2)">;

def err_typecheck_use_of_implied_shape_array : Error<
  "use of an array expression with an implied dimension specification">;

def err_var_not_assignable : Error<
  "assignment to a do variable %0">;
def note_var_prev_do_use : Note<
  "which is used in a do statement here">;

def err_char_star_length_incompatible : Error<
  "use of character length declarator '*' for a "
  "%select{local variable|parameter}0 %1">;

def err_array_implied_shape_must_be_last : Error<
  "the dimension declarator '*' must be used only in the last dimension">;
def err_array_implied_shape_incompatible : Error<
  "use of dimension declarator '*' for a local variable %0">;
def err_array_explicit_arg_shape_incompatible : Error<
  "use of argument dimension for a local variable %0">;
def err_array_explicit_shape_requires_int_arg : Error<
  "array specifier requires an integer argument (%0 invalid)">;
def err_array_subscript_dimension_count_mismatch : Error<
  "array subscript must have %0 subscript %plural{1:expression|:expressions}0">;


def warn_array_index_precedes_bounds : Warning<
  "array index %0 is before the beginning of the array">,
  InGroup<ArrayBounds>;
def warn_array_index_exceeds_bounds : Warning<
  "array index %0 is past the end of the array (which contains %1 "
  "element%s2)">, InGroup<ArrayBounds>;
def note_array_index_out_of_bounds : Note<
  "array %0 declared here">;

def err_stmt_label_must_decl_after : Error<
  "the statement label '%0' must be declared after the '%1' statement">;
def err_redefinition_of_stmt_label : Error<"redefinition of statement label '%0'">;
def err_undeclared_stmt_label_use : Error<"use of undeclared statement label '%0'">;
def warn_unused_stmt_label : Warning<"unused statement label '%0'">,
  InGroup<UnusedLabel>, DefaultIgnore;

def err_undeclared_common_block_use : Error<"use of undeclared common block %0">;

def err_redefinition_of_construct_name : Error<"redefinition of named construct %0">;
def err_use_of_invalid_construct_name : Error<"use of construct name for an unnamed construct">;
def err_expected_construct_name : Error<"expected construct name %0">;
def err_expected_subprogram_name : Error<"expected %select{program|function|subroutine}1 name %0">;
def err_expected_type_name : Error<"expected %select{type|class}1 name %0">;

def err_same_result_name : Error<
  "use of name %0 for the result variable (function uses the same name)">;


def err_expected_stmt_label_end_do : Error<
  "expected a do termination statement with a statement label '%0'">;

def err_invalid_do_terminating_stmt : Error<
  "invalid terminating statement for a DO loop">;

def err_stmt_not_in_if : Error<
  "use of '%0' outside an if construct">;
def err_end_do_without_do : Error<
  "use of 'end do' outside a do construct">;
def err_stmt_not_in_loop : Error<
  "'%0' statement not in loop statement">;
def err_stmt_not_in_named_loop : Error<
  "'%0' statement not in loop statement named %1">;
def err_stmt_not_in_select_case : Error<
  "use of '%0' outside a select case construct">;
def err_stmt_not_in_where : Error<
  "use of '%0' outside a where construct">;
def err_invalid_stmt_in_where : Error<
  "expected an assignment statement">;
def err_invalid_stmt_in_where_construct : Error<
  "expected an assignment statement in a where construct">;
def err_use_of_logical_range : Error<
  "use of logical range in a case statement">;


def err_use_implicit_none_stmt : Error<
  "use of 'IMPLICIT NONE' after 'IMPLICIT'">;
def err_use_implicit_stmt_after_none : Error<
  "use of 'IMPLICIT' after 'IMPLICIT NONE'">;
def err_redefinition_of_implicit_stmt_rule : Error<
  "redefinition of implicit rule %0">;
def err_redefinition_of_implicit_stmt_rule_range : Error<
  "redefinition of implicit rule in the range %0 - %1">;
def err_implicit_invalid_range : Error<
  "the range %0 - %1 isn't alphabetically ordered">;

def err_parameter_requires_const_init : Error<
  "parameter %0 must be initialized by a constant expression">;
def note_parameter_value_invalid_expr : Note<
  "this expression is not allowed in a constant expression">;
def err_expected_constant_expr : Error<
  "expected a constant expression">;
def err_stmt_requires_consant_expr : Error<
  "statement requires a constant expression">;

def err_intrinsic_invalid_func : Error<
  "invalid function name %0 in an intrinsic statement">;

def err_implied_do_expect_leaf_expr : Error<
  "expected an integer constant or an implied do variable expression">;
def err_implied_do_expect_expr : Error<
  "expected an implied do or an array element expression">;

def err_expect_stmt_name : Error<
  "expected name %0 for '%1' statement">;

def err_redefinition : Error<"redefinition of %0">;
def err_undeclared_var_use : Error<"use of undeclared identifier %0">;

def err_typecheck_call_too_few_args : Error<
  "too few arguments to "
  "%select{intrinsic function call|function call|subroutine call|type constructor}0, "
  "expected %1, have %2">;
def err_typecheck_call_too_few_args_one : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subroutine}0 call, "
  "single argument %1 was not specified">;
def err_typecheck_call_too_few_args_at_least : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subroutine}0 call, "
  "expected at least %1, have %2">;
def err_typecheck_call_too_few_args_at_least_one : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subroutine}0 call, "
  "at least argument %1 must be specified">;
def err_typecheck_call_too_many_args : Error<
  "too many arguments to "
  "%select{intrinsic function call|function call|subroutine call|type constructor}0, "
  "expected %1, have %2">;
def err_typecheck_call_too_many_args_one : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subroutine}0 call, "
  "expected single argument %1, have %2 arguments">;
def err_typecheck_call_too_many_args_at_most : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subroutine}0 call, "
  "expected at most %1, have %2">;
def err_typecheck_call_too_many_args_at_most_one : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subroutine}0 call, "
  "expected at most single argument %1, have %2 arguments">;

def err_call_requires_subroutine: Error<
  "statement requires a subroutine reference ("
  "%select{variable|intrinsic function|function}0 %1 invalid)">;

def err_call_non_recursive : Error<
  "calling a non-recursive %select{function|subroutine}0 %1">;

def err_invalid_subroutine_use : Error<
  "invalid use of subroutine %0">;

def err_spec_not_applicable_undeclared_ident : Error<
  "'%0' statement can't be applied because the identifier %1 "
  "isn't declared in the current context">;
def err_spec_not_applicable_not_var : Error<
  "'%0' statement can't be applied because %1 "
  "isn't a variable">;
def err_spec_requires_local_var : Error<
  "specification statement requires a local variable">;
def err_spec_requires_local_var_arg : Error<
  "specification statement requires a local variable or an argument">;
def err_spec_requires_var_or_arr_el : Error<
  "specification statement requires a variable or an array element expression">;
def err_spec_dimension_already_array : Error<
  "the specification statement 'dimension' cannot be applied to the array variable %0">;
def err_spec_qual_reapplication : Error<
 "the specification statement '%0' cannot be applied to the variable %1 more than once">;
def err_spec_not_applicable_to_common_block : Error<
  "the specification statement '%0' cannot be applied to a variable in common block">;

def warn_equivalence_same_object : Warning<
  "this equivalence connection uses the same object">;
def warn_equivalence_redundant : Warning<
  "redundant equivalence connection">;
def note_equivalence_identical_association : Note<
  "an identical association was already created here">;
def err_equivalence_conflicting_offsets : Error<
  "conflicting memory offsets in an equivalence connection">;
def note_equivalence_prev_offset : Note<
  "previous memory offset was defined here">;

def err_format_without_stmt_label : Error<
  "'FORMAT' statement is missing a statement label">;

def err_fmt_spec_stmt_label_not_format : Error<
  "use of statement label format specifier which doesn't label a 'FORMAT' statement">;

def err_func_return_type_already_specified : Error<
  "the return type for a function %0 was already specified">;
def err_stmt_not_in_func : Error<
  "'%0' statement should be used inside a function or a subroutine">;
def err_func_no_implicit_type : Error<
  "the function %0 requires a type specifier">;
def err_arg_no_implicit_type : Error<
  "the argument %0 requires a type specifier">;
def err_func_invalid_type : Error<
  "invalid type for a function %0">;

def err_invalid_kind_selector : Error<
  "invalid kind selector '%0' for type %1">;

def err_data_stmt_too_many_values : Error<
  "excess values in a 'data' statement">;
def err_data_stmt_not_enough_values : Error<
  "not enough values in a 'data' statement">;
def err_data_stmt_invalid_item : Error<
  "invalid item for a 'data' statement">;
def err_data_stmt_invalid_var : Error<
  "%select{parameter constant|function argument|function result variable}0 "
  "can't be initialized by a 'data' statement">;

def err_multiple_default_case_stmt : Error<
  "multiple default cases in one select case construct">;

def warn_deprecated_computed_goto_stmt : Warning<"computed goto statement is deprecated">,
    InGroup<Deprecated>;
def warn_deprecated_arith_if_stmt : Warning<"arithmetic if statement is deprecated">,
    InGroup<Deprecated>;

def err_duplicate_attr_spec : Error<
  "duplicate '%0' attribute specifier">;
def err_duplicate_intent_spec : Error<
  "duplicate '%0' attribute specifier">;
def err_duplicate_access_spec : Error<
  "duplicate '%0' access specifier">;

def err_duplicate_member : Error<
  "duplicate member %0">;
def err_invalid_type_field_array_shape : Error<
  "invalid shape for an array type field">;
def err_use_of_not_typename : Error<
  "invalid type name %0">;
def err_record_member_not_sequence : Error<
  "member %0 requires a type with a 'sequence' attribute (%1 invalid)">;
def err_no_member : Error<
  "no member named %0 in %1">;

}

}
