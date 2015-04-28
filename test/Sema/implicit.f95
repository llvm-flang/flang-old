! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM imptest
  IMPLICIT INTEGER (A, B)
  IMPLICIT REAL (C-E, F)
  IMPLICIT REAL (A) ! expected-error {{redefinition of implicit rule 'a'}}
  IMPLICIT INTEGER (F-G) ! expected-error {{redefinition of implicit rule in the range 'f' - 'g'}}
  IMPLICIT INTEGER (L, P, B) ! expected-error {{redefinition of implicit rule 'b'}}
  IMPLICIT INTEGER (Z-X) ! expected-error {{the range 'z' - 'x' isn't alphabetically ordered}}

  A = 1 ! CHECK: a = 1
  B = 2.0 ! CHECK: b = int(2)

  C = A ! CHECK: c = real(a)
  D = C ! CHECK: d = c
  E = -1.0 ! CHECK: e = (-1)
  F = C ! CHECK: f = c

  I = 0 ! CHECK: i = 0
END PROGRAM imptest
