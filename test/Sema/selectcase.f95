! RUN: %flang -fsyntax-only -verify < %s

PROGRAM selecttest
  PARAMETER (KK = 22)

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
  CASE (2:6,7,KK)
    I = 3
  CASE DEFAULT
    I = 0
  END SELECT

  SELECT CASE(.true.)
  CASE (.false.)
    I = 0
  CASE DEFAULT
    CONTINUE
  END SELECT

  SELECT CASE('Hello')
  CASE ('World', '!')
    I = -1
  CASE ('Hell')
    I = 1
  CASE DEFAULT
    I = 0
  END SELECT

  SELECT CASE((1.0, 2.0)) ! expected-error {{statement requires an expression of integer, logical or character type ('complex' invalid)}}
  END SELECT

  SELECT CASE(1.0) ! expected-error {{statement requires an expression of integer, logical or character type ('real' invalid)}}
  END SELECT

  SELECT CASE(1)
  CASE DEFAULT ! expected-note {{previous case defined here}}
    I = 0
  CASE DEFAULT ! expected-error {{multiple default cases in one select case construct}}
    I = 1
  END SELECT

  SELECT CASE(I)
  CASE (1, 2.0) ! expected-error {{expected an expression of integer type ('real' invalid)}}
    I = 0
  CASE (1:.false.) ! expected-error {{expected an expression of integer type ('logical' invalid)}}
    I = 1
  END SELECT

  SELECT CASE(.true.)
  CASE (.true.:.false.) ! expected-error {{use of logical range in a case statement}}
    I = 0
  CASE (.true.:) ! expected-error {{use of logical range in a case statement}}
  END SELECT

  SELECT CASE(.true.)
  CASE ((1.0,2.0)) ! expected-error {{expected an expression of logical type ('complex' invalid)}}
  END SELECT

  SELECT CASE('foo')
  CASE ('bar',42) ! expected-error {{expected an expression of character type ('integer' invalid)}}
  END SELECT

  CASE (1) ! expected-error {{use of 'case' outside a select case construct}}
  CASE DEFAULT ! expected-error {{use of 'case default' outside a select case construct}}
  END SELECT ! expected-error {{use of 'end select' outside a select case construct}}

END
