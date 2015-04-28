! RUN: %flang -fsyntax-only -verify < %s
PROGRAM arrtest
  INTEGER I_ARR(5)
  REAL R_ARR(6)
  INTEGER I

  I = 0
  I_ARR = (/ 1, 2, 3, -11, I /)
  R_ARR = (/ 1.0, 2.0, REAL(11), REAL(I), 2.0 * 4.0 + 6.0, 11.0 /)

  I_ARR = (/ 1, 2, 4, 5, 6 ! expected-error {{expected '/)'}}

ENDPROGRAM arrtest
