! RUN: %flang -fsyntax-only -verify < %s
PROGRAM test
  INTEGER ARR(10)
  X(I) = I+2
  Y(A) = CMPLX(A) ** (1.0,-4.0)
  J() = 1.0
  II(J) = ARR(J)
  ZZ() = 1 2 3 ! expected-error {{expected line break or ';' at end of statement}}
  ZZK(I = 2 ! expected-error {{expected ')'}}
  ARR(1) = 1

END PROGRAM test
