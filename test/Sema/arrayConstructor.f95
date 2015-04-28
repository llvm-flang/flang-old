! RUN: %flang -fsyntax-only -verify < %s
REAL FUNCTION foo()
  foo = 1.0
END
PROGRAM arrtest
  INTEGER I_ARR(5),I_MAT(2,2),I_ARR2(3)
  REAL R_ARR(7)
  CHARACTER(Len = 10) STR_ARR(2)
  INTEGER I

  I = 0
  I_ARR = (/ 1, 2, 3, -11, I /)
  I_ARR = (/ 1,2, (/ 3,4,5 /) /)
  I_ARR = (/ (/ (/ 0, 1 /), 2 /), 3, (/ 4 /) /)
  R_ARR = (/ 1.0, 2.0, REAL(11), REAL(I), 2.0 * 4.0 + 6.0, 1.0, foo() /)
  STR_ARR = (/ 'Hello', 'World' /)
  I_ARR = (/ I_MAT, 22 /)
  I_ARR = (/ 1, I_ARR2, I /)

  I_ARR = (/ I_ARR, I_MAT /) ! expected-error {{conflicting size for dimension 1 in an array expression (5 and 9)}}

  I_ARR = (/ 1, 2, 3.0, 11, 5/) ! expected-error {{expected an expression of 'integer' type ('real' invalid)}}
  STR_ARR = (/ 'Hello', 2 /) ! expected-error {{expected an expression of 'character' type ('integer' invalid)}}

ENDPROGRAM arrtest
