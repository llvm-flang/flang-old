! RUN: %flang -fsyntax-only -verify < %s
PROGRAM paramtest
  PARAMETER (x=0, y = 2.5, c = 'A') ! expected-note {{previous definition is here}}
  REAL :: z
  PARAMETER (z = x / 2.0, w = z .EQ. 0) ! expected-note {{previous definition is here}}
  COMPLEX comp
  PARAMETER (comp = 0)

  INTEGER NUMBER
  PARAMETER (NUMBER = .false.) ! expected-error {{initializing 'integer' with an expression of incompatible type 'logical'}}

  REAL w ! expected-error {{redefinition of 'w'}}

  PARAMETER (exprs = 1 + 2 * 7) ! expected-note@+2 {{this expression is not allowed in a constant expression}}

  PARAMETER (fail = C(1:1)) ! expected-error {{parameter 'fail' must be initialized by a constant expression}}

  INTEGER VAR ! expected-note@+1 {{this expression is not allowed in a constant expression}}
  PARAMETER (epicFail = VAR + 1) ! expected-error {{parameter 'epicfail' must be initialized by a constant expression}}

  PARAMETER (x = 22) ! expected-error {{redefinition of 'x'}}

END PROGRAM paramtest
