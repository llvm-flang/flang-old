! RUN: %flang -fsyntax-only -verify < %s

PROGRAM selecttest
  INTEGER I
  SELECT CASE(1)
  END SELECT

  I = 0
  a: SELECT CASE(I)
  CASE DEFAULT a
  END SELECT a

  SELECT CASE(I)
  CASE (1,2)
    I = 1
  CASE (3:)
    I = 42
  END SELECT

  SELECT CASE(I)
  CASE (:1)
    I = -1
  CASE (2:6,7)
    I = 3
  CASE DEFAULT
    I = 0
  END SELECT

  SELECT CASE(I ! expected-error {{expected ')'}}
  CASE () ! expected-error {{expected an expression}}
  END SELECT

  SELECT CASE(1)
    I = 0 ! expected-error {{expected 'case' or 'end select' statement after 'select case' statement}}
    I = 1
  END SELECT

  SELECT CASE(1)
    SELECT CASE(2) ! expected-error {{expected 'case' or 'end select' statement after 'select case' statement}}
    END SELECT
    I = 1
  END SELECT

END
