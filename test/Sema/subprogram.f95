! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
SUBROUTINE FOO(X,Y)
  REAL X,Y
END

SUBROUTINE oof()
END SUBROUTINE foo ! expected-error {{expected subroutine name 'oof'}}

SUBROUTINE BAR(ARG, ARG) ! expected-error {{redefinition of 'arg'}}
  REAL ARG
END ! expected-note@-2 {{previous definition is here}}

FUNCTION FUNC(ARG) ! expected-note {{previous definition is here}}
  REAL ARG
  REAL ARG ! expected-error {{redefinition of 'arg'}}
END

FUNCTION FUNC2() ! expected-note {{previous definition is here}}
END

FUNCTION FUNC2() ! expected-error {{redefinition of 'func2'}}
END

SUBROUTINE SUBB ! expected-note {{previous definition is here}}
  INTEGER SUBB ! expected-error {{redefinition of 'subb'}}
  REAL FUNC2
  LOGICAL L
  L = SUBB .EQ. 0 ! expected-error {{invalid use of subroutine 'subb'}}
END

FUNCTION FUNC3()
  INTEGER FUNC3
  INTEGER FUNC3 ! expected-error {{the return type for a function 'func3' was already specified}}
  FUNC3 = 1
END

REAL FUNCTION FUNC4()
  INTEGER FUNC4 ! expected-error {{the return type for a function 'func4' was already specified}}
  FUNC4 = 22 ! CHECK: func4 = real(22)
  FUNC4 = .false. ! expected-error {{assigning to 'real' from incompatible type 'logical'}}
END

FUNCTION FUNC5(ARG, IARG)
  FUNC5 = 1.0 ! CHECK: func5 = 1
  if(FUNC5 .EQ. 1.0) FUNC5 = 2.0

  FUNC5 = ARG ! CHECK: func5 = arg
  FUNC5 = IARG ! CHECK: func5 = real(iarg)
END

FUNCTION IFUNC()
  IFUNC = 22 ! CHECK: ifunc = 22
END

FUNCTION OFUNC(ZZ) ! expected-error {{the function 'ofunc' requires a type specifier}}
  IMPLICIT NONE
  INTEGER I ! expected-error@-2 {{the argument 'zz' requires a type specifier}}
  OFUNC = 22
  I = ZZ
END

FUNCTION FUNC6()
  FUNC6 = .true. ! expected-error {{assigning to 'real' from incompatible type 'logical'}}
END

FUNCTION FUNC7()
  INTEGER FUNC7(10) ! expected-error {{invalid type for a function 'func7'}}
  INTEGER I
  REAL R
  I = IFUNC() + FUNC3() ! CHECK: i = (ifunc()+func3())
  R = FUNC5(1.0, I) * FUNC4() ! CHECK: r = (func5(1, i)*func4())

  R = FUNC5(1.0) ! expected-error {{too few arguments to function call, expected 2, have 1}}
  R = FUNC5(1.0, I, I) ! expected-error {{too many arguments to function call, expected 2, have 3}}
END
