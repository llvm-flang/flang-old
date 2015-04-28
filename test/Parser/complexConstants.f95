! RUN: %flang -fsyntax-only -verify < %s
PROGRAM constants
  COMPLEX C
  PARAMETER (ZERO = 0.0)

  C = (1.0,2.0)
  C = (2.343,1E-4)
  C = (4,3)
  C = (-1,0)
  C = (ZERO, ZERO)

  C = (1, .false.) ! expected-error {{expected an integer or a real constant expression}}
END PROGRAM constants
