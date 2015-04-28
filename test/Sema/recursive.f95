! RUN: %flang -fsyntax-only -verify < %s

RECURSIVE SUBROUTINE FOO()
  CALL FOO
END

RECURSIVE FUNCTION FACTORIAL(X)
  INTEGER FACTORIAL, X
  IF(X <= 1) THEN
    FACTORIAL = 1
  ELSE
    FACTORIAL = FACTORIAL(X-1)*X
  END IF
END

SUBROUTINE SUB()
  CALL SUB ! expected-error {{calling a non-recursive subroutine 'sub'}}
END

FUNCTION FUNC(I)
  REAL FUNC, I
  FUNC = FUNC(I) ! expected-error {{calling a non-recursive function 'func'}}
END

FUNCTION FUNC2(I) RESULT(FUNC)
  REAL FUNC, I
  FUNC = FUNC2(I) ! expected-error {{calling a non-recursive function 'func2'}}
END

SUBROUTINE SUB2()
  INTEGER I
  I = SUB2() ! expected-error {{invalid use of subroutine 'sub2'}}
END

! FIXME: ambiguity when a function is returning an array (choose array?)
