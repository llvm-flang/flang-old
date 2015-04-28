! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

INTEGER FUNCTION FOO() RESULT(I)
  I = 1 ! CHECK: i = 1
END

FUNCTION BAR() RESULT(X)
  X = 0.0 ! CHECK: x = 0
END

REAL FUNCTION FUNC() RESULT(I)
  I = 1.0 ! CHECK: i = 1
END

FUNCTION RES() RESULT(RESULT)
  RESULT = 1
  RESULT = RESULT
END

FUNCTION FUNC5() RESULT (X)
  ! CHECK: logical function
  LOGICAL X
  X = .false.
END

FUNCTION FUNC6() RESULT (X) ! expected-note {{previous definition is here}}
  LOGICAL FUNC6 ! expected-error {{redefinition of 'func6'}}
END

FUNCTION FUNC2() RESULT(FUNC2) ! expected-error {{use of name 'func2' for the result variable (function uses the same name)}}
END

FUNCTION FUNC3(RES) RESULT(RES) ! expected-error {{redefinition of 'res'}}
END ! expected-note@-1 {{previous definition is here}}

