! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

PROGRAM dimtest
  IMPLICIT NONE

  PARAMETER(SCALAR = 1.0) ! expected-note {{'scalar' is a parameter constant defined here}}

  ! FIXME: note as above?
  REAL TheArray(10, 20)

  DIMENSION X(1,2,3,4,5)
  INTEGER X

  INTEGER Y, Z
  DIMENSION Y(20), Z(10)

  DIMENSION ARR(10) ! expected-error {{use of undeclared identifier 'arr'}}
  DIMENSION SCALAR(20) ! expected-error {{specification statement requires a local variable or an argument}}

  DIMENSION TheArray(10, 20) ! expected-error {{the specification statement 'dimension' cannot be applied to the array variable 'thearray'}}

  REAL A

  DIMENSION A(10), FOO(5:100) ! expected-error {{use of undeclared identifier 'foo'}}

ENDPROGRAM

subroutine sub1
  dimension i(10)
  i(1) = 2.0 ! CHECK: i(1) = int(2)
end

subroutine sub2
  dimension i(10)
  complex i
  i(1) = 1 ! CHECK: i(1) = cmplx(1)
end

subroutine sub3
  real i
  dimension i(10)
  i(1) = 1 ! CHECK: i(1) = real(1)
end
