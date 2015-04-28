! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM test
  X(I) = I+2 ! CHECK: real((i+2))
  Y(A) = 1.0 ! CHECK: 1

  COMPLEX CC
  CC(M) = (0.0,1.0)

  CHARACTER*(10) ABC
  ABC(I) = 'ABC'

  COMPLEX ZP
  BAR(ZP) = AIMAG(ZP)

  INTEGER FOO ! expected-note@+1 {{previous definition is here}}
  FOO(I,I) = 1 ! expected-error {{redefinition of 'i'}}

  Z(A) = 'Hello' ! expected-error {{returning 'character' from a function with incompatible result type 'real'}}

  I = X(2) ! CHECK: i = int(x(2))
  I = X() ! expected-error {{too few arguments to function call, expected 1, have 0}}

  A = Y(2.0) ! CHECK: a = y(2)

  print *, ABC(1)

  CALL X(3) ! expected-error {{statement requires a subroutine reference (function 'x' invalid)}}

END PROGRAM test
