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

  WHERE (.false.) ! expected-error {{statement requires an expression of logical array type ('logical' invalid)}}
    R_ARR = -1.0
  END WHERE

  WHERE(I_ARR) ! expected-error {{statement requires an expression of logical array type ('integer' invalid)}}
    I_ARR = 0
  END WHERE

  END WHERE ! expected-error {{use of 'end where' outside a where construct}}
  ELSE WHERE ! expected-error {{use of 'else where' outside a where construct}}

  WHERE(I_ARR == 0) PRINT *, 'Hello world' ! expected-error {{expected an assignment statement}}
  END WHERE ! expected-error {{use of 'end where' outside a where construct}}

  WHERE(I_ARR /= 0)
    I_ARR = 1
    PRINT *, 'Hello world' ! expected-error {{expected an assignment statement in a where construct}}
  ELSE WHERE
    I_ARR = 2
    PRINT *, 'Hello world' ! expected-error {{expected an assignment statement in a where construct}}
  END WHERE

  PRINT *, 'Hello world'

  WHERE(I_ARR == 0)
    WHERE(I_ARR == 0) ! expected-error {{expected an assignment statement in a where construct}}
      I_ARR = 1
    END WHERE
  END WHERE

  WHERE(I_ARR == 0)
    WHERE(I_ARR == 0) I_ARR = 1 ! expected-error {{expected an assignment statement in a where construct}}
  END WHERE

END
