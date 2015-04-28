! RUN: %flang -fsyntax-only -verify < %s
PROGRAM paramtest
  PARAMETER (x=0, y = 2.5, c = 'A')
  PARAMETER (NUM = 0.1e4)
  PARAMETER (CM = (0.5,-6e2))

  PARAMETER (Z 1) ! expected-error {{expected '='}}
  PARAMETER (1) ! expected-error {{expected identifier}}

  PARAMETER ! expected-error {{expected '('}}
  PARAMETER (A = 1 B = 2) ! expected-error {{expected ','}}
  PARAMETER (d = 33 ! expected-error {{expected ')'}}
  PARAMETER (chuff = 1)

END PROGRAM paramtest
