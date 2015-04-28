! RUN: %flang -fsyntax-only -verify < %s

SUBROUTINE FOO(ISIZEX,X)
  REAL X(ISIZEX,*)
END

SUBROUTINE VAR(A,X)
  REAL X(A,*) ! expected-error {{array specifier requires an integer argument ('real' invalid)}}
END

SUBROUTINE BAR(A,X)
  IMPLICIT NONE
  REAL X(A,*) ! expected-error {{the argument 'a' requires a type specifier}}
END
