! RUN: %flang -fsyntax-only -verify < %s
PROGRAM datatest
  INTEGER I, J, K
  REAL X,Y,Z, A, B
  INTEGER I_ARR(10)

  DATA I / 1 /
  DATA J, K / 2*42 /

  DATA X Y / 1, 2 / ! expected-error {{expected '/'}}

  DATA X, Y / 1 2 / ! expected-error {{expected '/'}}

  DATA A/1/,B/2/

  DATA ! expected-error {{expected an expression}}

  ! FIXME: sema
  !DATA (I_ARR(I), I = 1,10) / 10*0 /

END PROGRAM
