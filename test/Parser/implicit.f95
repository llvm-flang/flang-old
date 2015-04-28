! RUN: %flang -fsyntax-only -verify < %s
PROGRAM imptest
  IMPLICIT INTEGER(A)
  IMPLICIT REAL(B, G), COMPLEX(H)
  IMPLICIT INTEGER(C-D)
  IMPLICIT REAL(M - N)

  IMPLICIT REAL (0) ! expected-error {{expected a letter}}
  IMPLICIT REAL (X-'Y') ! expected-error {{expected a letter}}
  IMPLICIT REAL X ! expected-error {{expected '('}}
  IMPLICIT REAL (X ! expected-error {{expected ')'}}

  IMPLICIT REAL(Z) INTEGER(V) ! expected-error {{expected ','}}

  A = 33
  B = 44.9
  C = A
  D = C
  M = B
  N = M

END PROGRAM imptest
