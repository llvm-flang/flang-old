! RUN: %flang -fsyntax-only -verify < %s
PROGRAM wheretest
  INTEGER I_ARR(5)
  REAL R_ARR(5)

  I_ARR = 0

  WHERE(I_ARR == 0) I_ARR = I_ARR + 1

  WHERE(I_ARR <= 1)
    R_ARR = I_ARR * 2.0
  ELSE WHERE
    R_ARR = 0.0
  END WHERE

  WHERE(R_ARR >= 10.0)
    R_ARR = I_ARR
  END WHERE

  WHERE R_ARR >= 1.0) ! expected-error {{expected '('}}
    R_ARR = -1.0
  END WHERE

  WHERE ( ! expected-error {{expected an expression}}
    R_ARR = 0.0
  END WHERE

  WHERE (I_ARR < R_ARR ! expected-error {{expected ')'}}
    I_ARR = R_ARR
  END WHERE

END
