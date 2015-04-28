! RUN: %flang -fsyntax-only -verify < %s

SUBROUTINE SUB(I,J)
END

REAL FUNCTION FOO()
  Foo = 1
END

PROGRAM test
  EXTERNAL FUNC
  INTEGER NOFUNC

  CALL SUB(1,2)
  CALL SUB ! expected-error {{too few arguments to subroutine call, expected 2, have 0}}
  CALL SUB(1) ! expected-error {{too few arguments to subroutine call, expected 2, have 1}}
  CALL SUB(1,2,3,4) ! expected-error {{too many arguments to subroutine call, expected 2, have 4}}
  CALL FUNC
  CALL IMPFUNC(1,2)

  CALL NOFUNC ! expected-error {{statement requires a subroutine reference (variable 'nofunc' invalid)}}
  CALL FOO ! expected-error {{statement requires a subroutine reference (function 'foo' invalid)}}
END
